#include "Exploration.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace hp {
namespace {
constexpr std::array<const char*, 6> titles{
    "La pregunta", "La hipotesis", "El experimento", "La evidencia",
    "El debate", "La conclusion"};
constexpr std::array<const char*, 6> descriptions{
    "Planteen una pregunta sobre lo aprendido en CI-0115. Cada integrante aporta una idea.",
    "Propongan una respuesta y expliquen que resultado permitiria comprobarla.",
    "Disenen en equipo una prueba sencilla para evaluar su hipotesis.",
    "Compartan un ejemplo concreto del curso que apoye o cuestione su respuesta.",
    "Comparen dos soluciones. Que ventajas, limites y dudas encuentran en cada una?",
    "Resuman su hallazgo y una mejora para el siguiente reto. Regresen al portal final."};
}

Exploration::Exploration(EventBus& events, int width, int height,
                         std::uint32_t seed)
    : events_(events), maze_(width, height, seed) {
  if (width * height < 8) {
    throw std::invalid_argument("Exploration needs at least eight cells");
  }
  PlaceGlyphs();
}

void Exploration::PlaceGlyphs() {
  glyphs_.clear();
  collected_ = 0;
  completed_ = false;
  current_ = Start();
  auto minDistances = maze_.Distances(Start());
  const int exitIndex = static_cast<int>(std::max_element(
      minDistances.begin(), minDistances.end()) - minDistances.begin());
  exit_ = {exitIndex % maze_.Width(), exitIndex / maze_.Width()};
  const auto exitDistances = maze_.Distances(exit_);
  for (std::size_t i = 0; i < minDistances.size(); ++i) {
    minDistances[i] = std::min(minDistances[i], exitDistances[i]);
  }
  for (int id = 0; id < 6; ++id) {
    const int chosen = static_cast<int>(std::max_element(
        minDistances.begin(), minDistances.end()) - minDistances.begin());
    const Cell cell{chosen % maze_.Width(), chosen / maze_.Width()};
    glyphs_.push_back({id, cell, false, titles[id], descriptions[id]});
    const auto distances = maze_.Distances(cell);
    for (std::size_t i = 0; i < minDistances.size(); ++i) {
      minDistances[i] = std::min(minDistances[i], distances[i]);
    }
  }
  // Keep the route's educational progression readable while retaining the
  // spread selected above: IDs follow increasing distance from the start.
  const auto startDistances = maze_.Distances(Start());
  std::sort(glyphs_.begin(), glyphs_.end(), [&](const Glyph& a, const Glyph& b) {
    return startDistances[a.cell.z * maze_.Width() + a.cell.x] <
           startDistances[b.cell.z * maze_.Width() + b.cell.x];
  });
  for (int id = 0; id < 6; ++id) {
    glyphs_[id].id = id;
    glyphs_[id].title = titles[id];
    glyphs_[id].description = descriptions[id];
  }
}

bool Exploration::EnterCell(Cell cell) {
  if (!maze_.IsOpen(cell) ||
      (cell != current_ && !maze_.CanMove(current_, cell))) return false;
  current_ = cell;
  for (Glyph& glyph : glyphs_) {
    if (glyph.collected || glyph.cell != cell) continue;
    glyph.collected = true;
    ++collected_;
    events_.Publish(GlyphCollected{glyph.id});
    events_.Publish(EventUnlocked{glyph.id, glyph.title, glyph.description});
  }
  if (!completed_ && collected_ == static_cast<int>(glyphs_.size()) &&
      current_ == exit_) {
    completed_ = true;
    events_.Publish(ExpeditionCompleted{});
  }
  return true;
}

void Exploration::Regenerate(std::uint32_t seed) {
  maze_ = Maze(maze_.Width(), maze_.Height(), seed);
  PlaceGlyphs();
}

}  // namespace hp
