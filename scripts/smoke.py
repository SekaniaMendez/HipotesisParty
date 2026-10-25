"""Real HTTP + 25 WebSocket connections. Run against a running dev server."""
import argparse
import asyncio
import json
import time
from contextlib import AsyncExitStack

import httpx
from websockets.asyncio.client import connect


async def main(base: str):
    async with httpx.AsyncClient(base_url=base, timeout=10) as client, AsyncExitStack() as stack:
        health = (await client.get("/api/health")).json()
        assert health["maze_ready"] and health["web_ready"], health
        response = await client.post("/api/rooms", json={"minutes": 1})
        response.raise_for_status()
        room = response.json()
        code = room["code"]
        tokens = [room["token"], room["teacher_token"], room["projector_token"]]
        for i in range(22):
            joined = await client.post(f"/api/rooms/{code}/join", json={"name": f"Prueba {i+1:02}"})
            joined.raise_for_status()
            tokens.append(joined.json()["token"])
        latest = [None] * len(tokens)
        counts = [0] * len(tokens)
        async def reader(index, socket):
            async for raw in socket:
                data = json.loads(raw)
                if data["type"] == "state":
                    latest[index] = data
                    counts[index] += 1
        async def wait_for(predicate, timeout=5):
            async with asyncio.timeout(timeout):
                while not predicate():
                    await asyncio.sleep(.02)
        sockets, readers = [], []
        try:
            for i, token in enumerate(tokens):
                socket = await stack.enter_async_context(connect(f"{base.replace('http', 'ws', 1)}/ws/{code}?token={token}", close_timeout=1))
                sockets.append(socket)
                readers.append(asyncio.create_task(reader(i, socket)))
            await wait_for(lambda: all(latest))
            await sockets[0].send(json.dumps({"type": "start"}))
            await wait_for(lambda: all(s["status"] == "running" for s in latest))
            assert all(s["student_count"] == 22 for s in latest)
            # One valid move from every student, then verify each authoritative position.
            commands, originals = [], []
            directions = [("up", 1), ("right", 2), ("down", 4), ("left", 8)]
            for s in latest[3:]:
                cell = next(p["cell"] for p in s["players"] if p["id"] == s["self_id"])
                originals.append(cell)
                direction = next(d for d, bit in directions if s["paths"][cell] & bit)
                commands.append({"type": "move", "direction": direction})
            started = time.monotonic()
            await asyncio.gather(*(socket.send(json.dumps(command)) for socket, command in zip(sockets[3:], commands)))
            await wait_for(lambda: all(next(p["cell"] for p in s["players"] if p["id"] == s["self_id"]) != original
                                      for s, original in zip(latest[3:], originals)))
            elapsed = (time.monotonic() - started) * 1000
            assert -1 not in latest[0]["paths"]
            assert all(-1 in s["paths"] for s in latest[1:])
            await sockets[0].send(json.dumps({"type": "pause"}))
            await wait_for(lambda: all(s["status"] == "paused" for s in latest))
            await sockets[0].send(json.dumps({"type": "resume"}))
            await wait_for(lambda: all(s["status"] == "running" for s in latest))
            # Reload/reconnect a student with the same capability.
            identity = latest[-1]["self_id"]
            await sockets[-1].close()
            readers[-1].cancel()
            replacement = await stack.enter_async_context(connect(f"{base.replace('http', 'ws', 1)}/ws/{code}?token={tokens[-1]}", close_timeout=1))
            sockets[-1] = replacement
            latest[-1] = None
            readers.append(asyncio.create_task(reader(len(tokens)-1, replacement)))
            await wait_for(lambda: latest[-1] is not None)
            assert latest[-1]["self_id"] == identity
            assert latest[-1]["student_count"] == 22
            await sockets[0].send(json.dumps({"type": "finish"}))
            await wait_for(lambda: all(s["status"] == "finished" for s in latest))
            print(json.dumps({"ok": True, "connections": 25, "students": 22,
                              "all_student_moves_confirmed_ms": round(elapsed, 1),
                              "reconnection": "same identity", "finish": "all clients",
                              "snapshots_received": sum(counts)}, ensure_ascii=False))
        finally:
            # Keep readers draining while closing, so close frames aren't stuck behind snapshots.
            await asyncio.gather(*(socket.close() for socket in sockets), return_exceptions=True)
            for task in readers:
                task.cancel()
            await asyncio.gather(*readers, return_exceptions=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", default="http://127.0.0.1:8000")
    asyncio.run(main(parser.parse_args().url.rstrip("/")))
