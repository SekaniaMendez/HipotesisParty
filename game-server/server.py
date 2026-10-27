#!/usr/bin/env python3
"""Hipótesis Party LAN host. Python stdlib HTTP, one authoritative C++ worker/room."""
from __future__ import annotations

import argparse
from collections import OrderedDict
from dataclasses import dataclass, field
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import os
from pathlib import Path
import re
import secrets
import select
import subprocess
import sys
import threading
import time
from urllib.parse import parse_qs, unquote, urlsplit

MAX_BODY = 16 * 1024
MAX_ROOMS = 32
MAX_ACTIONS_PER_ACTOR = 10000
MAX_ENROLLMENTS = 512
ENROLLMENT_TTL = 15 * 60
ROOM_TTL = 8 * 60 * 60
ROOT = Path(__file__).resolve().parents[1]


class ApiError(Exception):
    def __init__(self, status, code, message):
        super().__init__(message)
        self.status, self.code, self.message = status, code, message


def text_field(body, key, maximum=40, default=None):
    value = body.get(key, default)
    if not isinstance(value, str):
        raise ApiError(400, "invalid_field", f"El campo {key} debe ser texto.")
    value = value.strip()
    try:
        encoded = value.encode("utf-8")
    except UnicodeError:
        raise ApiError(400, "invalid_field", f"El campo {key} debe ser texto UTF-8 válido.") from None
    if not value or len(value) > maximum or any(ord(c) < 32 or ord(c) == 127 for c in value):
        raise ApiError(400, "invalid_field", f"El campo {key} no es válido.")
    if maximum == 40 and len(encoded) > 64:
        raise ApiError(400, "invalid_field", f"El campo {key} supera 64 bytes UTF-8.")
    return value


def int_field(body, key, low, high, default=None):
    value = body.get(key, default)
    if type(value) is not int or not low <= value <= high:
        raise ApiError(400, "invalid_field", f"El campo {key} debe ser un entero válido.")
    return value


def number_field(body, key, low, high):
    value = body.get(key)
    try:
        valid = type(value) in (int, float) and math.isfinite(value) and low <= value <= high
    except OverflowError:
        valid = False
    if not valid:
        raise ApiError(400, "invalid_field", f"El campo {key} debe ser un número finito válido.")
    return value


class Worker:
    def __init__(self, executable: Path, seed: int):
        self.process = subprocess.Popen(
            [str(executable), "--seed", str(seed)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=sys.stderr, bufsize=0,
        )
        self.buffer = b""
        self.failed = False

    def call(self, command):
        if self.failed or self.process.poll() is not None:
            self.failed = True
            raise ApiError(503, "room_unavailable", "La sala se detuvo. Crea una sala nueva.")
        # Invalid local data must never terminate an otherwise healthy room.
        try:
            payload = json.dumps(command, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode() + b"\n"
        except (ValueError, UnicodeError):
            raise ApiError(400, "invalid_command", "El comando no es JSON UTF-8 válido.") from None
        try:
            self.process.stdin.write(payload)
            self.process.stdin.flush()
            deadline = time.monotonic() + 5.0
            while b"\n" not in self.buffer:
                remaining = deadline - time.monotonic()
                if remaining <= 0 or not select.select([self.process.stdout], [], [], remaining)[0]:
                    raise TimeoutError("worker timeout")
                chunk = os.read(self.process.stdout.fileno(), 65536)
                if not chunk:
                    raise EOFError("worker closed")
                self.buffer += chunk
                if len(self.buffer) > 1024 * 1024:
                    raise ValueError("worker output too large")
            line, self.buffer = self.buffer.split(b"\n", 1)
            response = json.loads(line)
            if not isinstance(response, dict) or "ok" not in response:
                raise ValueError("invalid worker response")
            return response
        except (OSError, ValueError, EOFError, TimeoutError):
            self.failed = True
            self.close()
            raise ApiError(503, "room_unavailable", "La sala no pudo responder. Crea una sala nueva.") from None

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=1)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=1)
        for stream in (self.process.stdin, self.process.stdout):
            if stream:
                stream.close()


@dataclass
class Actor:
    role: str
    team_id: int
    requests: OrderedDict = field(default_factory=OrderedDict)
    seen: set = field(default_factory=set)


@dataclass
class Room:
    code: str
    worker: Worker
    teacher_name: str = "Profesora"
    actors: dict = field(default_factory=dict)
    lock: threading.RLock = field(default_factory=threading.RLock)
    touched: float = field(default_factory=time.monotonic)


class RoomStore:
    def __init__(self, worker_path: Path):
        self.worker_path = worker_path.resolve()
        self.rooms = {}
        self.lock = threading.RLock()
        self.enrollments = {}
        self.enrollment_lock = threading.RLock()

    def close(self):
        with self.lock:
            for room in self.rooms.values():
                with room.lock:
                    room.worker.close()
            self.rooms.clear()

    def enroll(self, kind, body):
        """Optional client-generated 128-bit ID makes lost enrollment replies retryable."""
        perform = self.create if kind == "rooms" else self.join
        if "requestId" not in body:
            return perform(body)
        request_id = text_field(body, "requestId", 36)
        if not re.fullmatch(r"[0-9a-fA-F]{32}|[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}", request_id):
            raise ApiError(400, "invalid_request_id", "Usa un requestId aleatorio de 128 bits en hexadecimal o UUID.")
        request_id = request_id.replace("-", "").lower()
        canonical_body = dict(body)
        canonical_body.pop("requestId")
        fingerprint = kind + ":" + json.dumps(canonical_body, sort_keys=True, ensure_ascii=True, allow_nan=False)
        with self.enrollment_lock:
            now = time.monotonic()
            for old_id in list(self.enrollments):
                if self.enrollments[old_id][0] <= now:
                    del self.enrollments[old_id]
            if request_id in self.enrollments:
                _, previous, response = self.enrollments[request_id]
                if previous != fingerprint:
                    raise ApiError(409, "request_conflict", "Ese requestId ya identifica otra inscripción.")
                return response
            if len(self.enrollments) >= MAX_ENROLLMENTS:
                raise ApiError(503, "enrollment_capacity", "Hay demasiadas inscripciones recientes; vuelve a intentar más tarde.")
            response = perform(body)
            self.enrollments[request_id] = (now + ENROLLMENT_TTL, fingerprint, response)
            return response

    def create(self, body):
        text_field(body, "name", default="Profesora")
        teacher_name = text_field(body, "avatarName", default="Profesora")
        seed = int_field(body, "seed", 0, 0xFFFFFFFF, secrets.randbits(32))
        with self.lock:
            now = time.monotonic()
            for code in list(self.rooms):
                room = self.rooms[code]
                if now - room.touched > ROOM_TTL:
                    with room.lock:
                        room.worker.close()
                    del self.rooms[code]
            if len(self.rooms) >= MAX_ROOMS:
                raise ApiError(503, "room_capacity", "El anfitrión alcanzó el límite de salas.")
            alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
            while True:
                code = "".join(secrets.choice(alphabet) for _ in range(6))
                if code not in self.rooms:
                    break
            try:
                worker = Worker(self.worker_path, seed)
            except OSError:
                raise ApiError(503, "worker_missing", "Compila SessionWorker antes de abrir una sala.") from None
            room = Room(code, worker, teacher_name=teacher_name)
            token = secrets.token_urlsafe(32)
            actor = Actor("teacher", -1)
            room.actors[token] = actor
            try:
                state = self.snapshot(room, actor)
            except ApiError:
                worker.close()
                raise
            self.rooms[code] = room
            return {"ok": True, "code": code, "token": token, "role": actor.role,
                    "teamId": actor.team_id, "state": state}

    def room(self, body):
        code = text_field(body, "code", 6).upper()
        with self.lock:
            room = self.rooms.get(code)
        if room is None:
            raise ApiError(404, "room_not_found", "No existe una sala con ese código.")
        return room

    def authenticate(self, room, body):
        token = text_field(body, "token", 128)
        actor = room.actors.get(token)
        if actor is None:
            raise ApiError(403, "unauthorized", "La credencial no pertenece a esta sala.")
        room.touched = time.monotonic()
        return actor

    @staticmethod
    def checked(response):
        if not response.get("ok"):
            error = response.get("error", {})
            raise ApiError(409, error.get("code", "action_rejected"),
                           error.get("message", "La acción no se puede realizar ahora."))
        return response

    def snapshot(self, room, actor):
        response = self.checked(room.worker.call({"op": "state", "role": actor.role, "teamId": actor.team_id}))
        state = response["state"]
        state["roomCode"] = room.code
        state["teacherName"] = room.teacher_name
        return state

    def join(self, body):
        name = text_field(body, "teamName")
        avatar = text_field(body, "avatarName")
        index = int_field(body, "avatarIndex", 0, 5)
        room = self.room(body)
        with room.lock:
            response = self.checked(room.worker.call({"op": "add_team", "name": name,
                                "avatarName": avatar, "avatarIndex": index}))
            token = secrets.token_urlsafe(32)
            actor = Actor("team", response["teamId"])
            room.actors[token] = actor
            room.touched = time.monotonic()
            return {"ok": True, "code": room.code, "token": token, "role": actor.role,
                    "teamId": actor.team_id, "state": self.snapshot(room, actor)}

    def state(self, body):
        room = self.room(body)
        with room.lock:
            actor = self.authenticate(room, body)
            return {"ok": True, "state": self.snapshot(room, actor)}

    def action(self, body):
        request_id = text_field(body, "requestId", 80)
        action = text_field(body, "action", 32)
        command = {"op": "action", "action": action}
        if action in ("move", "teacher_move"):
            command.update(x=int_field(body, "x", 0, 511), z=int_field(body, "z", 0, 511))
        elif action in ("easy_answer", "hard_answer"):
            command.update(questionId=int_field(body, "questionId", 1, 0x7FFFFFFF),
                           choice=int_field(body, "choice", 0, 2))
        elif action == "final":
            if type(body.get("reject")) is not bool:
                raise ApiError(400, "invalid_field", "El campo reject debe ser booleano.")
            command.update(alternative=int_field(body, "alternative", 0, 2),
                           z=number_field(body, "z", -1e6, 1e6),
                           p=number_field(body, "p", 0, 1), reject=body["reject"])
        elif action not in ("start", "pause", "resume"):
            raise ApiError(400, "unknown_action", "Acción desconocida.")
        room = self.room(body)
        with room.lock:
            actor = self.authenticate(room, body)
            teacher_action = action in ("start", "pause", "resume", "teacher_move")
            if teacher_action != (actor.role == "teacher"):
                raise ApiError(403, "wrong_role", "Esta acción corresponde a otro rol.")
            command.update(role=actor.role, teamId=actor.team_id)
            fingerprint = json.dumps(command, sort_keys=True, ensure_ascii=False, allow_nan=False)
            if request_id in actor.requests:
                previous, status, response = actor.requests[request_id]
                if previous != fingerprint:
                    raise ApiError(409, "request_conflict", "Ese requestId ya identifica otra acción.")
                actor.requests.move_to_end(request_id)
                return status, response
            if request_id in actor.seen:
                raise ApiError(409, "stale_request", "La acción ya fue procesada; consulta el estado actual.")
            if len(actor.seen) >= MAX_ACTIONS_PER_ACTOR:
                raise ApiError(429, "session_limit", "Se alcanzó el límite de acciones de esta sesión.")
            worker_response = room.worker.call(command)
            status = 200 if worker_response.get("ok") else 409
            if status == 200:
                response = {"ok": True, "result": worker_response.get("result", {}),
                            "state": self.snapshot(room, actor)}
            else:
                response = worker_response
            actor.requests[request_id] = (fingerprint, status, response)
            actor.seen.add(request_id)
            while len(actor.requests) > 128:
                actor.requests.popitem(last=False)
            return status, response


class GameHTTPServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address, store, static_dir):
        self.store = store
        self.static_dir = Path(static_dir).resolve()
        super().__init__(address, Handler)


class Handler(SimpleHTTPRequestHandler):
    server_version = "HipotesisParty/1.0"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(args[2].static_dir), **kwargs)

    def setup(self):
        super().setup()
        self.connection.settimeout(10)

    def log_message(self, format, *args):
        # Credentials exist only in API query/body; never print query strings.
        path = urlsplit(self.path).path if hasattr(self, "path") else ""
        sys.stderr.write(f"{self.address_string()} {self.command} {path}\n")

    def end_headers(self):
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        super().end_headers()

    def send_json(self, status, payload):
        data = json.dumps(payload, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def check_origin(self):
        origin = self.headers.get("Origin")
        if origin:
            parsed = urlsplit(origin)
            if parsed.scheme not in ("http", "https") or parsed.netloc != self.headers.get("Host"):
                raise ApiError(403, "origin_denied", "La solicitud debe venir del mismo anfitrión.")

    def read_body(self):
        if self.headers.get_content_type() != "application/json":
            raise ApiError(415, "content_type", "Usa Content-Type: application/json.")
        try:
            size = int(self.headers.get("Content-Length", "-1"))
        except ValueError:
            size = -1
        if size < 0 or size > MAX_BODY:
            raise ApiError(413, "body_size", "El cuerpo debe tener como máximo 16 KB.")
        try:
            def invalid_constant(_):
                raise ValueError("nonfinite JSON")
            def finite_float(value):
                number = float(value)
                if not math.isfinite(number):
                    raise ValueError("nonfinite JSON")
                return number
            body = json.loads(self.rfile.read(size), parse_constant=invalid_constant, parse_float=finite_float)
        except (ValueError, UnicodeError, RecursionError):
            raise ApiError(400, "invalid_json", "El cuerpo debe ser JSON válido con números finitos.") from None
        if not isinstance(body, dict):
            raise ApiError(400, "invalid_json", "El cuerpo debe ser un objeto JSON.")
        return body

    def api(self, callback):
        try:
            self.check_origin()
            status, payload = callback()
            self.send_json(status, payload)
        except ApiError as error:
            self.send_json(error.status, {"ok": False, "error": {"code": error.code, "message": error.message}})
        except (BrokenPipeError, ConnectionResetError):
            pass
        except Exception:
            self.send_json(500, {"ok": False, "error": {"code": "server_error", "message": "No se pudo procesar la solicitud."}})

    def do_GET(self):
        path = urlsplit(self.path).path
        if path.startswith("/api/"):
            def get_state():
                if path != "/api/state":
                    raise ApiError(404, "not_found", "Ruta desconocida.")
                query = parse_qs(urlsplit(self.path).query)
                body = {key: values[0] for key, values in query.items() if len(values) == 1}
                return 200, self.server.store.state(body)
            self.api(get_state)
            return
        if path == "/":
            self.path = "/HypotesisParty.html"
        super().do_GET()

    def do_POST(self):
        path = urlsplit(self.path).path
        def post():
            body = self.read_body()
            if path == "/api/rooms":
                return 201, self.server.store.enroll("rooms", body)
            if path == "/api/join":
                return 200, self.server.store.enroll("join", body)
            if path == "/api/action":
                return self.server.store.action(body)
            raise ApiError(404, "not_found", "Ruta desconocida.")
        self.api(post)

    def do_OPTIONS(self):
        self.api(lambda: (_ for _ in ()).throw(ApiError(403, "origin_denied", "No se admite acceso desde otros orígenes.")))

    def list_directory(self, path):
        self.send_error(404, "Not found")
        return None

    def send_head(self):
        # A build directory also contains archives/objects with authoritative
        # rules. Only published browser outputs and runtime assets are served.
        path = unquote(urlsplit(self.path).path)
        if path == "/":
            self.path = "/HypotesisParty.html"
            path = self.path
        runtime_files = {"/HypotesisParty.html", "/HypotesisParty.js", "/HypotesisParty.wasm",
                         "/HypotesisParty.data", "/HypotesisParty.worker.js", "/favicon.ico"}
        asset = path.startswith("/assets/") and Path(path).suffix.lower() in {".glb", ".ttf", ".vs", ".fs", ".png"}
        resolved = Path(self.translate_path(path)).resolve()
        if ((path not in runtime_files and not asset) or ".." in path.split("/") or
                not resolved.is_relative_to(self.server.static_dir)):
            self.send_error(404, "Not found")
            return None
        return super().send_head()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1", help="0.0.0.0 para dispositivos de la misma LAN")
    parser.add_argument("--port", type=int, default=8001)
    parser.add_argument("--worker", type=Path, default=ROOT / "build/game-server/SessionWorker")
    parser.add_argument("--web-dir", type=Path, default=ROOT / "build-web")
    args = parser.parse_args()
    if not args.worker.is_file():
        parser.error(f"No existe el worker: {args.worker}. Compila el proyecto nativo.")
    if not (args.web_dir / "HypotesisParty.html").is_file():
        parser.error(f"No existe HypotesisParty.html en {args.web_dir}. Compila el cliente web.")
    store = RoomStore(args.worker)
    server = GameHTTPServer((args.host, args.port), store, args.web_dir)
    print(f"Hipótesis Party: http://{args.host}:{server.server_port}/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        store.close()


if __name__ == "__main__":
    main()
