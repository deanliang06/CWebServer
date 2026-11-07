import asyncio
import importlib.util
import os
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('bench', Path(__file__).with_name('run.py'))
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)


class MetricsTests(unittest.TestCase):
    def test_nearest_rank_percentiles(self):
        self.assertIsNone(bench.percentile([], .95))
        self.assertEqual(bench.percentile(list(range(1, 101)), .95), 95)
        self.assertEqual(bench.percentile([9], .99), 9)

    @unittest.skipUnless(Path('/proc/self/stat').exists(), 'Linux /proc required')
    def test_process_group_resource_accounting(self):
        cpu, rss, count = bench.resources(os.getpgrp())
        self.assertGreaterEqual(cpu, 0)
        self.assertGreater(rss, 0)
        self.assertGreaterEqual(count, 1)


class IntegrityTests(unittest.IsolatedAsyncioTestCase):
    async def check_response(self, response, expected, error=None):
        async def handler(reader, writer):
            await reader.readuntil(b'\r\n\r\n')
            # Deliberately fragment header and binary body across writes.
            for offset in range(0, len(response), 3):
                writer.write(response[offset:offset + 3])
                await writer.drain()
            writer.close()
            await writer.wait_closed()

        server = await asyncio.start_server(handler, '127.0.0.1', 0)
        async with server:
            port = server.sockets[0].getsockname()[1]
            if error:
                with self.assertRaises(error):
                    await bench.fetch(port, 'test.png', expected, 2)
            else:
                self.assertGreater(await bench.fetch(port, 'test.png', expected, 2), 0)

    async def test_binary_fragmentation(self):
        await self.check_response(b'HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\na\x00b', b'a\x00b')

    async def test_truncated_response_rejected(self):
        await self.check_response(b'HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\na', b'abc', asyncio.IncompleteReadError)

    async def test_wrong_body_rejected(self):
        await self.check_response(b'HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nxyz', b'abc', ValueError)

    async def test_extra_bytes_rejected(self):
        await self.check_response(b'HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabc\x00', b'abc', ValueError)


if __name__ == '__main__':
    unittest.main()
