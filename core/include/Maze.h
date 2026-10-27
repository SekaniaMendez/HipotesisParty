#pragma once

#include <cstdint>
#include <vector>

namespace hp {

struct Cell {
  int x = 0;
  int z = 0;
  bool operator==(const Cell&) const = default;
};

// Cells are floors; walls lie between cells. Iterative randomized DFS follows
// the DeepSearch generation approach in treasure_labyrinth, with local seeded
// randomness and value storage so the core also runs without graphics.
class Maze {
 public:
  explicit Maze(int width = 13, int height = 13,
                std::uint32_t seed = 20261027u);
  int Width() const { return width_; }
  int Height() const { return height_; }
  std::uint32_t Seed() const { return seed_; }
  bool IsOpen(Cell cell) const;
  bool CanMove(Cell from, Cell to) const;
  std::vector<Cell> Neighbors(Cell cell) const;
  // Includes both endpoints; empty if either endpoint is outside the maze.
  std::vector<Cell> ShortestPath(Cell from, Cell to) const;
  // Row-major distances (index = z * Width() + x); unreachable = -1.
  // BFS is Dijkstra specialized to this maze's uniform edge cost of one.
  std::vector<int> Distances(Cell from) const;

 private:
  int width_;
  int height_;
  std::uint32_t seed_;
  std::vector<std::uint8_t> passages_;
  int Index(Cell cell) const { return cell.z * width_ + cell.x; }
};

}  // namespace hp
