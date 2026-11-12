"""Synthetic events only. Standard library runner, no accounts or network."""
import asyncio
import importlib.util
import pathlib
import unittest

spec = importlib.util.spec_from_file_location("event_dispatch", pathlib.Path(__file__).parents[1] / "event_dispatch.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
EventDispatch = module.EventDispatch


class DispatchTests(unittest.IsolatedAsyncioTestCase):
    async def test_group_wait_does_not_block_connect_callback(self):
        ready = asyncio.get_running_loop().create_future()
        seen = []
        async def data(event, payload):
            await asyncio.shield(ready)
            seen.append(payload)
        dispatch = EventDispatch(data)
        dispatch.put("group", "first")
        await asyncio.sleep(0)
        # Same native callback thread can now deliver the control event.
        ready.set_result(True)
        dispatch.put("message", "second")
        await asyncio.wait_for(dispatch.queue.join(), .5)
        self.assertEqual(seen, ["first", "second"])
        await dispatch.close()

    async def test_slow_startup_does_not_discard_queued_data(self):
        ready=asyncio.Event()
        seen=[]
        async def data(event, payload): seen.append(payload)
        dispatch=EventDispatch(data, timeout=.01, prerequisite=ready.wait)
        dispatch.put('message', 'kept through startup')
        await asyncio.sleep(.04)
        self.assertEqual(dispatch.completed, 0)
        self.assertEqual(dispatch.errors, 0)
        self.assertEqual(dispatch.queue.qsize(), 1)
        ready.set()
        await asyncio.wait_for(dispatch.queue.join(), .1)
        self.assertEqual(seen, ['kept through startup'])
        await dispatch.close()

    async def test_timeout_does_not_poison_readiness_or_following_groups(self):
        ready = asyncio.get_running_loop().create_future()
        seen = []
        async def data(event, payload):
            if event == "stalled":
                await asyncio.shield(ready)
            seen.append(payload)
        dispatch = EventDispatch(data, timeout=.02)
        dispatch.put("stalled", "unavailable")
        dispatch.put("healthy", "available")
        await asyncio.wait_for(dispatch.queue.join(), .5)
        self.assertFalse(ready.cancelled())
        self.assertEqual(seen, ["available"])
        self.assertEqual(dispatch.health()["errors"], 1)
        await dispatch.close()

    async def test_bounded_queue_fails_explicitly_and_controls_remain_free(self):
        async def data(*args):
            pass
        dispatch = EventDispatch(data, limit=2)
        dispatch.put(1, object())
        dispatch.put(2, object())
        with self.assertRaisesRegex(RuntimeError, "capacity"):
            dispatch.put(3, object())
        self.assertEqual(dispatch.queue.qsize(), 2)
        self.assertTrue(dispatch.health()["failed"])
        await dispatch.close()
        self.assertEqual(dispatch.queue.qsize(), 0)

    async def test_error_category_contains_no_payload(self):
        async def data(*args):
            raise ValueError("PRIVATE_MESSAGE_AND_TOKEN")
        dispatch = EventDispatch(data)
        with self.assertLogs("slidgnal.event_dispatch", level="WARNING") as logs:
            dispatch.put(1, object())
            await dispatch.queue.join()
        self.assertNotIn("PRIVATE_MESSAGE_AND_TOKEN", str(logs.output))
        self.assertIn("ValueError", str(logs.output))
        await dispatch.close()

    async def test_repeated_start_stop_leaves_no_workers(self):
        for _ in range(5):
            async def data(*args):
                await asyncio.sleep(60)
            dispatch = EventDispatch(data)
            dispatch.put(1, object())
            await asyncio.sleep(0)
            await dispatch.close()
            self.assertTrue(dispatch.task.done())
            self.assertEqual(dispatch.queue.qsize(), 0)
            with self.assertRaises(RuntimeError):
                dispatch.put(2, object())

    async def test_payload_owned_until_processed(self):
        import weakref
        class Payload: pass
        gate = asyncio.Event()
        async def data(*args):
            await gate.wait()
        dispatch = EventDispatch(data)
        payload = Payload()
        ref = weakref.ref(payload)
        dispatch.put(1, payload)
        del payload
        self.assertIsNotNone(ref())
        gate.set()
        await dispatch.queue.join()
        await dispatch.close()
        self.assertIsNone(ref())

if __name__ == "__main__":
    unittest.main()
