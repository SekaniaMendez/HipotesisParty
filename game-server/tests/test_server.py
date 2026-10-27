"""Real HTTP + native worker integration; no rendering, third-party packages, or mocks."""
import concurrent.futures
import contextlib
import http.client
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import sys
import tempfile
import threading
import unittest
import uuid
from urllib.parse import urlencode

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("party_server", ROOT / "game-server/server.py")
server = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = server
spec.loader.exec_module(server)
WORKER = Path(os.environ.get("HP_WORKER", ROOT / "build/game-server/SessionWorker"))


class ServerTests(unittest.TestCase):
    def setUp(self):
        if not WORKER.is_file():
            self.fail(f"Build SessionWorker first, or set HP_WORKER: {WORKER}")
        self.directory = tempfile.TemporaryDirectory()
        Path(self.directory.name, "HypotesisParty.html").write_text("<!doctype html><title>Campus</title>")
        self.store = server.RoomStore(WORKER)
        self.http = server.GameHTTPServer(("127.0.0.1", 0), self.store, self.directory.name)
        self.thread = threading.Thread(target=self.http.serve_forever, daemon=True)
        self.thread.start()
        self.counter = 0

    def tearDown(self):
        self.http.shutdown()
        self.http.server_close()
        self.thread.join(timeout=2)
        self.store.close()
        self.directory.cleanup()

    def request(self, method, path, body=None, headers=None, raw=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.http.server_port, timeout=5)
        headers = dict(headers or {})
        if body is not None:
            raw = json.dumps(body).encode()
            headers.setdefault("Content-Type", "application/json")
        connection.request(method, path, body=raw, headers=headers)
        response = connection.getresponse()
        data = response.read()
        code = response.status
        connection.close()
        return code, json.loads(data) if path.startswith("/api/") else data.decode()

    def create(self):
        status, response = self.request("POST", "/api/rooms", {"name": "Clase", "avatarName": "Elena", "seed": 42})
        self.assertEqual(status, 201, response)
        return response

    def join(self, teacher, name="Azul", index=0):
        status, response = self.request("POST", "/api/join", {
            "code": teacher["code"], "teamName": name, "avatarName": "Ada", "avatarIndex": index})
        self.assertEqual(status, 200, response)
        return response

    def state(self, actor):
        status, response = self.request("GET", "/api/state?" + urlencode({"code": actor["code"], "token": actor["token"]}))
        self.assertEqual(status, 200, response)
        return response["state"]

    def action(self, actor, action, request_id=None, **fields):
        self.counter += 1
        return self.request("POST", "/api/action", {
            "code": actor["code"], "token": actor["token"], "action": action,
            "requestId": request_id or f"request-{self.counter}", **fields})

    def test_lobby_ownership_privacy_and_pause(self):
        teacher = self.create()
        self.assertEqual(teacher["state"]["teacherName"], "Elena")
        first = self.join(teacher)
        second = self.join(teacher, "Verde", 1)
        view = self.state(first)
        self.assertEqual(view["phase"], "lobby")
        self.assertIsNone(view["seed"])
        self.assertLess(len(view["cells"]), view["width"] * view["height"])
        self.assertEqual(view["own"]["clues"], [])
        self.assertIsNone(view["question"])
        self.assertNotEqual(view["own"]["hypothesis"]["title"], self.state(second)["own"]["hypothesis"]["title"])
        privileged = self.state(teacher)
        self.assertEqual(len(privileged["cells"]), 169)
        self.assertEqual(len(privileged["objects"]), 28)
        self.assertNotIn(second["token"], json.dumps(privileged))
        self.assertEqual(self.action(first, "start")[0], 403)
        self.assertEqual(self.action(teacher, "start")[0], 200)
        state = self.state(first)
        self.assertNotIn("correct", state["question"])
        self.assertNotIn("explanation", state["question"])
        self.assertIsNone(state["own"]["assessment"])
        foreign = self.state(second)["question"]["id"]
        self.assertEqual(self.action(first, "easy_answer", questionId=foreign, choice=0)[0], 409)
        self.assertEqual(self.action(first, "teacher_move", x=1, z=0)[0], 403)
        self.assertEqual(self.action(teacher, "pause")[0], 200)
        self.assertEqual(self.state(second)["phase"], "paused")
        self.assertEqual(self.action(first, "easy_answer", questionId=state["question"]["id"], choice=0)[0], 409)
        self.assertEqual(self.action(teacher, "teacher_move", x=1, z=0)[0], 200)
        self.assertEqual(self.action(teacher, "resume")[0], 200)
        status, response = self.request("POST", "/api/join", {"code": teacher["code"], "teamName": "Tarde", "avatarName": "Luz", "avatarIndex": 2})
        self.assertEqual(status, 409, response)
        self.assertEqual(self.action(first, "final", alternative=0, z=0, p=0.5, reject=False)[0], 409)

    def test_enrollment_retry_keeps_credentials_and_rejects_conflicts(self):
        body = {"name": "Clase", "avatarName": "Elena", "seed": 42, "requestId": uuid.uuid4().hex}
        status, teacher = self.request("POST", "/api/rooms", body)
        self.assertEqual(status, 201)
        status, replay = self.request("POST", "/api/rooms", body)
        self.assertEqual(status, 201)
        self.assertEqual(replay, teacher)
        self.assertEqual(len(self.store.rooms), 1)
        conflict = dict(body, avatarName="Otra persona")
        status, response = self.request("POST", "/api/rooms", conflict)
        self.assertEqual(status, 409)
        self.assertEqual(response["error"]["code"], "request_conflict")
        self.assertNotIn("token", response)
        join = {"code": teacher["code"], "teamName": "Azul", "avatarName": "Ada", "avatarIndex": 0, "requestId": str(uuid.uuid4())}
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
            replies = list(executor.map(lambda _: self.request("POST", "/api/join", join), range(2)))
        self.assertEqual(replies[0][0], 200)
        self.assertEqual(replies[0], replies[1])
        self.assertEqual(len(self.state(teacher)["teams"]), 1)
        status, response = self.request("POST", "/api/join", dict(join, teamName="Verde"))
        self.assertEqual(status, 409)
        self.assertNotIn("token", response)
        status, response = self.request("POST", "/api/rooms", {"requestId": "predictable"})
        self.assertEqual(status, 400)

    def test_idempotency_concurrent_retry_and_retired_ids(self):
        teacher = self.create()
        team = self.join(teacher)
        self.action(teacher, "start")
        question = self.state(team)["question"]["id"]
        def answer():
            return self.action(team, "easy_answer", request_id="stable-answer", questionId=question, choice=0)
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
            responses = list(executor.map(lambda _: answer(), range(2)))
        self.assertEqual(responses[0], responses[1])
        self.assertEqual(responses[0][0], 200)
        self.assertEqual(self.state(team)["own"]["moves"], 5)
        self.assertEqual(self.state(team)["own"]["easyAnswered"], 1)
        self.assertEqual(self.action(team, "easy_answer", request_id="stable-answer", questionId=question, choice=1)[0], 409)
        for i in range(130):
            self.assertEqual(self.action(team, "move", request_id=f"blocked-{i}", x=12, z=12)[0], 409)
        status, response = answer()
        self.assertEqual(status, 409)
        self.assertEqual(response["error"]["code"], "stale_request")
        self.assertEqual(self.state(team)["own"]["easyAnswered"], 1)

    def test_wrong_answer_grants_two_and_wall_blocks(self):
        teacher = self.create()
        team = self.join(teacher)
        self.action(teacher, "start")
        question = self.state(team)["question"]["id"]
        status, response = self.action(team, "easy_answer", questionId=question, choice=1)
        self.assertEqual(status, 200)
        self.assertFalse(response["result"]["correct"])
        self.assertEqual(response["state"]["own"]["moves"], 2)
        self.assertEqual(self.action(team, "move", x=0, z=1)[0], 409)
        self.assertEqual(self.action(team, "move", x=1, z=0)[0], 200)
        self.assertEqual(self.state(team)["own"]["moves"], 1)

    def test_input_validation_cannot_kill_room(self):
        teacher = self.create()
        status, _ = self.request("POST", "/api/join", {"code": teacher["code"], "teamName": "\ud800", "avatarName": "Ada", "avatarIndex": 0})
        self.assertEqual(status, 400)
        self.assertEqual(self.state(teacher)["phase"], "lobby")
        status, _ = self.request("POST", "/api/rooms", raw=b'{"seed":NaN}', headers={"Content-Type": "application/json"})
        self.assertEqual(status, 400)
        status, _ = self.request("POST", "/api/rooms", raw=b"x" * (server.MAX_BODY + 1), headers={"Content-Type": "application/json"})
        self.assertEqual(status, 413)
        status, _ = self.request("POST", "/api/rooms", {"name": "No"}, headers={"Origin": "https://foreign.example"})
        self.assertEqual(status, 403)
        status, _ = self.request("GET", "/api/state?" + urlencode({"code": teacher["code"], "token": "forged"}))
        self.assertEqual(status, 403)
        another = self.create()
        status, _ = self.request("GET", "/api/state?" + urlencode({"code": another["code"], "token": teacher["token"]}))
        self.assertEqual(status, 403)
        team = self.join(teacher)
        status, _ = self.action(team, "final", alternative=0, z=10**1000, p=0.5, reject=False)
        self.assertEqual(status, 400)

    def test_worker_failure_is_explicit(self):
        teacher = self.create()
        worker = self.store.rooms[teacher["code"]].worker
        worker.process.kill()
        worker.process.wait(timeout=2)
        status, response = self.request("GET", "/api/state?" + urlencode({"code": teacher["code"], "token": teacher["token"]}))
        self.assertEqual(status, 503)
        self.assertEqual(response["error"]["code"], "room_unavailable")

    def test_capacity_static_page_and_log_privacy(self):
        teacher = self.create()
        for i in range(6):
            self.join(teacher, f"Equipo {i}", i)
        status, _ = self.request("POST", "/api/join", {"code": teacher["code"], "teamName": "Séptimo", "avatarName": "Ada", "avatarIndex": 0})
        self.assertEqual(status, 409)
        status, content = self.request("GET", "/")
        self.assertEqual(status, 200)
        self.assertIn("Campus", content)
        Path(self.directory.name, "libGameCore.a").write_bytes(b"private answer bank")
        self.assertEqual(self.request("GET", "/libGameCore.a")[0], 404)
        output = io.StringIO()
        with contextlib.redirect_stderr(output):
            self.state(teacher)
        self.assertIn("/api/state", output.getvalue())
        self.assertNotIn(teacher["token"], output.getvalue())

    def test_full_team_expedition_and_private_clues(self):
        teacher = self.create()
        team = self.join(teacher)
        other = self.join(teacher, "Verde", 1)
        self.action(teacher, "start")
        full = self.state(teacher)
        topology = {(c["x"], c["z"]): c["open"] for c in full["cells"]}
        directions = [(0,-1), (1,0), (0,1), (-1,0)]
        def route(start, goal):
            queue = [start]
            parents = {start: None}
            for at in queue:
                if at == goal:
                    break
                for bit, (dx,dz) in enumerate(directions):
                    next_cell = (at[0] + dx, at[1] + dz)
                    if topology[at] & (1 << bit) and next_cell not in parents:
                        parents[next_cell] = at
                        queue.append(next_cell)
            path = [goal]
            while path[-1] != start:
                path.append(parents[path[-1]])
            return list(reversed(path))
        view = self.state(team)
        actions = 0
        while not view["own"]["canSubmitFinal"]:
            while view["question"]:
                question = view["question"]
                status, response = self.action(team, question["kind"] + "_answer", questionId=question["id"], choice=0)
                self.assertEqual(status, 200, response)
                view = response["state"]
            current = next(t["cell"] for t in view["teams"] if t["id"] == team["teamId"])
            start = (current["x"], current["z"])
            targets = [o for o in self.state(teacher)["objects"] if o["owner"] == team["teamId"] and not o["resolved"]]
            if not targets:
                break
            path = min((route(start, (o["x"],o["z"])) for o in targets), key=len)
            for x, z in path[1:]:
                while view["question"]:
                    question = view["question"]
                    status, response = self.action(team, question["kind"] + "_answer", questionId=question["id"], choice=0)
                    self.assertEqual(status, 200, response)
                    view = response["state"]
                status, response = self.action(team, "move", x=x, z=z)
                self.assertEqual(status, 200, response)
                view = response["state"]
                actions += 1
                self.assertLess(actions, 2000, "expedition stalled")
        self.assertEqual(view["own"]["hardAnswered"], 10)
        self.assertEqual(len(view["own"]["clues"]), 4)
        other_view = self.state(other)
        self.assertEqual(other_view["own"]["clues"], [])
        serialized_other = json.dumps(other_view)
        for clue in view["own"]["clues"]:
            self.assertNotIn(clue["text"], serialized_other)
        z = (492 - 500) / (18 / math.sqrt(36))
        p = 0.5 * math.erfc(-z / math.sqrt(2))
        status, response = self.action(team, "final", alternative=0, z=z, p=p, reject=True)
        self.assertEqual(status, 200, response)
        self.assertEqual(response["result"]["criteria"], [True] * 4)
        self.assertTrue(response["state"]["own"]["finalSubmitted"])
        score = response["state"]["own"]["score"]
        self.assertEqual(self.action(team, "final", alternative=0, z=z, p=p, reject=True)[0], 409)
        self.assertEqual(self.state(team)["own"]["score"], score)
        self.assertFalse(self.state(other)["own"]["finalSubmitted"])


if __name__ == "__main__":
    unittest.main()
