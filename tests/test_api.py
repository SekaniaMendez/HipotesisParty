from contextlib import ExitStack

import pytest
from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect

from server.app import app, rooms


def receive_state(socket, predicate=lambda s: True):
    for _ in range(100):
        result = socket.receive_json()
        if result["type"] == "state" and predicate(result):
            return result
    raise AssertionError("No matching state received")


def test_http_websocket_roles_reconnection_and_finish():
    with TestClient(app) as client:
        assert client.get("/").status_code == 200
        assert client.get("/api/health").json()["maze_ready"]
        created = client.post("/api/rooms", json={"minutes": 1}).json()
        code = created["code"]
        assert client.post(f"/api/rooms/{code}/join", json={"name": "  "}).status_code == 422
        joined = client.post(f"/api/rooms/{code}/join", json={"name": "Ana"}).json()
        with client.websocket_connect(f"/ws/{code}?token={created['token']}") as host:
            with client.websocket_connect(f"/ws/{code}?token={joined['token']}") as student:
                initial = receive_state(student)
                person_id = initial["self_id"]
                student.send_json({"type": "start", "role": "host"})
                while True:
                    message = student.receive_json()
                    if message["type"] == "error":
                        assert "director" in message["message"]
                        break
                assert rooms[code].status == "lobby"
                host.send_json({"type": "start"})
                running = receive_state(student, lambda s: s["status"] == "running")
                assert running["question"] is None
                student.send_json({"type": [], "direction": []})
                student.send_json({"type": "move", "direction": "down"})
                receive_state(student)
            with client.websocket_connect(f"/ws/{code}?token={joined['token']}") as again:
                restored = receive_state(again)
                assert restored["self_id"] == person_id
                assert restored["student_count"] == 1
                host.send_json({"type": "finish"})
                assert receive_state(again, lambda s: s["status"] == "finished")["ranking"][0]["name"] == "Ana"


def test_22_students_teacher_host_and_projector_connected_together():
    with TestClient(app) as client, ExitStack() as stack:
        created = client.post("/api/rooms", json={}).json()
        code = created["code"]
        tokens = [created["token"], created["teacher_token"], created["projector_token"]]
        for i in range(22):
            response = client.post(f"/api/rooms/{code}/join", json={"name": f"Estudiante {i}"})
            assert response.status_code == 200
            tokens.append(response.json()["token"])
        assert client.post(f"/api/rooms/{code}/join", json={"name": "Exceso"}).status_code == 409
        sockets = [stack.enter_context(client.websocket_connect(f"/ws/{code}?token={token}")) for token in tokens]
        sockets[0].send_json({"type": "start"})
        for socket in sockets:
            state = receive_state(socket, lambda s: s["status"] == "running")
            assert state["student_count"] == 22
            assert len(state["ranking"]) == 22
        assert len(rooms[code].peers) == 25
        sockets[0].send_json({"type": "pause"})
        receive_state(sockets[-1], lambda s: s["status"] == "paused")
        sockets[0].send_json({"type": "resume"})
        receive_state(sockets[-1], lambda s: s["status"] == "running")


def test_invalid_capabilities_and_foreign_websocket_origin():
    with TestClient(app) as client:
        created = client.post("/api/rooms", json={}).json()
        code = created["code"]
        with pytest.raises(WebSocketDisconnect):
            with client.websocket_connect(f"/ws/{code}?token=invalid"):
                pass
        with pytest.raises(WebSocketDisconnect):
            with client.websocket_connect(f"/ws/{code}?token={created['token']}", headers={"origin": "https://foreign.invalid"}):
                pass
