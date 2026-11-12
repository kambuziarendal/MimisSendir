import asyncio
import importlib.util
from pathlib import Path
import threading
import unittest

spec=importlib.util.spec_from_file_location('native_calls', Path(__file__).parents[1]/'native_calls.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class NativeCallsTests(unittest.IsolatedAsyncioTestCase):
    async def test_queue_wait_uses_original_execution_deadline(self):
        release=threading.Event();entered=threading.Event();calls=[]
        lane=m.NativeCalls(max_waiters=4,timeout=.15)
        def blocked():entered.set();release.wait(2)
        def second():calls.append(1);release.wait(.12)
        try:
            first=asyncio.create_task(lane.run('SendTyping',blocked))
            while not entered.is_set():await asyncio.sleep(.001)
            # Cancel the first Python wait without cancelling its native call.
            # Queued work must be rejected, never started over the native call.
            waiting=asyncio.create_task(lane.run('SendMessage',second))
            while not lane.health()['waiting']:await asyncio.sleep(.001)
            first.cancel();await asyncio.gather(first,return_exceptions=True)
            with self.assertRaises(m.NativeCallBusy):await waiting
            self.assertEqual(calls,[])
            release.set();await asyncio.shield(lane.pending)
            # A queued call has only the time remaining from its arrival.
            import time
            lane.timeout=.12
            first=asyncio.create_task(lane.run('SendTyping',time.sleep,.07))
            await asyncio.sleep(.005)
            before=time.monotonic()
            with self.assertRaises(m.NativeCallTimeout):await lane.run('SendMessage',time.sleep,.09)
            self.assertLess(time.monotonic()-before,.17)
            await first
            await asyncio.shield(lane.pending)
        finally:release.set();lane.close()

    async def test_action_burst_is_serial_and_never_replayed(self):
        release=threading.Event();entered=threading.Event();calls=[]
        lane=m.NativeCalls(max_waiters=4,timeout=1)
        def typing():
            calls.append('typing');entered.set();release.wait(2)
        def receipt(number):calls.append(number);return number
        try:
            first=asyncio.create_task(lane.run('SendTyping',typing))
            while not entered.is_set():await asyncio.sleep(.001)
            rest=[asyncio.create_task(lane.run('SendReceipt',receipt,n)) for n in range(4)]
            while lane.health()['waiting']<4:await asyncio.sleep(.001)
            with self.assertRaises(m.NativeCallBusy):await lane.run('SendMessage',receipt,'overflow')
            self.assertEqual(calls,['typing'])
            release.set();await first
            self.assertEqual(await asyncio.gather(*rest),list(range(4)))
            self.assertEqual(calls,['typing',0,1,2,3])
            self.assertEqual(lane.health()['waiting'],0)
        finally:release.set();lane.close()

    async def test_cancelled_queued_action_never_starts(self):
        release=threading.Event();entered=threading.Event();calls=[]
        lane=m.NativeCalls(max_waiters=4,timeout=1)
        def blocked():entered.set();release.wait(2)
        try:
            first=asyncio.create_task(lane.run('SendTyping',blocked))
            while not entered.is_set():await asyncio.sleep(.001)
            cancelled=asyncio.create_task(lane.run('SendMessage',lambda:calls.append('cancelled')))
            while not lane.health()['waiting']:await asyncio.sleep(.001)
            cancelled.cancel();await asyncio.gather(cancelled,return_exceptions=True)
            release.set();await first
            await lane.run('SendMessage',lambda:calls.append('healthy'))
            self.assertEqual(calls,['healthy'])
        finally:release.set();lane.close()

    async def test_timed_out_native_action_keeps_queued_actions_unstarted(self):
        release=threading.Event();entered=threading.Event();calls=[]
        lane=m.NativeCalls(max_waiters=4,timeout=.04)
        def blocked():entered.set();release.wait(2)
        try:
            first=asyncio.create_task(lane.run('SendMessage',blocked))
            while not entered.is_set():await asyncio.sleep(.001)
            second=asyncio.create_task(lane.run('SendReceipt',lambda:calls.append('receipt')))
            with self.assertRaises(m.NativeCallTimeout):await first
            with self.assertRaises(m.NativeCallBusy):await second
            self.assertEqual(calls,[])
            self.assertTrue(lane.health()['pending'])
            release.set();await asyncio.shield(lane.pending)
        finally:release.set();lane.close()

    async def test_closing_lane_rejects_waiting_actions(self):
        release=threading.Event();entered=threading.Event();calls=[]
        lane=m.NativeCalls(max_waiters=4,timeout=1)
        def blocked():entered.set();release.wait(2)
        try:
            first=asyncio.create_task(lane.run('SendTyping',blocked))
            while not entered.is_set():await asyncio.sleep(.001)
            second=asyncio.create_task(lane.run('SendMessage',lambda:calls.append('send')))
            while not lane.health()['waiting']:await asyncio.sleep(.001)
            lane.close();release.set();await first
            with self.assertRaises(m.NativeCallBusy):await second
            self.assertEqual(calls,[])
        finally:release.set();lane.close()

    async def test_stall_keeps_loop_responsive_and_lane_bounded(self):
        release=threading.Event()
        entered=threading.Event()
        def blocked():
            entered.set()
            release.wait(2)
            return 'late result'
        lane=m.NativeCalls(timeout=.03)
        try:
            job=asyncio.create_task(lane.run('synthetic', blocked))
            ticks=0
            while not job.done():
                await asyncio.sleep(.003)
                ticks+=1
            with self.assertRaises(m.NativeCallTimeout):await job
            self.assertTrue(entered.is_set())
            self.assertGreater(ticks, 2)
            for _ in range(100):
                with self.assertRaises(m.NativeCallBusy):await lane.run('retry', blocked)
            self.assertTrue(lane.health()['pending'])
            release.set()
            await asyncio.wait_for(asyncio.shield(lane.pending), 1)
            self.assertEqual(await lane.run('healthy', lambda: 42), 42)
        finally:
            release.set();lane.close()

    async def test_cancellation_does_not_release_running_native_slot(self):
        release=threading.Event()
        lane=m.NativeCalls(timeout=1)
        try:
            job=asyncio.create_task(lane.run('synthetic', release.wait, 2))
            await asyncio.sleep(.01)
            job.cancel()
            await asyncio.gather(job, return_exceptions=True)
            self.assertTrue(lane.health()['pending'])
            with self.assertRaises(m.NativeCallBusy):await lane.run('retry', lambda: None)
            release.set()
            await asyncio.wait_for(asyncio.shield(lane.pending), 1)
        finally:
            release.set();lane.close()

    async def test_closed_lane_does_not_start_work(self):
        lane=m.NativeCalls();lane.close()
        with self.assertRaises(m.NativeCallBusy):await lane.run('closed', lambda: None)

    async def test_readonly_transient_retries_but_send_never_replays(self):
        calls=[]
        def intermittent():
            calls.append(1)
            if len(calls) < 3:raise ConnectionError('synthetic connection reset')
            return 42
        lane=m.NativeCalls(retry_base=.001)
        try:
            self.assertEqual(await lane.run('GetGroup', intermittent), 42)
            self.assertEqual(len(calls),3)
            calls.clear()
            with self.assertRaises(ConnectionError):await lane.run('SendMessage',intermittent)
            self.assertEqual(len(calls),1)
        finally:lane.close()

    async def test_forbidden_does_not_retry_and_rate_limit_cools_down(self):
        calls=[]
        def forbidden():calls.append(1);raise RuntimeError('HTTP 403')
        def limited():calls.append(1);raise RuntimeError('response status: 429')
        lane=m.NativeCalls(retry_base=.001)
        try:
            with self.assertRaises(RuntimeError):await lane.run('GetGroup', forbidden)
            self.assertEqual(len(calls),1)
            with self.assertRaises(RuntimeError):await lane.run('GetGroup', limited)
            self.assertTrue(lane.health()['rate_limited'])
            with self.assertRaises(m.NativeCallBusy):await lane.run('GetGroup', limited)
            self.assertEqual(len(calls),2)
        finally:lane.close()
