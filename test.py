import asyncio
import websockets

PORT = 11011

async def test():
    url = "ws://localhost:" + str(PORT) + "/"
    async with websockets.connect(url) as ws:
        pass

try:
    asyncio.get_event_loop().run_until_complete(test())
except KeyboardInterrupt:
    pass
asyncio.get_event_loop().close()