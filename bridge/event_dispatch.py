"""Bounded, ordered data events without blocking the Go control callback.

Imported by the pinned Slidgnal adapter. No Signal I/O belongs in this module.
Payload objects are owned Python wrappers before the native callback returns.
"""
import asyncio
import logging
import time


class EventDispatch:
    def __init__(self, handler, *, limit=256, timeout=30.0, prerequisite=None):
        self.handler = handler
        self.prerequisite = prerequisite
        self.queue = asyncio.Queue(maxsize=limit)
        self.timeout = timeout
        self.task = None
        self.closed = False
        self.failed = False
        self.completed = 0
        self.errors = 0
        self.last_success = None
        self.last_duration = None
        self.log = logging.getLogger("slidgnal.event_dispatch")

    def put(self, event, payload):
        if self.closed or self.failed:
            raise RuntimeError("Signal event processing unavailable")
        try:
            self.queue.put_nowait((event, payload))
        except asyncio.QueueFull:
            # Fail visibly rather than silently dropping an event or blocking
            # the one native callback thread (which also carries connect).
            self.failed = True
            self.errors += 1
            self.log.error("Signal event queue capacity exceeded; recovery required")
            raise RuntimeError("Signal event queue capacity exceeded") from None
        if self.task is None or self.task.done():
            self.task = asyncio.create_task(self._drain(), name="signal-data-events")

    async def _drain(self):
        # Startup may legitimately exceed the per-event processing timeout.
        # Do not discard historical messages merely because roster sync is slow.
        if self.prerequisite is not None:
            await self.prerequisite()
        while not self.queue.empty() and not self.closed:
            event, payload = self.queue.get_nowait()
            started = time.monotonic()
            try:
                await asyncio.wait_for(self.handler(event, payload), self.timeout)
            except asyncio.CancelledError:
                raise
            except Exception as exc:
                self.errors += 1
                # Category only: exception strings can contain identities,
                # payload text, attachment URLs or upstream response bodies.
                self.log.warning("Signal event failed: category=%s", type(exc).__name__)
            else:
                self.completed += 1
                self.last_success = time.monotonic()
            finally:
                self.last_duration = time.monotonic() - started
                self.queue.task_done()
                del payload

    async def close(self):
        self.closed = True
        if self.task is not None and not self.task.done():
            self.task.cancel()
            await asyncio.gather(self.task, return_exceptions=True)
        while not self.queue.empty():
            self.queue.get_nowait()
            self.queue.task_done()

    def health(self):
        return dict(queue=self.queue.qsize(), completed=self.completed,
                    errors=self.errors, failed=self.failed, closed=self.closed,
                    last_success=self.last_success, last_duration=self.last_duration)
