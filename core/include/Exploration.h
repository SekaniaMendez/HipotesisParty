#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EventBus.h"
#include "Maze.h"

namespace hp {

struct GlyphCollected { int id; };
struct EventUnlocked { int id; std::string title; std::string description; };
struct ExpeditionCompleted {};

struct Glyph {
  int id;
  Cell cell;
  bool collected = false;
  std::string title;
  std::string description;
};

class Exploration {
 public:
  explicit Exploration(EventBus& events, int width = 13, int height = 13,
                       std::uint32_t seed = 20261027u);
  const Maze& GetMaze() const { return maze_; }
  Cell Start() const { return {0, 0}; }
  Cell Exit() const { return exit_; }
  Cell CurrentCell() const { return current_; }
  const std::vector<Glyph>& Glyphs() const { return glyphs_; }
  // Returns false on a blocked/non-adjacent move. Re-entering the current cell
  // is allowed but collection/completion events are only emitted once.
  bool EnterCell(Cell cell);
  int CollectedCount() const { return collected_; }
  bool Completed() const { return completed_; }
  // Dispatch pending events before regeneration; this doesn't clear unrelated
  // events from the shared bus. Glyph state is reset and the seed is preserved.
  void Regenerate(std::uint32_t seed);

 private:
  EventBus& events_;
  Maze maze_;
  Cell exit_{};
  Cell current_{};
  std::vector<Glyph> glyphs_;
  int collected_ = 0;
  bool completed_ = false;
  void PlaceGlyphs();
};

}  // namespace hp
