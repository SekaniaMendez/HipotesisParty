#include "Exploration.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void TestMaze() {
  for (std::uint32_t seed = 0; seed < 60; ++seed) {
    const hp::Maze maze(13, 11, seed);
    const hp::Maze same(13, 11, seed);
    const auto distances = maze.Distances({0, 0});
    Require(distances.size() == 143, "distance size");
    Require(std::all_of(distances.begin(), distances.end(),
                        [](int d) { return d >= 0; }), "maze disconnected");
    int edges = 0;
    for (int z = 0; z < maze.Height(); ++z) {
      for (int x = 0; x < maze.Width(); ++x) {
        const hp::Cell cell{x, z};
        const auto neighbors = maze.Neighbors(cell);
        Require(neighbors == same.Neighbors(cell), "seed not deterministic");
        edges += static_cast<int>(neighbors.size());
        for (const auto next : neighbors) {
          Require(maze.CanMove(next, cell), "asymmetric wall");
        }
        const auto path = maze.ShortestPath({0, 0}, cell);
        Require(path.front() == hp::Cell{0, 0} && path.back() == cell,
                "path endpoints");
        Require(static_cast<int>(path.size()) == distances[z * 13 + x] + 1,
                "path not shortest");
        for (std::size_t i = 1; i < path.size(); ++i) {
          Require(maze.CanMove(path[i - 1], path[i]), "path crosses wall");
        }
        Require(!maze.CanMove(cell, cell), "self edge");
      }
    }
    Require(edges / 2 == 142, "generation is not a spanning tree");
    Require(!maze.IsOpen({-1, 0}) && !maze.IsOpen({13, 0}) &&
                !maze.IsOpen({0, -1}) && !maze.IsOpen({0, 11}), "boundary");
    Require(!maze.CanMove({0, 0}, {-1, 0}), "outside movement");
    Require(!maze.CanMove({0, 0}, {1, 1}), "diagonal movement");
    Require(maze.ShortestPath({0, 0}, {13, 0}).empty(), "invalid target");
    const auto invalidDistances = maze.Distances({-1, -1});
    Require(std::all_of(invalidDistances.begin(), invalidDistances.end(),
                        [](int d) { return d == -1; }), "invalid distances");
  }
  for (const auto size : {hp::Cell{1, 1}, hp::Cell{1, 11}, hp::Cell{11, 1}}) {
    const hp::Maze maze(size.x, size.z, 1);
    const auto path = maze.ShortestPath({0, 0}, {size.x - 1, size.z - 1});
    Require(static_cast<int>(path.size()) == size.x * size.z, "narrow maze");
  }
  bool threw = false;
  try { hp::Maze bad(0, 3); } catch (const std::invalid_argument&) { threw = true; }
  Require(threw, "invalid dimensions accepted");
  const hp::Maze a(13, 13, 1), b(13, 13, 2);
  bool different = false;
  for (int z = 0; z < 13; ++z) {
    for (int x = 0; x < 13; ++x) {
      different |= a.Neighbors({x, z}) != b.Neighbors({x, z});
    }
  }
  Require(different, "different seeds produce identical maze");
}

void TestEvents() {
  struct Ping { int value; };
  struct Other {};
  hp::EventBus bus;
  int originalCalls = 0, removedCalls = 0, addedCalls = 0, otherCalls = 0;
  hp::EventBus::Subscription original = 0, removed = 0;
  original = bus.Subscribe<Ping>([&](const Ping& ping) {
    ++originalCalls;
    Require(ping.value == 42, "event value lost");
    bus.Unsubscribe(original);
    bus.Unsubscribe(removed);
    bus.Subscribe<Ping>([&](const Ping&) { ++addedCalls; });
    bus.Subscribe<Other>([&](const Other&) { ++otherCalls; });
    bus.Publish(Other{});
    Require(bus.Dispatch() == 0, "reentrant dispatch");
  });
  removed = bus.Subscribe<Ping>([&](const Ping&) { ++removedCalls; });
  bus.Publish(Ping{42});
  Require(originalCalls == 0, "publish ran synchronously");
  Require(bus.Dispatch() == 1, "dispatch count");
  Require(originalCalls == 1 && removedCalls == 0 && addedCalls == 0,
          "subscriber mutation invalid");
  Require(otherCalls == 0 && bus.PendingCount() == 1, "nested event not deferred");
  bus.Dispatch();
  Require(otherCalls == 1, "deferred event missing");
  bus.Publish(Ping{5});
  bus.Publish(Ping{6});
  Require(bus.Dispatch(1) == 1 && addedCalls == 1 && bus.PendingCount() == 1,
          "bounded dispatch invalid");
  bus.Dispatch();
  Require(addedCalls == 2 && originalCalls == 1, "subscription lifetime");
  bus.Unsubscribe(999999);

  struct Fails {};
  const auto fails = bus.Subscribe<Fails>([](const Fails&) {
    throw std::runtime_error("expected callback failure");
  });
  bus.Publish(Fails{});
  try { bus.Dispatch(); } catch (const std::runtime_error&) {}
  bus.Unsubscribe(fails);
  bus.Publish(Ping{7});
  bus.Dispatch();
  Require(addedCalls == 3, "dispatch guard not reset after exception");
}

void WalkTo(hp::Exploration& game, hp::Cell target, hp::EventBus& bus) {
  const auto path = game.GetMaze().ShortestPath(game.CurrentCell(), target);
  for (const auto cell : path) {
    Require(game.EnterCell(cell), "valid movement rejected");
    bus.Dispatch();
  }
}

void TestExploration() {
  for (std::uint32_t seed = 0; seed < 40; ++seed) {
    hp::EventBus bus;
    hp::Exploration game(bus, 13, 13, seed);
    std::array<int, 6> collections{}, unlocks{};
    int completions = 0;
    bus.Subscribe<hp::GlyphCollected>([&](const hp::GlyphCollected& event) {
      Require(event.id >= 0 && event.id < 6, "glyph id");
      ++collections[event.id];
    });
    bus.Subscribe<hp::EventUnlocked>([&](const hp::EventUnlocked& event) {
      Require(!event.title.empty() && !event.description.empty(), "empty event");
      ++unlocks[event.id];
    });
    bus.Subscribe<hp::ExpeditionCompleted>([&](const auto&) { ++completions; });
    Require(game.Glyphs().size() == 6, "glyph count");
    Require(!game.EnterCell({-1, 0}), "outside movement allowed");
    Require(!game.EnterCell(game.Exit()), "teleport allowed");
    for (std::size_t i = 0; i < game.Glyphs().size(); ++i) {
      const auto& glyph = game.Glyphs()[i];
      Require(glyph.cell != game.Start() && glyph.cell != game.Exit(),
              "glyph overlaps start/exit");
      for (std::size_t j = 0; j < i; ++j) {
        Require(glyph.cell != game.Glyphs()[j].cell, "duplicate glyph position");
      }
      Require(!game.GetMaze().ShortestPath(game.Start(), glyph.cell).empty(),
              "unreachable glyph");
      WalkTo(game, glyph.cell, bus);
      game.EnterCell(glyph.cell);
      bus.Dispatch();
    }
    Require(game.CollectedCount() == 6, "missing collection");
    Require(!game.Completed(), "completed outside exit");
    WalkTo(game, game.Exit(), bus);
    game.EnterCell(game.Exit());
    bus.Dispatch();
    Require(game.Completed() && completions == 1, "completion count");
    for (int i = 0; i < 6; ++i) {
      Require(collections[i] == 1 && unlocks[i] == 1, "duplicate/missing event");
    }
    const auto previous = game.Glyphs();
    game.Regenerate(seed);
    Require(!game.Completed() && game.CollectedCount() == 0 &&
                game.CurrentCell() == game.Start(), "regeneration state");
    for (int i = 0; i < 6; ++i) {
      Require(game.Glyphs()[i].cell == previous[i].cell &&
                  !game.Glyphs()[i].collected, "regeneration seed/state");
    }
  }
  hp::EventBus bus;
  hp::Exploration smallest(bus, 2, 4, 3);
  Require(smallest.Glyphs().size() == 6, "small maze glyph count");
  bool threw = false;
  try { hp::Exploration tooSmall(bus, 2, 3); }
  catch (const std::invalid_argument&) { threw = true; }
  Require(threw, "too-small exploration accepted");
}
}  // namespace

int main() {
  try {
    TestMaze();
    TestEvents();
    TestExploration();
    std::cout << "Maze, event bus, and exploration tests passed.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Core test failure: " << error.what() << '\n';
    return 1;
  }
}
