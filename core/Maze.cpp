#include "Maze.h"

#include <algorithm>
#include <array>
#include <queue>
#include <random>
#include <stdexcept>

namespace hp {
namespace {
constexpr std::array<Cell, 4> directions{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
Cell Add(Cell a, Cell b) { return {a.x + b.x, a.z + b.z}; }
}

Maze::Maze(int width, int height, std::uint32_t seed)
    : width_(width), height_(height), seed_(seed) {
  if (width < 1 || height < 1 || width > 512 || height > 512) {
    throw std::invalid_argument("Maze dimensions must be between 1 and 512");
  }
  passages_.resize(width * height, 0);
  std::vector<bool> visited(passages_.size(), false);
  std::vector<Cell> stack{{0, 0}};
  visited[0] = true;
  std::mt19937 random(seed);
  while (!stack.empty()) {
    const Cell current = stack.back();
    std::array<int, 4> candidates{};
    unsigned count = 0;
    for (int d = 0; d < 4; ++d) {
      const Cell next = Add(current, directions[d]);
      if (IsOpen(next) && !visited[Index(next)]) candidates[count++] = d;
    }
    if (count == 0) {
      stack.pop_back();
      continue;
    }
    // Rejection sampling avoids modulo bias and implementation-dependent
    // uniform_int_distribution/shuffle behavior across native and Web builds.
    const std::uint32_t threshold = (0u - count) % count;
    std::uint32_t sample;
    do { sample = random(); } while (sample < threshold);
    const int direction = candidates[sample % count];
    const Cell next = Add(current, directions[direction]);
    passages_[Index(current)] |= 1u << direction;
    passages_[Index(next)] |= 1u << ((direction + 2) % 4);
    visited[Index(next)] = true;
    stack.push_back(next);
  }
}

bool Maze::IsOpen(Cell cell) const {
  return cell.x >= 0 && cell.z >= 0 && cell.x < width_ && cell.z < height_;
}

bool Maze::CanMove(Cell from, Cell to) const {
  if (!IsOpen(from) || !IsOpen(to)) return false;
  for (int d = 0; d < 4; ++d) {
    if (Add(from, directions[d]) == to) {
      return (passages_[Index(from)] & (1u << d)) != 0 &&
             (passages_[Index(to)] & (1u << ((d + 2) % 4))) != 0;
    }
  }
  return false;
}

std::vector<Cell> Maze::Neighbors(Cell cell) const {
  std::vector<Cell> result;
  if (!IsOpen(cell)) return result;
  for (const Cell direction : directions) {
    const Cell next = Add(cell, direction);
    if (CanMove(cell, next)) result.push_back(next);
  }
  return result;
}

std::vector<int> Maze::Distances(Cell from) const {
  std::vector<int> distances(passages_.size(), -1);
  if (!IsOpen(from)) return distances;
  std::queue<Cell> queue;
  queue.push(from);
  distances[Index(from)] = 0;
  while (!queue.empty()) {
    const Cell current = queue.front();
    queue.pop();
    for (const Cell next : Neighbors(current)) {
      if (distances[Index(next)] != -1) continue;
      distances[Index(next)] = distances[Index(current)] + 1;
      queue.push(next);
    }
  }
  return distances;
}

std::vector<Cell> Maze::ShortestPath(Cell from, Cell to) const {
  if (!IsOpen(from) || !IsOpen(to)) return {};
  const auto distances = Distances(to);
  if (distances[Index(from)] < 0) return {};
  std::vector<Cell> path{from};
  while (from != to) {
    for (const Cell next : Neighbors(from)) {
      if (distances[Index(next)] == distances[Index(from)] - 1) {
        from = next;
        path.push_back(from);
        break;
      }
    }
  }
  return path;
}

}  // namespace hp
