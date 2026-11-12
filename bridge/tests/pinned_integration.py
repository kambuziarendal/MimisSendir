"""Run in pinned image, no network/mounts with user state. Tests real adapter methods."""
import asyncio
import logging
import sys
from types import SimpleNamespace as NS
import unittest

from slidgnal.session import Session
from slidgnal.generated import signal
from slidge.group.room import LegacyMUC
from slidgnal.group import Bookmarks

PATCHED = hasattr(Session, '_dispatch_event')
if PATCHED:
    from slidgnal.event_dispatch import EventDispatch

class ReachedParticipant(Exception): pass

class Tests(unittest.IsolatedAsyncioTestCase):
    @unittest.skipUnless(hasattr(Session, 'signal_call'), 'candidate native-call interface')
    async def test_typing_with_read_receipts_and_message_burst(self):
        import inspect
        import threading
        from slidgnal.mixins import RecipientMixin
        from slidgnal.native_calls import NativeCalls
        from slixmpp.exceptions import XMPPError
        queued='max_waiters' in inspect.signature(NativeCalls).parameters
        release=threading.Event();entered=threading.Event();seen=[]
        def typing(value):
            seen.append('typing');entered.set();release.wait(2)
        def receipt(value):seen.append(int(value.Timestamps[0]))
        lane=NativeCalls(timeout=1,**({'max_waiters':64} if queued else {}))
        session=NS(_action_calls=lane,_metadata_calls=NativeCalls(),
                   signal=NS(SendTyping=typing,SendReceipt=receipt))
        session.signal_call=lambda *args:Session.signal_call(session,*args)
        peer=NS(session=session,legacy_id='00000000-0000-4000-8000-000000000001')
        try:
            first=asyncio.create_task(RecipientMixin.on_chat_state(peer,'composing',None))
            while not entered.is_set():await asyncio.sleep(.001)
            pending=[asyncio.create_task(RecipientMixin.on_displayed(peer,
                peer.legacy_id+'|'+str(1700000000000+n),None)) for n in range(4)]
            pending.append(asyncio.create_task(session.signal_call('SendMessage',lambda:seen.append('message'))))
            await asyncio.sleep(.02)
            self.assertEqual(seen,['typing'])
            release.set();await first
            results=await asyncio.gather(*pending,return_exceptions=True)
            if queued:
                self.assertEqual(results,[None]*5)
                self.assertEqual(seen,['typing',*[1700000000000+n for n in range(4)],'message'])
            else:
                self.assertTrue(all(isinstance(e,XMPPError) and e.condition=='resource-constraint' for e in results))
                print('DEPLOYED BASELINE REPRODUCED: typing rejects four reads and a message')
        finally:
            release.set();lane.close();session._metadata_calls.close()

    async def test_native_serial_callback_before_connect(self):
        """Group before connect reproduces the startup dependency cycle."""
        loop = asyncio.get_running_loop()
        ready = loop.create_future()
        seen = []
        class Contact:
            @property
            def name(self):
                raise ReachedParticipant()
        room = NS(session=NS(contacts=NS(ready=ready)), stored=NS(id=None))
        async def group(data):
            try:
                await LegacyMUC.get_participant_by_contact(room, Contact())
            except ReachedParticipant:
                seen.append('group')
        async def connect(data):
            seen.append('connect')
            ready.set_result(True)
        obj = NS(handle_group=group, handle_connect=connect)
        if PATCHED:
            obj._dispatch_event = lambda event, data: Session._dispatch_event(obj, event, data)
            obj._data_events = EventDispatch(obj._dispatch_event, timeout=.5)
        payload = signal.EventPayload()
        async def serial_native_calls():
            await Session.handle_event(obj, signal.EventGroup, payload.handle)
            await Session.handle_event(obj, signal.EventConnect, payload.handle)
        task = asyncio.create_task(serial_native_calls())
        done, _ = await asyncio.wait({task}, timeout=.1)
        if PATCHED:
            self.assertIn(task, done)
            await task
            await obj._data_events.queue.join()
            self.assertEqual(seen, ['connect', 'group'])
            await obj._data_events.close()
        else:
            self.assertNotIn(task, done)
            self.assertEqual(seen, [])
            print('BASELINE REPRODUCED: group waits for contacts; connect cannot run')
            task.cancel()
            await asyncio.gather(task, return_exceptions=True)

    async def test_group_failure_isolation(self):
        from slixmpp.exceptions import XMPPError
        seen=[]
        async def update(address, data):
            if address == 'unavailable':
                raise XMPPError('forbidden', 'synthetic inaccessible group')
            seen.append(address)
        session=NS(signal=NS(ListGroups=lambda: [NS(Address='unavailable'), NS(Address='healthy')]),
                   log=logging.getLogger('test'), send_gateway_message=lambda text: None)
        async def signal_call(operation, function, *args): return function(*args)
        session.signal_call = signal_call
        bookmarks=NS(session=session, by_legacy_id=update)
        if PATCHED:
            await Bookmarks.fill(bookmarks)
            self.assertEqual(seen, ['healthy'])
            self.assertEqual(bookmarks.failed_groups, 1)
        else:
            with self.assertRaises(XMPPError):
                await Bookmarks.fill(bookmarks)
            self.assertEqual(seen, [])
            print('BASELINE REPRODUCED: one group failure aborts remaining sync')

    @unittest.skipUnless(hasattr(Session, 'signal_call'), 'candidate native-call interface')
    async def test_blocked_metadata_does_not_block_send_or_grow_threads(self):
        import threading
        from slidgnal.native_calls import NativeCalls
        from slixmpp.exceptions import XMPPError
        release=threading.Event()
        session=NS(_metadata_calls=NativeCalls(timeout=.03), _action_calls=NativeCalls(timeout=.03))
        def blocked(): release.wait(1)
        try:
            job=asyncio.create_task(Session.signal_call(session, 'GetGroup', blocked))
            await asyncio.sleep(.005)
            self.assertEqual(await Session.signal_call(session, 'SendMessage', lambda:'synthetic-id'), 'synthetic-id')
            with self.assertRaises(XMPPError) as error:await job
            self.assertEqual(error.exception.condition, 'remote-server-timeout')
            self.assertTrue(session._metadata_calls.health()['pending'])
            with self.assertRaises(XMPPError) as busy:
                await Session.signal_call(session, 'GetGroup', blocked)
            self.assertEqual(busy.exception.condition, 'resource-constraint')
        finally:
            release.set()
            if session._metadata_calls.pending:
                await asyncio.shield(session._metadata_calls.pending)
            session._metadata_calls.close();session._action_calls.close()

    @unittest.skipUnless(hasattr(Session, 'signal_call'), 'candidate native-call interface')
    async def test_failed_group_listing_returns_explicit_partial_availability(self):
        from slixmpp.exceptions import XMPPError
        status=[]
        async def unavailable(*args):raise XMPPError('remote-server-timeout')
        obj=NS(session=NS(signal=NS(ListGroups=lambda:None),signal_call=unavailable,
                          log=logging.getLogger('test'),send_gateway_message=status.append))
        await Bookmarks.fill(obj)
        self.assertEqual(obj.failed_groups, -1)
        self.assertIn('unconfirmed', status[0])

    async def test_bookmark_titles_and_renames(self):
        from slixmpp import ClientXMPP, JID
        from slixmpp.plugins.xep_0060.stanza import Item
        client = ClientXMPP('fixture@example.test', 'unused-offline')
        client.register_plugin('xep_0402')
        published=[]
        async def publish(iq): published.append(iq)
        async def participant(): return NS(jid=JID('room@example.test/self'))
        old=Item()
        old['conference']['nick']='self'
        old['conference']['name']='Old title'
        old['conference']['autojoin']=False
        async def existing(): return old
        room=NS(jid=JID('room@example.test'), name='New title',
                user_jid=JID('fixture@example.test'),
                get_user_participant=participant,
                _LegacyMUC__get_bookmark=existing,
                session=NS(user=NS(preferences={'always_invite_when_adding_bookmarks':False})),
                log=logging.getLogger('test'),
                xmpp=NS(plugin={'xep_0402':client.plugin['xep_0402'],
                                'xep_0356':NS(send_privileged_iq=publish)}))
        await LegacyMUC.add_to_bookmarks(room)
        if PATCHED:
            self.assertEqual(len(published), 1)
            conference=published[0]['pubsub']['publish']['items'][0]['conference']
            self.assertEqual(conference['name'], 'New title')
            self.assertFalse(conference['autojoin'])
            published.clear()
            room.name=''
            await LegacyMUC.add_to_bookmarks(room)
            self.assertEqual(published, [])
            room.name='Old title'
            await LegacyMUC.add_to_bookmarks(room)
            self.assertEqual(published, [])
        else:
            self.assertEqual(published, [])
            print('BASELINE REPRODUCED: title change alone never updates bookmark')

    async def test_cancelled_group_does_not_cancel_shared_contacts_readiness(self):
        ready=asyncio.get_running_loop().create_future()
        room=NS(session=NS(contacts=NS(ready=ready)))
        task=asyncio.create_task(LegacyMUC.get_participant_by_contact(room, None))
        await asyncio.sleep(0)
        task.cancel()
        await asyncio.gather(task, return_exceptions=True)
        self.assertEqual(ready.cancelled(), not PATCHED)

if __name__ == '__main__':
    print('Pinned adapter mode:', 'PATCHED' if PATCHED else 'BASELINE')
    unittest.main()
