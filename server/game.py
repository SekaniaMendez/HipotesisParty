"""Authoritative room state. All mutations run on the single asyncio event loop."""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
from pathlib import Path
import json
import random
import secrets
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
CONTENT = json.loads((ROOT / "server/content.json").read_text())
# Same OpenPaths bit layout as treasure_labyrinth/shared/Tile.h.
DIRECTIONS = {"up": (0, -1, 1, 4), "right": (1, 0, 2, 8),
              "down": (0, 1, 4, 1), "left": (-1, 0, 8, 2)}
MAX_STUDENTS = 22
PROTECTION = 8.0


class GameError(ValueError):
    pass


@dataclass
class Maze:
    width: int
    height: int
    paths: list[int]

    @classmethod
    def generate(cls) -> Maze:
        binary = ROOT / "build/native/maze_export"
        if not binary.is_file():
            raise GameError("Falta compilar el laberinto: bash scripts/build-native.sh")
        result = subprocess.run([str(binary), "18", "14"], capture_output=True,
                                text=True, check=True, timeout=5)
        return cls(**json.loads(result.stdout))

    def neighbor(self, cell: int, direction: str) -> int | None:
        dx, dy, bit, opposite = DIRECTIONS[direction]
        x, y = cell % self.width + dx, cell // self.width + dy
        if not (0 <= x < self.width and 0 <= y < self.height):
            return None
        other = y * self.width + x
        if self.paths[cell] & bit and self.paths[other] & opposite:
            return other
        return None

    def distances(self, start: int, radius: int = 10000) -> dict[int, int]:
        found = {start: 0}
        queue = deque([start])
        while queue:
            cell = queue.popleft()
            if found[cell] >= radius:
                continue
            for direction in DIRECTIONS:
                other = self.neighbor(cell, direction)
                if other is not None and other not in found:
                    found[other] = found[cell] + 1
                    queue.append(other)
        return found


@dataclass
class Participant:
    name: str
    role: str
    cell: int = 0
    id: str = field(default_factory=lambda: secrets.token_hex(6))
    token: str = field(default_factory=lambda: secrets.token_urlsafe(24))
    connected: bool = False
    score: int = 0
    captures: int = 0
    solved: set[int] = field(default_factory=set)
    protected_until: float = 0
    last_move: float = 0
    last_attempt: float = -100
    question: dict | None = None
    feedback: dict | None = None


class Room:
    def __init__(self, code: str, maze: Maze, minutes: int = 15):
        self.code, self.maze = code, maze
        self.created_at = time.monotonic()
        self.status = "lobby"
        self.duration = minutes * 60.0
        self.remaining = self.duration
        self.last_tick = self.created_at
        self.people: dict[str, Participant] = {}
        self.host = self._add("Director", "host")
        self.teacher = self._add("La profe", "teacher")
        self.projector = self._add("Proyector", "projector")
        self.teacher.cell = (maze.height // 2) * maze.width + maze.width // 2
        # Spread the challenges using graph distance, not just screen distance.
        self.stations: list[int] = []
        for _ in range(8):
            origins = self.stations or [self.teacher.cell]
            distances = [maze.distances(origin) for origin in origins]
            cell = max(range(len(maze.paths)),
                       key=lambda c: min(d[c] for d in distances))
            self.stations.append(cell)
        self.target: str | None = None
        self.event = "La sala está abierta. La profe todavía no ha salido."
        self.event_serial = 0
        self.peers: dict = {}

    @property
    def students(self) -> list[Participant]:
        return [p for p in self.people.values() if p.role == "student"]

    def _add(self, name: str, role: str) -> Participant:
        person = Participant(name=name, role=role)
        self.people[person.id] = person
        return person

    def spawn(self) -> int:
        distances = self.maze.distances(self.teacher.cell)
        occupied = {p.cell for p in self.students}
        choices = sorted(distances, key=distances.get, reverse=True)
        available = [c for c in choices[:max(40, len(choices)//3)] if c not in occupied]
        return random.choice(available or choices[:40])

    def join(self, name: str) -> Participant:
        if self.status == "finished":
            raise GameError("Esta partida ya terminó.")
        if len(self.students) >= MAX_STUDENTS:
            raise GameError("La sala está llena: máximo 22 estudiantes.")
        person = self._add(name, "student")
        person.cell = self.spawn()
        person.protected_until = self.elapsed + PROTECTION
        return person

    @property
    def elapsed(self) -> float:
        return self.duration - self.remaining

    def identify(self, token: str) -> Participant | None:
        return next((p for p in self.people.values()
                     if secrets.compare_digest(p.token, token)), None)

    def announce(self, text: str):
        self.event, self.event_serial = text, self.event_serial + 1

    def update(self, now: float | None = None):
        now = time.monotonic() if now is None else now
        delta, self.last_tick = max(0, now - self.last_tick), now
        if self.status != "running":
            return
        self.remaining = max(0, self.remaining - delta)
        if self.remaining == 0:
            self.finish()
            return
        for person in self.students:
            if person.question and self.elapsed >= person.question["deadline"]:
                self.resolve(person, None)
        self.check_capture()

    def control(self, actor: Participant, action: str):
        if actor.role != "host":
            raise GameError("Solo el director puede controlar la partida.")
        if action == "start" and self.status == "lobby":
            if not self.students:
                raise GameError("Esperá a que entre al menos un estudiante.")
            self.status = "running"
            self.last_tick = time.monotonic()
            self.announce("¡La profe ya está en el laberinto!")
        elif action == "pause" and self.status == "running":
            self.status = "paused"
        elif action == "resume" and self.status == "paused":
            self.status = "running"
            self.last_tick = time.monotonic()
        elif action == "finish" and self.status in {"running", "paused"}:
            self.finish()
        else:
            raise GameError("Esa acción no corresponde al estado actual.")

    def finish(self):
        self.status = "finished"
        self.target = None
        for person in self.students:
            person.question = None
        self.announce("Investigación cerrada. ¡Tenemos podio!")

    def move(self, actor: Participant, direction: str, now: float | None = None):
        if direction not in DIRECTIONS:
            raise GameError("Dirección inválida.")
        if self.status != "running" or actor.role not in {"teacher", "student"}:
            return
        if actor.question or (actor.role == "teacher" and self.target):
            return
        now = time.monotonic() if now is None else now
        cooldown = 0.22 if actor.role == "teacher" else 0.18
        if now - actor.last_move < cooldown:
            return
        actor.last_move = now
        cell = self.maze.neighbor(actor.cell, direction)
        if cell is not None:
            actor.cell = cell
            self.check_capture()

    def check_capture(self):
        if self.status != "running" or self.target or not self.teacher.connected:
            return
        for student in self.students:
            if (student.connected and student.cell == self.teacher.cell and not student.question
                    and student.protected_until <= self.elapsed):
                self.target = student.id
                student.captures += 1
                self.ask(student, "hard")
                self.announce(f"¡La profe encontró a {student.name}! Pregunta difícil en curso.")
                break

    def ask(self, actor: Participant, kind: str, station: int | None = None):
        bank = CONTENT[kind]
        index = random.randrange(len(bank))
        actor.feedback = None
        actor.question = {"kind": kind, "index": index, "station": station,
                          "id": secrets.token_hex(8),
                          "deadline": self.elapsed + (45 if kind == "hard" else 30)}

    def interact(self, actor: Participant):
        if self.status != "running" or actor.role != "student" or actor.question:
            return
        if actor.cell not in self.stations:
            raise GameError("Buscá una estación de pistas para responder.")
        if actor.cell in actor.solved:
            raise GameError("Ya recuperaste esta pista. Buscá otra estación.")
        if self.elapsed - actor.last_attempt < 10:
            raise GameError("Esperá unos segundos antes de intentar otra pista.")
        self.ask(actor, "normal", actor.cell)

    def answer(self, actor: Participant, question_id: str, choice: int):
        if self.status != "running" or not actor.question:
            raise GameError("No hay una pregunta activa.")
        if question_id != actor.question["id"]:
            raise GameError("Esa pregunta ya no está activa.")
        if type(choice) is not int or not 0 <= choice < 4:
            raise GameError("Elegí una respuesta válida.")
        if self.elapsed >= actor.question["deadline"]:
            self.resolve(actor, None)
        else:
            self.resolve(actor, choice)

    def resolve(self, actor: Participant, choice: int | None):
        q = actor.question
        source = CONTENT[q["kind"]][q["index"]]
        correct = choice == source["answer"]
        points = (20 if q["kind"] == "hard" else 10) if correct else (
            -5 if q["kind"] == "hard" else 0)
        previous = actor.score
        actor.score = max(0, actor.score + points)
        if correct and q["kind"] == "normal":
            actor.solved.add(q["station"])
        actor.last_attempt = self.elapsed
        actor.feedback = {"correct": correct, "points": actor.score - previous,
                          "explanation": source["explanation"], "timeout": choice is None}
        actor.question = None
        actor.protected_until = self.elapsed + PROTECTION
        if q["kind"] == "hard":
            self.target = None
            actor.cell = self.spawn()
            self.announce(f"{actor.name} terminó el desafío y regresó a una zona segura.")

    def snapshot(self, viewer: Participant) -> dict:
        focus = self.teacher if viewer.role in {"host", "projector"} else viewer
        visible = (set(range(len(self.maze.paths))) if viewer.role == "host"
                   else set(self.maze.distances(focus.cell, 4)))
        players = []
        for p in [self.teacher, *self.students]:
            if p.cell not in visible or (not p.connected and p.id != viewer.id):
                continue
            players.append({"id": p.id, "name": p.name, "role": p.role,
                            "cell": p.cell, "protected": p.protected_until > self.elapsed,
                            "busy": bool(p.question) or (p.role == "teacher" and bool(self.target))})
        q = None
        if viewer.question:
            source = CONTENT[viewer.question["kind"]][viewer.question["index"]]
            q = {"id": viewer.question["id"], "kind": viewer.question["kind"],
                 "prompt": source["prompt"], "choices": source["choices"],
                 "seconds": max(0, int(viewer.question["deadline"] - self.elapsed))}
        ranks = sorted(self.students, key=lambda p: (-p.score, -len(p.solved), p.name.casefold()))
        ranking = [{"id": p.id, "name": p.name, "score": p.score, "clues": len(p.solved),
                    "captures": p.captures, "connected": p.connected} for p in ranks]
        return {"type": "state", "code": self.code, "status": self.status,
                "remaining": int(self.remaining), "role": viewer.role, "self_id": viewer.id,
                "score": viewer.score, "clues": len(viewer.solved), "focus": focus.cell,
                "protected_seconds": max(0, int(viewer.protected_until - self.elapsed)),
                "width": self.maze.width, "height": self.maze.height,
                "paths": [mask if i in visible else -1 for i, mask in enumerate(self.maze.paths)],
                "players": players, "stations": [{"cell": c, "solved": c in viewer.solved}
                    for c in self.stations if c in visible],
                "question": q, "feedback": viewer.feedback, "ranking": ranking,
                "event": self.event, "event_serial": self.event_serial,
                "teacher_busy": bool(self.target), "student_count": len(self.students),
                "teacher_connected": self.teacher.connected}
