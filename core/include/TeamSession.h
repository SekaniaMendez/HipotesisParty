#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "EventBus.h"
#include "Maze.h"

namespace hp {

struct Question {
  int id = -1;
  bool hard = false;
  std::string title;
  std::string prompt;
  std::array<std::string, 3> options;
  // Authority-only fields: NEVER include these in an unanswered client view.
  int correct = 0;
  std::string explanation;
};

struct Team {
  int id = -1;
  std::string name;
  std::string avatarName;
  int avatarIndex = 0;
  Cell cell{};
  int moves = 0;
  int score = 0;
  int easyAnswered = 0;
  int hardAnswered = 0;
  std::vector<int> clues;
  bool finalSubmitted = false;
};

struct TeamGlyph {
  int id = -1;
  int owner = -1;
  Cell cell{};
  int questionIndex = -1;  // Index into TeamSession::Questions().
  bool answered = false;
};

struct TeamClue {
  int id = -1;
  int owner = -1;
  Cell cell{};
  std::string title;
  std::string text;  // Send to owner only AFTER collected.
  bool collected = false;
};

struct AnswerResult {
  bool accepted = false;
  bool correct = false;
  int movesGranted = 0;
  int scoreAwarded = 0;
  std::string explanation;
};

struct FinalAssessment {
  bool accepted = false;
  std::array<bool, 4> criteria{};  // H1, z, p, decision: 100 points each.
  int scoreAwarded = 0;
  int expectedAlternative = -1;
  double expectedZ = 0;
  double expectedP = 0;
  bool expectedReject = false;
};

struct FinalChallenge {
  std::string title;
  // Indices are always 0=less, 1=two-sided, 2=greater.
  std::array<std::string, 3> alternatives;
};

struct TeamJoined { int teamId; };
struct TeamMoved { int teamId; Cell cell; };
struct TeamQuestionAnswered { int teamId; int questionId; bool correct; int scoreAwarded; };
struct TeamClueCollected { int teamId; int clueId; };
struct TeamFinalSubmitted { int teamId; int scoreAwarded; };
struct TeacherMoved { Cell cell; };

// Pure authoritative game rules. The network host owns lobby/start/pause and
// authentication; call these mutation methods only for an authenticated actor.
// Only send discovered maze walls, the requesting team's visible objects, and
// public question fields. This object intentionally holds all answer secrets.
class TeamSession {
 public:
  explicit TeamSession(std::uint32_t seed, int width = 13, int height = 13);
  TeamSession(const TeamSession&) = delete;
  TeamSession& operator=(const TeamSession&) = delete;

  // IDs 0..5; -1 for invalid/duplicate names, invalid avatar index (0..5),
  // capacity reached, or insufficient cells for fourteen new exclusive items.
  int AddTeam(std::string name, std::string avatarName, int avatarIndex);
  const Maze& GetMaze() const { return maze_; }
  const std::vector<Team>& Teams() const { return teams_; }
  const std::vector<TeamGlyph>& Glyphs() const { return glyphs_; }
  const std::vector<TeamClue>& Clues() const { return clues_; }
  const std::vector<Question>& Questions() const { return questions_; }
  const Team* FindTeam(int teamId) const;

  // Pointer lifetime ends at next mutating session operation.
  // hard=true blocks movement until answered. Easy appears at zero moves.
  const Question* PendingQuestion(int teamId) const;
  bool MoveTeam(int teamId, Cell destination);
  AnswerResult AnswerQuestion(int teamId, int expectedQuestionId, int optionIndex);

  bool IsDiscovered(int teamId, Cell cell) const;
  bool IsVisible(int teamId, Cell cell) const;
  std::vector<Cell> KnownRoute(int teamId, Cell from, Cell to) const;

  Cell TeacherCell() const { return teacher_; }
  bool MoveTeacher(Cell destination);
  FinalChallenge Challenge(int teamId) const;
  bool CanSubmitFinal(int teamId) const;
  // Numeric tolerance: z ±0.03, p ±0.0005 (report p to at least 3 decimals).
  // The decision is independently compared with the exact erfc probability.
  FinalAssessment SubmitFinal(int teamId, int alternativeIndex, double z,
                              double p, bool reject);
  const FinalAssessment* FinalResult(int teamId) const;
  EventBus& Events() { return events_; }

 private:
  struct Progress {
    std::vector<bool> discovered;
    std::vector<bool> visible;
    int easyQuestionIndex = -1;
    int pendingGlyphIndex = -1;
    std::optional<FinalAssessment> final;
  };
  Maze maze_;
  EventBus events_;
  std::vector<Team> teams_;
  std::vector<Progress> progress_;
  std::vector<TeamGlyph> glyphs_;
  std::vector<TeamClue> clues_;
  std::vector<Question> questions_;
  Cell teacher_{};
  int nextQuestionId_ = 1;

  bool ValidTeam(int teamId) const;
  int Index(Cell cell) const { return cell.z * maze_.Width() + cell.x; }
  void Reveal(int teamId);
  void QueueEasy(int teamId);
  void CollectAt(int teamId);
};

}  // namespace hp
