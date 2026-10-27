#include "TeamSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

namespace {
void Check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void Resolve(hp::TeamSession& session, int team) {
  while (const auto* question = session.PendingQuestion(team)) {
    const auto answer = session.AnswerQuestion(team, question->id, question->correct);
    Check(answer.accepted && answer.correct, "valid question answer rejected");
  }
  session.Events().Dispatch();
}

void WalkTo(hp::TeamSession& session, int team, hp::Cell destination,
            bool resolveLast = true) {
  const auto path = session.GetMaze().ShortestPath(session.FindTeam(team)->cell, destination);
  Check(!path.empty(), "objective is unreachable");
  for (std::size_t i = 1; i < path.size(); ++i) {
    Resolve(session, team);
    Check(session.MoveTeam(team, path[i]), "legal move rejected");
    session.Events().Dispatch();
  }
  if (resolveLast) Resolve(session, team);
}

void CompleteObjectives(hp::TeamSession& session, int team) {
  std::vector<hp::Cell> targets;
  for (const auto& glyph : session.Glyphs()) {
    if (glyph.owner == team) targets.push_back(glyph.cell);
  }
  for (const auto& clue : session.Clues()) {
    if (clue.owner == team) targets.push_back(clue.cell);
  }
  for (const auto cell : targets) WalkTo(session, team, cell);
  Check(session.FindTeam(team)->hardAnswered == 10, "ten hard questions not answered");
  Check(session.FindTeam(team)->clues.size() == 4, "four clues not collected");
  Check(session.CanSubmitFinal(team), "completed team cannot submit final");
}

void TestTeamsAndInventory() {
  hp::TeamSession session(72);
  Check(session.AddTeam("  ", "Avatar", 0) == -1, "empty team accepted");
  Check(session.AddTeam("Team", "", 0) == -1, "empty avatar accepted");
  Check(session.AddTeam("Team", "Avatar", 6) == -1, "invalid avatar accepted");
  Check(session.AddTeam("A\nB", "Avatar", 0) == -1, "control characters accepted");
  Check(session.AddTeam(std::string(65, 'x'), "Avatar", 0) == -1, "overlong name accepted");
  Check(session.AddTeam("  Física  ", "  Luna  ", 0) == 0, "team add failed");
  Check(session.FindTeam(0)->name == "Física" && session.FindTeam(0)->avatarName == "Luna",
        "name trim failed");
  Check(session.AddTeam("Física", "Else", 1) == -1, "duplicate team accepted");
  for (int i = 1; i < 6; ++i) {
    Check(session.AddTeam("Team " + std::to_string(i), "Avatar " + std::to_string(i), i) == i,
          "team id sequence");
  }
  Check(session.AddTeam("Seventh", "Avatar", 0) == -1, "seventh team accepted");
  Check(session.Glyphs().size() == 60 && session.Clues().size() == 24,
        "exclusive objective counts");
  std::set<int> locations, ids;
  for (const auto& question : session.Questions()) {
    Check(ids.insert(question.id).second, "duplicate occurrence id");
    Check(question.correct >= 0 && question.correct < 3 && !question.explanation.empty(),
          "invalid question bank entry");
  }
  for (const auto& glyph : session.Glyphs()) {
    Check(locations.insert(glyph.cell.z * 13 + glyph.cell.x).second, "overlapping objectives");
    Check(glyph.cell != hp::Cell{0, 0} && session.GetMaze().IsOpen(glyph.cell), "glyph location");
    Check(session.Questions()[glyph.questionIndex].hard, "glyph has easy question");
  }
  for (const auto& clue : session.Clues()) {
    Check(locations.insert(clue.cell.z * 13 + clue.cell.x).second, "overlapping clue");
    Check(!clue.text.empty(), "empty clue text");
  }
  Check(!session.FindTeam(-1) && !session.FindTeam(6), "invalid team lookup");
  Check(!session.MoveTeam(-1, {0, 1}) && !session.PendingQuestion(99), "invalid team accepted");
  Check(!session.SubmitFinal(0, 0, 0, 0, true).accepted, "premature final accepted");
  Check(!session.FinalResult(0), "answer disclosed before submission");
  hp::TeamSession narrow(1, 3, 5);
  Check(narrow.AddTeam("Only", "One", 0) == 0 && narrow.AddTeam("No room", "Two", 1) == -1,
        "small maze capacity");
}

void TestBudgetsAndReplays() {
  hp::TeamSession session(123);
  session.AddTeam("A", "Ana", 0);
  session.AddTeam("B", "Bruno", 3);
  const auto start = session.FindTeam(0)->cell;
  const auto next = session.GetMaze().Neighbors(start).front();
  const auto easy = *session.PendingQuestion(0);
  Check(!easy.hard && !session.MoveTeam(0, next), "movement before easy answer");
  Check(!session.AnswerQuestion(0, easy.id, 3).accepted, "invalid option accepted");
  Check(!session.AnswerQuestion(1, easy.id, easy.correct).accepted, "foreign question accepted");
  const auto correct = session.AnswerQuestion(0, easy.id, easy.correct);
  Check(correct.accepted && correct.movesGranted == 5 && correct.scoreAwarded == 0,
        "correct easy reward must be five moves and no score");
  Check(!session.AnswerQuestion(0, easy.id, easy.correct).accepted, "replayed easy accepted");
  Check(!session.MoveTeam(0, {-1, -1}) && session.FindTeam(0)->moves == 5,
        "denied move spent budget");
  Check(!session.MoveTeam(0, start) && session.FindTeam(0)->moves == 5, "same cell spent budget");
  auto cell = start;
  for (int i = 0; i < 5; ++i) {
    const auto destination = cell == start ? next : start;
    Check(session.MoveTeam(0, destination), "budgeted move rejected");
    cell = destination;
    if (const auto* pending = session.PendingQuestion(0); pending && pending->hard) {
      session.AnswerQuestion(0, pending->id, pending->correct);
    }
  }
  Check(session.FindTeam(0)->moves == 0 && !session.MoveTeam(0, start), "sixth move allowed");
  const auto secondEasy = *session.PendingQuestion(0);
  Check(!secondEasy.hard && secondEasy.id != easy.id, "easy occurrence not renewed");
  Check(!session.AnswerQuestion(0, easy.id, easy.correct).accepted, "stale easy id accepted");
  const auto wrong = session.AnswerQuestion(0, secondEasy.id, (secondEasy.correct + 1) % 3);
  Check(wrong.accepted && !wrong.correct && wrong.movesGranted == 2,
        "incorrect easy must award two moves");
  Check(session.FindTeam(0)->easyAnswered == 2 && session.FindTeam(1)->moves == 0 &&
        session.FindTeam(1)->easyAnswered == 0, "teams share budgets");
}

void TestFogAndTeacher() {
  hp::TeamSession session(351);
  session.AddTeam("A", "Ana", 0);
  session.AddTeam("B", "Bruno", 3);
  const auto initialDistances = session.GetMaze().Distances({0, 0});
  int visible = 0;
  bool wallBlocks = false;
  for (int z = 0; z < 13; ++z) {
    for (int x = 0; x < 13; ++x) {
      const hp::Cell cell{x, z};
      Check(session.IsVisible(0, cell) == (initialDistances[z * 13 + x] <= 2),
            "fog uses straight-line visibility through a wall");
      Check(session.IsDiscovered(0, cell) == session.IsVisible(0, cell), "initial discovered mismatch");
      if (session.IsVisible(0, cell)) ++visible;
      if (x + z <= 2 && initialDistances[z * 13 + x] > 2) wallBlocks = true;
    }
  }
  Check(visible < 13 && wallBlocks, "fog reveals too much or does not block walls");
  const hp::Cell distant = session.Glyphs().front().cell;
  Check(session.KnownRoute(0, {0, 0}, distant).empty(), "route disclosed hidden maze");
  std::vector<bool> discovered(169);
  for (int i = 0; i < 169; ++i) discovered[i] = session.IsDiscovered(0, {i % 13, i / 13});
  WalkTo(session, 0, distant);
  const auto newDistances = session.GetMaze().Distances(distant);
  for (int i = 0; i < 169; ++i) {
    const hp::Cell cell{i % 13, i / 13};
    Check(session.IsVisible(0, cell) == (newDistances[i] <= 2), "moved visibility radius");
    Check(!discovered[i] || session.IsDiscovered(0, cell), "explored fog was forgotten");
    Check(session.IsDiscovered(1, cell) == (initialDistances[i] <= 2), "fog leaked across teams");
  }
  const auto known = session.KnownRoute(0, {0, 0}, distant);
  Check(!known.empty(), "known route absent");
  for (const auto cell : known) Check(session.IsDiscovered(0, cell), "known route crosses hidden cell");
  const auto cluesBefore = session.FindTeam(1)->clues.size();
  const auto glyphsBefore = session.FindTeam(1)->hardAnswered;
  const auto teacherRoute = session.GetMaze().ShortestPath(session.TeacherCell(), session.Clues()[4].cell);
  for (std::size_t i = 1; i < teacherRoute.size(); ++i) {
    Check(session.MoveTeacher(teacherRoute[i]), "teacher movement blocked by budget");
  }
  Check(!session.MoveTeacher({-1, 0}), "teacher crosses world bounds");
  Check(session.FindTeam(1)->clues.size() == cluesBefore &&
        session.FindTeam(1)->hardAnswered == glyphsBefore &&
        session.FindTeam(1)->moves == 0, "teacher collected or spent team objectives");
}

void TestOwnershipAndHardQuestions() {
  hp::TeamSession session(104);
  session.AddTeam("A", "Ana", 0);
  session.AddTeam("B", "Bruno", 3);
  const auto foreignClue = session.Clues()[4];
  WalkTo(session, 0, foreignClue.cell);
  Check(!session.Clues()[4].collected, "foreign clue collected");
  Check(session.FindTeam(1)->clues.empty(), "other team received a clue");
  const auto foreignGlyph = session.Glyphs()[10];
  WalkTo(session, 0, foreignGlyph.cell);
  Check(!session.Glyphs()[10].answered && session.FindTeam(1)->hardAnswered == 0,
        "foreign glyph activated");
  const auto ownGlyph = *std::find_if(session.Glyphs().begin(), session.Glyphs().end(),
      [](const hp::TeamGlyph& g) { return g.owner == 1; });
  WalkTo(session, 1, ownGlyph.cell, false);
  const auto pending = *session.PendingQuestion(1);
  Check(pending.hard, "owned glyph did not activate hard question");
  const int budget = session.FindTeam(1)->moves;
  const auto neighbor = session.GetMaze().Neighbors(ownGlyph.cell).front();
  Check(!session.MoveTeam(1, neighbor) && session.FindTeam(1)->moves == budget,
        "pending hard question did not block movement");
  const int previousScore = session.FindTeam(1)->score;
  const int previousHard = session.FindTeam(1)->hardAnswered;
  const auto result = session.AnswerQuestion(1, pending.id, (pending.correct + 1) % 3);
  Check(result.accepted && !result.correct && result.scoreAwarded == 0 && result.movesGranted == 0,
        "incorrect hard reward");
  Check(session.FindTeam(1)->score == previousScore &&
        session.FindTeam(1)->hardAnswered == previousHard + 1 &&
        session.FindTeam(1)->moves == budget, "incorrect hard state");
  Check(!session.AnswerQuestion(1, pending.id, pending.correct).accepted, "hard second attempt accepted");
  WalkTo(session, 1, neighbor);
  WalkTo(session, 1, ownGlyph.cell);
  Check(!session.PendingQuestion(1) || !session.PendingQuestion(1)->hard,
        "answered glyph reactivated");
}

void TestFinalAssessmentAndMath() {
  hp::TeamSession session(9876);
  constexpr std::array<int, 6> alternative{0, 2, 1, 2, 0, 1};
  constexpr std::array<double, 6> z{-8.0 / 3.0, 2, 2, 2, -1, 1};
  constexpr std::array<double, 6> p{0.0038303806, 0.0227501319, 0.0455002639,
                                    0.0227501319, 0.1586552539, 0.3173105079};
  constexpr std::array<bool, 6> reject{true, false, true, true, false, false};
  for (int i = 0; i < 6; ++i) session.AddTeam("Team " + std::to_string(i), "Avatar", i);
  int finalEvents = 0;
  session.Events().Subscribe<hp::TeamFinalSubmitted>([&](const auto&) { ++finalEvents; });
  for (int i = 0; i < 6; ++i) {
    CompleteObjectives(session, i);
    Check(session.FindTeam(i)->score == 1000, "hard total must be 1000");
    Check(!session.SubmitFinal(i, alternative[i], std::numeric_limits<double>::quiet_NaN(),
                              p[i], reject[i]).accepted, "NaN final accepted");
    Check(!session.SubmitFinal(i, alternative[i], z[i], 1.1, reject[i]).accepted,
          "invalid probability accepted");
    const auto result = session.SubmitFinal(i, alternative[i], z[i], p[i], reject[i]);
    Check(result.accepted && result.scoreAwarded == 400, "correct final did not award 400");
    Check(std::abs(result.expectedZ - z[i]) < 1e-10 &&
          std::abs(result.expectedP - p[i]) < 1e-9 &&
          result.expectedAlternative == alternative[i] && result.expectedReject == reject[i],
          "known-sigma hypothesis math differs from expected");
    Check(session.FindTeam(i)->score == 1400 && session.FindTeam(i)->finalSubmitted,
          "final score not 1400");
    Check(!session.CanSubmitFinal(i) && session.FinalResult(i) &&
          !session.SubmitFinal(i, alternative[i], z[i], p[i], reject[i]).accepted,
          "final scoring replay");
    Check(!session.MoveTeam(i, session.GetMaze().Neighbors(session.FindTeam(i)->cell).front()),
          "finished team moved");
    Check(!session.PendingQuestion(i), "finished team gets questions");
    session.Events().Dispatch();
  }
  Check(finalEvents == 6, "duplicate final event");

  hp::TeamSession partial(65);
  partial.AddTeam("Partial", "Avatar", 0);
  CompleteObjectives(partial, 0);
  const double exactLeftP = 0.5 * std::erfc(-z[0] / std::sqrt(2.0));
  const auto result = partial.SubmitFinal(0, 2, z[0] + 0.03, exactLeftP + 0.0005, false);
  Check(result.accepted && result.scoreAwarded == 200 &&
        !result.criteria[0] && result.criteria[1] && result.criteria[2] && !result.criteria[3],
        "partial criteria/tolerance boundary incorrect");
  Check(partial.FindTeam(0)->score == 1200, "partial final total");

  hp::TeamSession probability(71);
  for (int i = 0; i < 3; ++i) probability.AddTeam("T" + std::to_string(i), "Avatar", i);
  CompleteObjectives(probability, 2);
  const auto crossedAlpha = probability.SubmitFinal(2, 1, 2, 0.0504, true);
  Check(crossedAlpha.accepted && crossedAlpha.scoreAwarded == 300 &&
        !crossedAlpha.criteria[2] && crossedAlpha.criteria[3],
        "imprecise p crossing alpha received numeric credit");
}
}  // namespace

int main() {
  try {
    TestTeamsAndInventory();
    TestBudgetsAndReplays();
    TestFogAndTeacher();
    TestOwnershipAndHardQuestions();
    TestFinalAssessmentAndMath();
    std::cout << "Team session tests passed: ownership, fog, movement, questions, six hypotheses and final scoring.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Team session test failure: " << error.what() << '\n';
    return 1;
  }
}
