"""Bound native calls and retry only read-only, known transient failures.

A timeout cannot cancel Go. Its slot remains occupied until completion.
Messages/actions are never replayed. Only Get/List operations may retry.
"""
import asyncio
from concurrent.futures import ThreadPoolExecutor
import logging
import random
import re
import time

class NativeCallBusy(RuntimeError): pass
class NativeCallTimeout(TimeoutError): pass


def failure_category(exc):
    text = str(exc).lower()
    if re.search(r'(?:http\s+|status(?:\s+code)?[\s:=]+)403\b', text): return 'forbidden'
    if re.search(r'(?:http\s+|status(?:\s+code)?[\s:=]+)429\b', text): return 'rate_limited'
    if isinstance(exc, ConnectionError) or any(term in text for term in
        ('connection reset', 'connection refused', 'temporary failure', 'i/o timeout')):
        return 'transient'
    return 'other'


class NativeCalls:
    def __init__(self, *, timeout=20.0, retry_base=.2, max_waiters=0):
        self.timeout = timeout
        self.retry_base = retry_base
        self.executor = ThreadPoolExecutor(max_workers=1, thread_name_prefix='signal-native')
        self.pending = None
        self.closed = False
        self.log = logging.getLogger('slidgnal.native_calls')
        self.last_duration = None
        self.timeouts = 0
        self.retries = 0
        self.cooldown_until = 0
        self.max_waiters = max_waiters
        self._gate = asyncio.Lock()
        self._waiting = 0

    async def run(self, operation, function, *args):
        if not self.max_waiters:
            return await self._run(operation, function, *args)
        # Admission is not a retry: each accepted action reaches native code at
        # most once. Bound both queue size and total queue/execution time.
        if self.closed or time.monotonic() < self.cooldown_until:
            raise NativeCallBusy('Native operation lane is unavailable; not started')
        if self._waiting >= self.max_waiters:
            raise NativeCallBusy('Native operation queue is full; not started')
        started = time.monotonic()
        self._waiting += 1
        try:
            try:
                await asyncio.wait_for(self._gate.acquire(), self.timeout)
            except TimeoutError:
                raise NativeCallBusy('Native operation queue wait expired; not started') from None
        finally:
            self._waiting -= 1
        try:
            remaining = self.timeout - (time.monotonic() - started)
            if remaining <= 0:
                raise NativeCallBusy('Native operation queue wait expired; not started')
            return await self._run(operation, function, *args, budget=remaining)
        finally:
            self._gate.release()

    async def _run(self, operation, function, *args, budget=None):
        if self.closed:
            raise NativeCallBusy('Native operation lane is closed')
        if time.monotonic() < self.cooldown_until:
            raise NativeCallBusy('Native service rate limit cooldown active')
        if self.pending is not None and not self.pending.done():
            raise NativeCallBusy('Previous native operation has not finished')
        # Reserve the lane over retry delays too; no caller can overtake it.
        if getattr(self, '_reserved', False):
            raise NativeCallBusy('Previous native operation has not finished')
        self._reserved = True
        started = time.monotonic()
        attempts = 3 if operation.startswith(('Get', 'List')) else 1
        try:
            for attempt in range(attempts):
                remaining = (self.timeout if budget is None else budget) - (time.monotonic() - started)
                if remaining <= 0: raise NativeCallTimeout('Native operation budget exhausted')
                self.pending = asyncio.get_running_loop().run_in_executor(self.executor, function, *args)
                self.pending.add_done_callback(lambda future: future.exception() if not future.cancelled() else None)
                try:
                    return await asyncio.wait_for(asyncio.shield(self.pending), remaining)
                except TimeoutError:
                    self.timeouts += 1
                    raise NativeCallTimeout('Native operation timed out; outcome is unknown') from None
                except Exception as exc:
                    category = failure_category(exc)
                    if category == 'rate_limited':
                        # The native adapter does not expose Retry-After. Use a
                        # conservative cooldown, never an immediate retry loop.
                        self.cooldown_until = time.monotonic() + 60
                    if category != 'transient' or attempt + 1 == attempts: raise
                    self.retries += 1
                    delay = self.retry_base * (2 ** attempt) * random.uniform(.8, 1.2)
                    await asyncio.sleep(min(delay, max(0, self.timeout - (time.monotonic()-started))))
        finally:
            self._reserved = False
            self.last_duration = time.monotonic() - started
            self.log.info('Native operation=%s elapsed_ms=%d pending=%s',
                          operation, self.last_duration * 1000,
                          self.pending is not None and not self.pending.done())

    def close(self):
        self.closed = True
        self.executor.shutdown(wait=False, cancel_futures=True)

    def health(self):
        return dict(pending=self.pending is not None and not self.pending.done(),
                    waiting=self._waiting,
                    timeouts=self.timeouts, retries=self.retries,
                    rate_limited=time.monotonic()<self.cooldown_until,
                    closed=self.closed, last_duration=self.last_duration)
