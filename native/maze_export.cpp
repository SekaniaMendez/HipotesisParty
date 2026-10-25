// Adapter around the original Treasure Labyrinth DFS/BFS implementations (MIT).
// FastAPI invokes this native tool once per room; raylib is not needed by the server.
#include <treasure_labyrinth/algorithms/DeepSearch.hpp>
#include <treasure_labyrinth/algorithms/BreadthSearch.hpp>
#include <treasure_labyrinth/algorithms/Node.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

int main(int argc, char** argv) {
  const int width = argc > 1 ? std::atoi(argv[1]) : 18;
  const int height = argc > 2 ? std::atoi(argv[2]) : 14;
  if (width < 4 || height < 4 || width > 64 || height > 64) return 1;
  std::vector<std::unique_ptr<Node>> nodes;
  std::vector<int> paths(width * height, 0);
  for (int id = 0; id < width * height; ++id)
    nodes.push_back(std::make_unique<Node>(id, id / width, id % width));
  auto neighbors = [&](Node* n) {
    std::vector<Node*> result;
    const int x = n->getCol(), y = n->getRow();
    if (y > 0) result.push_back(nodes[(y-1)*width+x].get());
    if (x+1 < width) result.push_back(nodes[y*width+x+1].get());
    if (y+1 < height) result.push_back(nodes[(y+1)*width+x].get());
    if (x > 0) result.push_back(nodes[y*width+x-1].get());
    return result;
  };
  auto open = [&](Node* from, Node* to) {
    int bit = 0, opposite = 0;
    if (to->getRow() < from->getRow()) { bit=1; opposite=4; }
    else if (to->getCol() > from->getCol()) { bit=2; opposite=8; }
    else if (to->getRow() > from->getRow()) { bit=4; opposite=1; }
    else { bit=8; opposite=2; }
    paths[from->getId()] |= bit;
    paths[to->getId()] |= opposite;
  };
  if (argc > 3 && std::string(argv[3]) == "bfs")
    BreadthSearch::generatePath(nodes[0].get(), neighbors, open);
  else DeepSearch::generatePath(nodes[0].get(), neighbors, open);
  // Additional reciprocal passages, as in Maze::applyExtraEdges, for escape routes.
  std::mt19937 random(std::random_device{}());
  std::bernoulli_distribution extra(0.18);
  for (auto& n : nodes) {
    for (Node* other : neighbors(n.get())) {
      if (other->getId() > n->getId() && extra(random)) open(n.get(), other);
    }
  }
  std::cout << "{\"width\":" << width << ",\"height\":" << height << ",\"paths\":[";
  for (size_t i = 0; i < paths.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << paths[i];
  }
  std::cout << "]}";
}
