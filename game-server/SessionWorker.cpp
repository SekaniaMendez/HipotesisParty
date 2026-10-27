// One process owns one room. Standard output is newline-delimited JSON only.
#include "TeamSession.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>

using json = nlohmann::json;

namespace {
json Cell(hp::Cell cell) { return {{"x", cell.x}, {"z", cell.z}}; }
json Error(const char* code, const char* message) {
  return {{"ok", false}, {"error", {{"code", code}, {"message", message}}}};
}
json Assessment(const hp::FinalAssessment& result) {
  return {{"criteria", result.criteria}, {"scoreAwarded", result.scoreAwarded},
          {"expectedAlternative", result.expectedAlternative}, {"expectedZ", result.expectedZ},
          {"expectedP", result.expectedP}, {"expectedReject", result.expectedReject}};
}

class RoomSession {
 public:
  explicit RoomSession(std::uint32_t seed) : session_(seed) {}

  json Run(const json& command) {
    const auto op = command.at("op").get<std::string>();
    if (op == "add_team") {
      if (started_) return Error("lobby_closed", "La expedición ya comenzó; no se admiten equipos nuevos.");
      const int id = session_.AddTeam(command.at("name").get<std::string>(),
                                     command.at("avatarName").get<std::string>(),
                                     command.at("avatarIndex").get<int>());
      if (id < 0) return Error("team_rejected", "El nombre ya está en uso o la sala alcanzó seis equipos.");
      ++revision_;
      session_.Events().Dispatch();
      return {{"ok", true}, {"teamId", id}};
    }
    const auto role = command.at("role").get<std::string>();
    const int teamId = command.at("teamId").get<int>();
    const bool teacher = role == "teacher" && teamId == -1;
    if (!teacher && (role != "team" || !session_.FindTeam(teamId)))
      return Error("unknown_actor", "Actor desconocido.");
    if (op == "state") return {{"ok", true}, {"state", Snapshot(teacher, teamId)}};
    if (op != "action") return Error("unknown_command", "Comando desconocido.");
    const auto action = command.at("action").get<std::string>();
    if (teacher) return TeacherAction(action, command);
    if (!started_ || paused_ || Finished())
      return Error("session_inactive", "La expedición no está activa.");

    json result = json::object();
    if (action == "move") {
      const hp::Cell next{command.at("x").get<int>(), command.at("z").get<int>()};
      if (!session_.MoveTeam(teamId, next))
        return Error("move_rejected", "Necesitas movimientos y un paso adyacente sin pared.");
      feedback_.erase(teamId);
      result["moved"] = true;
    } else if (action == "easy_answer" || action == "hard_answer") {
      const auto* question = session_.PendingQuestion(teamId);
      const int questionId = command.at("questionId").get<int>();
      const int choice = command.at("choice").get<int>();
      if (!question || question->id != questionId || question->hard != (action == "hard_answer"))
        return Error("question_stale", "La pregunta ya cambió o no corresponde a este equipo.");
      const auto answer = session_.AnswerQuestion(teamId, questionId, choice);
      if (!answer.accepted) return Error("answer_rejected", "La respuesta no se puede registrar.");
      result = {{"questionId", questionId}, {"correct", answer.correct},
                {"explanation", answer.explanation}, {"movesGranted", answer.movesGranted},
                {"scoreAwarded", answer.scoreAwarded}};
      feedback_[teamId] = result;
    } else if (action == "final") {
      const double z = command.at("z").get<double>();
      const double p = command.at("p").get<double>();
      if (!std::isfinite(z) || !std::isfinite(p))
        return Error("invalid_number", "La respuesta debe contener números finitos.");
      const auto answer = session_.SubmitFinal(teamId, command.at("alternative").get<int>(),
                                               z, p, command.at("reject").get<bool>());
      if (!answer.accepted)
        return Error("final_unavailable", "Completa los diez glifos y reúne tus pistas antes de entregar.");
      result = Assessment(answer);
    } else return Error("wrong_role", "Esta acción no corresponde a un equipo.");
    ++revision_;
    session_.Events().Dispatch();
    return {{"ok", true}, {"result", result}};
  }

 private:
  hp::TeamSession session_;
  bool started_ = false;
  bool paused_ = false;
  std::uint64_t revision_ = 0;
  std::map<int, json> feedback_;

  bool Finished() const {
    const auto& teams = session_.Teams();
    return started_ && !teams.empty() && std::all_of(teams.begin(), teams.end(),
                  [](const hp::Team& team) { return team.finalSubmitted; });
  }

  json TeacherAction(const std::string& action, const json& command) {
    if (action == "start") {
      if (started_ || session_.Teams().empty())
        return Error("start_rejected", "Se necesita al menos un equipo y una sala sin iniciar.");
      started_ = true;
    } else if (action == "pause") {
      if (!started_ || paused_ || Finished())
        return Error("pause_rejected", "La expedición no está en curso.");
      paused_ = true;
    } else if (action == "resume") {
      if (!started_ || !paused_ || Finished())
        return Error("resume_rejected", "La expedición no está pausada.");
      paused_ = false;
    } else if (action == "teacher_move") {
      if (!session_.MoveTeacher({command.at("x").get<int>(), command.at("z").get<int>()}))
        return Error("move_rejected", "Elige un paso adyacente sin pared para la profesora.");
    } else return Error("wrong_role", "Esta acción no corresponde a la profesora.");
    ++revision_;
    session_.Events().Dispatch();
    return {{"ok", true}, {"result", {{"accepted", true}}}};
  }

  json Snapshot(bool teacher, int teamId) const {
    const auto& maze = session_.GetMaze();
    json state = {{"revision", revision_}, {"phase", Finished() ? "finished" :
                   (!started_ ? "lobby" : (paused_ ? "paused" : "playing"))},
                  {"role", teacher ? "teacher" : "team"}, {"teamId", teamId},
                  {"width", maze.Width()}, {"height", maze.Height()},
                  {"seed", teacher ? json(maze.Seed()) : json(nullptr)},
                  {"teacher", nullptr}, {"teams", json::array()},
                  {"cells", json::array()}, {"visible", json::array()},
                  {"objects", json::array()}, {"question", nullptr},
                  {"feedback", nullptr}, {"own", nullptr}};
    const auto visible = [&](hp::Cell cell) { return teacher || session_.IsVisible(teamId, cell); };
    if (visible(session_.TeacherCell())) state["teacher"] = Cell(session_.TeacherCell());
    for (const auto& team : session_.Teams()) {
      state["teams"].push_back({{"id", team.id}, {"name", team.name},
          {"avatarName", team.avatarName}, {"avatarIndex", team.avatarIndex},
          {"cell", (team.id == teamId || visible(team.cell)) ? Cell(team.cell) : json(nullptr)},
          {"score", team.score}, {"hardAnswered", team.hardAnswered},
          {"clueCount", team.clues.size()}, {"finished", team.finalSubmitted}});
    }
    constexpr std::array<hp::Cell, 4> directions{{{0,-1},{1,0},{0,1},{-1,0}}};
    for (int z = 0; z < maze.Height(); ++z) for (int x = 0; x < maze.Width(); ++x) {
      const hp::Cell cell{x,z};
      if (teacher || session_.IsDiscovered(teamId, cell)) {
        int open = 0;
        for (int i = 0; i < 4; ++i)
          if (maze.CanMove(cell, {x + directions[i].x, z + directions[i].z})) open |= 1 << i;
        state["cells"].push_back({{"x", x}, {"z", z}, {"open", open}});
      }
      if (visible(cell)) state["visible"].push_back(Cell(cell));
    }
    for (const auto& glyph : session_.Glyphs()) {
      if (teacher || (glyph.owner == teamId && !glyph.answered && visible(glyph.cell)))
        state["objects"].push_back({{"kind", "glyph"}, {"id", glyph.id},
              {"owner", glyph.owner}, {"x", glyph.cell.x}, {"z", glyph.cell.z},
              {"resolved", glyph.answered}});
    }
    for (const auto& clue : session_.Clues()) {
      if (teacher || (clue.owner == teamId && !clue.collected && visible(clue.cell)))
        state["objects"].push_back({{"kind", "clue"}, {"id", clue.id},
              {"owner", clue.owner}, {"x", clue.cell.x}, {"z", clue.cell.z},
              {"resolved", clue.collected}});
    }
    if (!teacher) {
      const auto& team = *session_.FindTeam(teamId);
      const auto challenge = session_.Challenge(teamId);
      json own = {{"id", teamId}, {"moves", team.moves}, {"score", team.score},
          {"easyAnswered", team.easyAnswered}, {"hardAnswered", team.hardAnswered},
          {"clues", json::array()}, {"finalSubmitted", team.finalSubmitted},
          {"canSubmitFinal", session_.CanSubmitFinal(teamId)}, {"assessment", nullptr},
          {"hypothesis", {{"id", teamId}, {"title", challenge.title},
            {"statement", "Reúne tus cuatro pistas para formular la hipótesis y calcular z, p y la decisión."},
            {"alternatives", challenge.alternatives}}}};
      for (const auto& clue : session_.Clues())
        if (clue.owner == teamId && clue.collected)
          own["clues"].push_back({{"id", clue.id}, {"title", clue.title}, {"text", clue.text}});
      if (const auto* assessment = session_.FinalResult(teamId)) own["assessment"] = Assessment(*assessment);
      state["own"] = std::move(own);
      if (started_ && !team.finalSubmitted) {
        if (const auto* question = session_.PendingQuestion(teamId))
          state["question"] = {{"id", question->id}, {"kind", question->hard ? "hard" : "easy"},
                    {"title", question->title}, {"prompt", question->prompt}, {"options", question->options}};
      }
      if (const auto found = feedback_.find(teamId); found != feedback_.end()) state["feedback"] = found->second;
    }
    return state;
  }
};
}  // namespace

int main(int argc, char** argv) {
  std::uint32_t seed = 20261027u;
  try {
    if (argc == 3 && std::string(argv[1]) == "--seed") seed = static_cast<std::uint32_t>(std::stoul(argv[2]));
    RoomSession room(seed);
    std::string line;
    while (std::getline(std::cin, line)) {
      try {
        if (line.size() > 16 * 1024) {
          std::cout << Error("invalid_command", "Comando demasiado grande.").dump() << std::endl;
          continue;
        }
        std::cout << room.Run(json::parse(line)).dump() << std::endl;
      } catch (const std::exception&) {
        std::cout << Error("invalid_command", "Comando inválido.").dump() << std::endl;
      }
    }
  } catch (const std::exception&) {
    std::cerr << "SessionWorker: initialization failed\n";
    return 1;
  }
}
