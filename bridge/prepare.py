#!/usr/bin/env python3
"""Stage pinned Python patches offline. Refuses drift and existing output.

Never points at a running container, a database, or a production installation.
"""
import argparse
import ast
import hashlib
from pathlib import Path
import shutil

EXPECTED = {
    'slidgnal/mixins.py': '4efa9b2270c01169432e5cd1cedbe72611eb2d3f204b79a8029467c39fcff7b0',
    'slidgnal/contact.py': '68349d7b0c06bab4e45cddfd1aaa73859dae2a0e65997725a35678ac60e4b9ad',
    'slidgnal/session.py': 'c102242a63ee5f14e5f31337c1fff97419b049b8da3372393adb4d5887e0caf0',
    'slidgnal/group.py': '4e93ef38fd8cb0e77c2613731682a11c179d2d6511b7d2aa657a60c634c7eca8',
    'slidge/group/room.py': 'de9631bc81211ce7ed1a8f65bf2f232decf56efde3d9cc0ebbdc1430991b4ff5',
}

def replace(text, old, new):
    assert text.count(old) == 1, f'Patch context count mismatch: {old[:60]}'
    return text.replace(old, new)


def offload_async_calls(text):
    # Rewrite only native I/O calls inside async methods, preserving surrounding
    # source/comments. Go value constructors/property access stay on the loop.
    tree = ast.parse(text)
    offsets = [0]
    for line in text.splitlines(keepends=True):
        offsets.append(offsets[-1] + len(line))
    edits = {}
    for function in ast.walk(tree):
        if not isinstance(function, ast.AsyncFunctionDef): continue
        for node in ast.walk(function):
            if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Attribute): continue
            if ast.unparse(node.func.value) != 'self.session.signal': continue
            start = offsets[node.lineno-1] + node.col_offset
            end = offsets[node.end_lineno-1] + node.end_col_offset
            method = ast.get_source_segment(text, node.func)
            original = text[start:end]
            args = original[len(method)+1:-1]
            edits[start] = (end, 'await self.session.signal_call(' + repr(node.func.attr)
                            + ', ' + method + (',' if args.strip() else '') + args + ')')
    for start, (end, new) in sorted(edits.items(), reverse=True):
        text = text[:start] + new + text[end:]
    ast.parse(text)
    return text


def stage(source, dest):
    assert not dest.exists(), 'Refusing existing destination'
    files = {}
    for name, digest in EXPECTED.items():
        data = (source / name).read_bytes()
        assert hashlib.sha256(data).hexdigest() == digest, f'Pinned source mismatch: {name}'
        files[name] = data.decode()
    s = files['slidgnal/session.py']
    s = replace(s, 'from .group import MUC, Bookmarks, Participant',
                'from .group import MUC, Bookmarks, Participant\nfrom .event_dispatch import EventDispatch\nfrom .native_calls import NativeCalls, NativeCallBusy, NativeCallTimeout\nfrom .bounded_map import BoundedMap')
    s = replace(s, 'self.__handle_event = make_sync(self.handle_event, self.xmpp.loop)',
                'self._metadata_calls = NativeCalls()\n        self._action_calls = NativeCalls(max_waiters=64)\n        self._data_events = EventDispatch(self._dispatch_event, prerequisite=self._wait_data_ready)\n        self.__handle_event = make_sync(self.handle_event, self.xmpp.loop)')
    s = replace(s, '        self.signal.Disconnect()\n',
                '        await self.signal_call("Disconnect", self.signal.Disconnect)\n        await self._data_events.close()\n')
    s = replace(s, '        self.__reset_connected()\n        self.signal.Login()',
                '        self.__reset_connected()\n        if self._data_events.closed:\n            self._data_events = EventDispatch(self._dispatch_event, prerequisite=self._wait_data_ready)\n        self.signal.Login()')
    s = replace(s, '        match event:\n', '''        # The native gateway has ONE callback thread. Never wait there for
        # data handling that depends on a later EventConnect callback.
        # EventPayload owns a Go reference before this callback returns.
        if event in (signal.EventLogin, signal.EventConnect, signal.EventLogout,
                     signal.EventArchiveSync):
            await self._dispatch_event(event, data)
        else:
            self._data_events.put(event, data)

    async def _dispatch_event(self, event, data) -> None:
        match event:
''')
    s = replace(s, 'self.__message_ids: dict[str, str] = {}',
                'self.__message_ids = BoundedMap()')
    s = replace(s, 'self.__message_timestamps: dict[float, str] = {}',
                'self.__message_timestamps = BoundedMap()')
    s = replace(s, 'self.__message_reactions: dict[str, str] = {}',
                'self.__message_reactions = BoundedMap()')
    s = replace(s, '        self.signal.Login()\n',
                '        await self.signal_call("Login", self.signal.Login)\n')
    s = replace(s, '    async def logout(self) -> None:', '''    def diagnostic_state(self):
        return {
            "signal_ready": self.ready.done() and not self.ready.cancelled(),
            "contacts_ready": self.contacts.ready.done() and not self.contacts.ready.cancelled(),
            "groups_ready": self.bookmarks.ready.done() and not self.bookmarks.ready.cancelled(),
            "group_failures": getattr(self.bookmarks, "failed_groups", None),
            "events": self._data_events.health(),
            "metadata": self._metadata_calls.health(),
            "actions": self._action_calls.health(),
            "message_refs": len(self.__message_ids),
            "timestamp_refs": len(self.__message_timestamps),
            "reaction_refs": len(self.__message_reactions),
        }

    async def _wait_data_ready(self):
        await asyncio.shield(self.ready)
        await asyncio.shield(self.contacts.ready)

    async def signal_call(self, operation, function, *args):
        lane = self._metadata_calls if operation.startswith(("Get", "List")) else self._action_calls
        try:
            return await lane.run(operation, function, *args)
        except NativeCallBusy:
            raise XMPPError("resource-constraint", "Signal operation lane unavailable or queue limit reached; operation not started") from None
        except NativeCallTimeout:
            raise XMPPError("remote-server-timeout", "Signal operation timed out; outcome unknown. Do not automatically resend.") from None

    async def logout(self) -> None:''')
    s = replace(s, '        self.signal.Logout()\n',
                '        await self.signal_call("Logout", self.signal.Logout)\n')
    s = replace(s, '        return await self.__connected',
                '        return await asyncio.wait_for(asyncio.shield(self.__connected), 120)')
    s = replace(s, 'hasattr(self, "__connected")', 'hasattr(self, "_Session__connected")')
    files['slidgnal/session.py'] = s
    s = files['slidgnal/group.py']
    s = replace(s, 'from typing import TYPE_CHECKING', 'import asyncio\nfrom typing import TYPE_CHECKING')
    s = replace(s, '''        for data in self.session.signal.ListGroups():
            await self.by_legacy_id(str(data.Address), data)''', '''        self.failed_groups = 0
        for data in self.session.signal.ListGroups():
            try:
                await asyncio.wait_for(self.by_legacy_id(str(data.Address), data), 15)
            except (XMPPError, TimeoutError) as exc:
                self.failed_groups += 1
                self.session.log.warning("Group sync failed: category=%s", type(exc).__name__)
        if self.failed_groups:
            self.session.send_gateway_message(
                f"Group sync incomplete: {self.failed_groups} group(s) unavailable. "
                "Other conversations remain available.")''')
    s = replace(s, '            data = self.get_signal_group()',
                '            data = await self.session.signal_call("GetGroup", self.get_signal_group)')
    s = offload_async_calls(s)
    # Bookmark participant persistence replaces the room's ORM object. Apply
    # avatar changes afterwards so that replacement cannot discard them.
    s = replace(s, '''        if data.Avatar.Data:
            await self.set_avatar(Avatar(data=bytes(data.Avatar.Data)))
        elif data.Avatar.Delete:
            await self.set_avatar(None)
        self._set_expire_timer(data.ExpireTimer)
        await self.add_to_bookmarks()''', '''        self._set_expire_timer(data.ExpireTimer)
        await self.add_to_bookmarks()
        # Bookmark updates can replace self.stored via participant persistence.
        # Apply the avatar to that final room object before updating_info commits.
        if data.Avatar.Data:
            await self.set_avatar(Avatar(data=bytes(data.Avatar.Data)))
        elif data.Avatar.Delete:
            await self.set_avatar(None)''')
    s = replace(s, "        for data in await self.session.signal_call('ListGroups', self.session.signal.ListGroups):", '''        try:
            groups = await self.session.signal_call('ListGroups', self.session.signal.ListGroups)
        except XMPPError as exc:
            self.failed_groups = -1  # listing failed; number unavailable is unknown
            self.session.log.warning("Group listing failed: category=%s", exc.condition)
            self.session.send_gateway_message("Group listing unavailable. Direct conversations remain available; group readiness is unconfirmed.")
            return
        for data in groups:''')
    files['slidgnal/group.py'] = s
    s = files['slidgnal/contact.py']
    s = replace(s, '            data = self.get_signal_contact()',
                '            data = await self.session.signal_call("GetContact", self.get_signal_contact)')
    files['slidgnal/contact.py'] = offload_async_calls(s)
    files['slidgnal/mixins.py'] = offload_async_calls(files['slidgnal/mixins.py'])
    s = files['slidge/group/room.py']
    s = 'import asyncio\n' + s
    s = replace(s, '        await self.session.contacts.ready',
                '        await asyncio.shield(self.session.contacts.ready)')
    s = replace(s, '        new["conference"]["nick"] = user_resource',
                '''        new["conference"]["nick"] = user_resource
        # Do not erase a previous known title when partial Signal metadata
        # lacks a title. Include renames in change detection below.
        new["conference"]["name"] = self.name or (
            existing["conference"]["name"] if existing is not None else "")''')
    s = replace(s, '            change = existing["conference"]["nick"] != user_resource',
                '''            change = (existing["conference"]["nick"] != user_resource
                      or existing["conference"]["name"] != new["conference"]["name"])''')
    files['slidge/group/room.py'] = s
    dest.mkdir(parents=True)
    for name, text in files.items():
        target = dest / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
    shutil.copyfile(Path(__file__).with_name('event_dispatch.py'), dest / 'slidgnal/event_dispatch.py')
    shutil.copyfile(Path(__file__).with_name('native_calls.py'), dest / 'slidgnal/native_calls.py')
    shutil.copyfile(Path(__file__).with_name('bounded_map.py'), dest / 'slidgnal/bounded_map.py')
    return files

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    files = stage(args.source, args.destination)
    for name in sorted(files):
        print(hashlib.sha256((args.destination / name).read_bytes()).hexdigest(), name)
