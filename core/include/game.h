#pragma once
#include <cstdint>
#include <memory>
namespace hp { class PartyGame; }
// GPU resources live between InitWindow and CloseWindow.
class Game {
 public:
  explicit Game(std::uint32_t seed = 20261027u, bool smokeTest = false);
  ~Game();
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;
  void UpdateDrawFrame();
  void drawFrame();
  bool ShouldQuit() const;
  int ExitCode() const;
 private:
  std::unique_ptr<hp::PartyGame> impl_;
};
