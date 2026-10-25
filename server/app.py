from __future__ import annotations

import asyncio
from contextlib import asynccontextmanager, suppress
from dataclasses import dataclass, field
import json
import secrets
import time
from urllib.parse import urlsplit

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field, field_validator

from server.game import GameError, Maze, Participant, ROOT, Room

rooms: dict[str, Room] = {}


@dataclass
class Peer:
    socket: WebSocket
    queue: asyncio.Queue = field(default_factory=lambda: asyncio.Queue(maxsize=2))

    def offer(self, message: dict):
        if self.queue.full():
            self.queue.get_nowait()
        self.queue.put_nowait(message)


def publish(room: Room):
    for person_id, peer in tuple(room.peers.items()):
        peer.offer(room.snapshot(room.people[person_id]))


async def ticker():
    while True:
        await asyncio.sleep(0.1)
        now = time.monotonic()
        for code, room in tuple(rooms.items()):
            room.update(now)
            publish(room)
            if now - room.created_at > 7200 and not room.peers:
                rooms.pop(code, None)


@asynccontextmanager
async def lifespan(app: FastAPI):
    task = asyncio.create_task(ticker())
    yield
    task.cancel()
    with suppress(asyncio.CancelledError):
        await task
    for room in list(rooms.values()):
        for peer in list(room.peers.values()):
            with suppress(Exception):
                await peer.socket.close(code=1001)
    rooms.clear()


app = FastAPI(title="Hipótesis Party", lifespan=lifespan)


class CreateRoom(BaseModel):
    minutes: int = Field(default=15, ge=1, le=60)


class JoinRoom(BaseModel):
    name: str = Field(min_length=1, max_length=20)

    @field_validator("name")
    @classmethod
    def clean_name(cls, name: str) -> str:
        name = " ".join(name.split())
        if not name or any(ord(c) < 32 for c in name):
            raise ValueError("Escribí un apodo válido.")
        return name


@app.get("/api/health")
async def health():
    return {"ok": True, "maze_ready": (ROOT / "build/native/maze_export").is_file(),
            "web_ready": (ROOT / "web/build/game.wasm").is_file()}


@app.post("/api/rooms", status_code=201)
async def create_room(body: CreateRoom):
    if len(rooms) >= 20:
        raise HTTPException(429, "Hay demasiadas salas activas.")
    code = "".join(secrets.choice("ABCDEFGHJKLMNPQRSTUVWXYZ23456789") for _ in range(5))
    while code in rooms:
        code = "".join(secrets.choice("ABCDEFGHJKLMNPQRSTUVWXYZ23456789") for _ in range(5))
    try:
        # Native generation happens off the asyncio loop so existing rooms stay responsive.
        maze = await asyncio.to_thread(Maze.generate)
    except (GameError, OSError, ValueError) as exc:
        raise HTTPException(503, str(exc)) from exc
    room = Room(code, maze, body.minutes)
    rooms[code] = room
    return {"code": code, "token": room.host.token, "role": "host",
            "teacher_token": room.teacher.token, "projector_token": room.projector.token}


@app.post("/api/rooms/{code}/join")
async def join_room(code: str, body: JoinRoom):
    room = rooms.get(code.upper())
    if not room:
        raise HTTPException(404, "No encontramos esa sala.")
    try:
        person = room.join(body.name)
    except GameError as exc:
        raise HTTPException(409, str(exc)) from exc
    return {"code": room.code, "token": person.token, "role": person.role}


async def sender(peer: Peer):
    try:
        while True:
            message = await peer.queue.get()
            await asyncio.wait_for(peer.socket.send_json(message), timeout=2)
    except (TimeoutError, RuntimeError, WebSocketDisconnect):
        with suppress(Exception):
            await peer.socket.close(code=1001)


def dispatch(room: Room, actor: Participant, data: dict):
    action = data.get("type")
    if not isinstance(action, str):
        raise GameError("Mensaje inválido.")
    if action == "move":
        direction = data.get("direction")
        if not isinstance(direction, str):
            raise GameError("Dirección inválida.")
        room.move(actor, direction)
    elif action == "interact":
        room.interact(actor)
    elif action == "answer":
        room.answer(actor, data.get("question_id"), data.get("choice"))
    elif action in {"start", "pause", "resume", "finish"}:
        room.control(actor, action)
    elif action == "dismiss_feedback":
        actor.feedback = None
    elif action != "ping":
        raise GameError("Acción desconocida.")


@app.websocket("/ws/{code}")
async def websocket(socket: WebSocket, code: str, token: str = ""):
    # Capabilities identify the actor; player ids/roles sent by the client are never trusted.
    origin = socket.headers.get("origin")
    if origin and urlsplit(origin).netloc != socket.headers.get("host"):
        await socket.close(code=1008)
        return
    room = rooms.get(code.upper())
    actor = room.identify(token) if room else None
    if not room or not actor:
        await socket.close(code=1008)
        return
    await socket.accept()
    old = room.peers.get(actor.id)
    if old:
        with suppress(Exception):
            await old.socket.close(code=4001, reason="Sesión abierta en otra pestaña")
    peer = Peer(socket)
    room.peers[actor.id] = peer
    actor.connected = True
    writer = asyncio.create_task(sender(peer))
    publish(room)
    window, count = time.monotonic(), 0
    try:
        while True:
            raw = await socket.receive_text()
            if len(raw) > 2048:
                await socket.close(code=1009)
                break
            now = time.monotonic()
            if now - window >= 1:
                window, count = now, 0
            count += 1
            if count > 30:
                continue
            try:
                data = json.loads(raw)
                if not isinstance(data, dict):
                    raise GameError("Mensaje inválido.")
                room.update()
                dispatch(room, actor, data)
                publish(room)
            except (GameError, json.JSONDecodeError) as exc:
                peer.offer({"type": "error", "message": str(exc)})
    except (WebSocketDisconnect, RuntimeError):
        pass
    finally:
        writer.cancel()
        with suppress(asyncio.CancelledError, Exception):
            await writer
        if room.peers.get(actor.id) is peer:
            room.peers.pop(actor.id, None)
            actor.connected = False
            publish(room)


@app.get("/")
async def index():
    return FileResponse(ROOT / "web/index.html")


app.mount("/static", StaticFiles(directory=ROOT / "web"), name="static")
