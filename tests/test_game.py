import json
import subprocess
import time

import pytest

from server.game import CONTENT, DIRECTIONS, GameError, MAX_STUDENTS, Maze, ROOT, Room


@pytest.fixture
def room():
    return Room("TEST1", Maze.generate(), minutes=1)


@pytest.mark.parametrize("engine", ["dfs", "bfs"])
def test_imported_generator_connects_every_cell_reciprocally(engine):
    for _ in range(3):
        result = subprocess.run([str(ROOT / "build/native/maze_export"), "18", "14", engine],
                                capture_output=True, text=True, check=True)
        maze = Maze(**json.loads(result.stdout))
        assert len(maze.distances(0)) == 18 * 14
        for cell, mask in enumerate(maze.paths):
            for direction, (dx, dy, bit, opposite) in DIRECTIONS.items():
                if mask & bit:
                    other = maze.neighbor(cell, direction)
                    assert other is not None
                    assert maze.paths[other] & opposite


def test_capacity_and_distinct_spawn_positions(room):
    for i in range(MAX_STUDENTS):
        room.join(f"Estudiante {i}")
    assert len({s.cell for s in room.students}) == MAX_STUDENTS
    with pytest.raises(GameError, match="llena"):
        room.join("Exceso")


def test_movement_is_validated_and_spectators_cannot_move(room):
    student = room.join("Ana")
    room.control(room.host, "start")
    student.cell = 0
    room.move(student, "up", now=10)
    assert student.cell == 0
    valid = next(d for d in DIRECTIONS if room.maze.neighbor(0, d) is not None)
    expected = room.maze.neighbor(0, valid)
    room.move(student, valid, now=11)
    assert student.cell == expected
    room.move(student, valid, now=11.01)
    assert student.cell == expected  # Rate limit enforced on the server.
    original = room.projector.cell
    room.move(room.projector, valid, now=20)
    assert room.projector.cell == original
    with pytest.raises(GameError):
        room.control(student, "finish")


def test_capture_question_is_private_and_resolves_once(room):
    student = room.join("Ana")
    student.connected = room.teacher.connected = True
    room.control(room.host, "start")
    student.cell = room.teacher.cell
    student.protected_until = 0
    room.check_capture()
    q = student.question
    assert q["kind"] == "hard"
    assert room.target == student.id
    public = room.snapshot(room.projector)
    private = room.snapshot(student)
    assert public["question"] is None
    assert "answer" not in private["question"]
    assert "index" not in private["question"]
    answer = CONTENT["hard"][q["index"]]["answer"]
    room.answer(student, q["id"], answer)
    assert student.score == 20
    assert room.target is None
    assert student.cell != room.teacher.cell
    assert student.protected_until > room.elapsed
    with pytest.raises(GameError):
        room.answer(student, q["id"], answer)
    assert student.score == 20


def test_question_protection_normal_reward_and_no_farming(room):
    student = room.join("Ana")
    room.control(room.host, "start")
    student.cell = room.stations[0]
    room.interact(student)
    original = student.cell
    room.move(student, "down", now=10)
    assert student.cell == original
    room.teacher.connected = student.connected = True
    room.teacher.cell = student.cell
    student.protected_until = 0
    room.check_capture()
    assert room.target is None
    q = student.question
    room.answer(student, q["id"], CONTENT["normal"][q["index"]]["answer"])
    assert student.score == 10
    assert student.cell in student.solved
    with pytest.raises(GameError, match="Ya recuperaste"):
        room.interact(student)


def test_pause_freezes_clock_questions_and_protection(room):
    student = room.join("Ana")
    room.control(room.host, "start")
    room.ask(student, "hard")
    room.control(room.host, "pause")
    left = room.remaining
    room.update(room.last_tick + 100)
    assert room.remaining == left
    assert student.question is not None
    with pytest.raises(GameError):
        room.answer(student, student.question["id"], 0)
    room.control(room.host, "resume")
    room.update(room.last_tick + 46)
    assert student.question is None
    assert student.feedback["timeout"]
    assert student.score >= 0


def test_visibility_cannot_leak_map_or_distant_players(room):
    student = room.join("Ana")
    student.connected = room.teacher.connected = True
    snapshot = room.snapshot(student)
    assert snapshot["paths"].count(-1) > 0
    visible = room.maze.distances(student.cell, 4)
    assert all(p["cell"] in visible for p in snapshot["players"])
    assert -1 not in room.snapshot(room.host)["paths"]
    assert -1 in room.snapshot(room.projector)["paths"]


def test_time_limit_ends_room(room):
    student = room.join("Ana")
    room.control(room.host, "start")
    room.ask(student, "normal")
    room.update(room.last_tick + 61)
    assert room.status == "finished"
    assert student.question is None
    with pytest.raises(GameError):
        room.join("Tarde")
