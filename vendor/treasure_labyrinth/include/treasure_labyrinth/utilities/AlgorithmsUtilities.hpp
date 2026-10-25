/**
 * @file AlgorithmsUtilities.hpp
 * @brief Header file for utility functions related to algorithms used in the Treasure Labyrinth
 * project. This file contains declarations of utility functions that assist in various algorithmic
 * operations, such as random selection and pathfinding.
 * @author Giovanni Mendez Sanchez
 * @date 09-05-26
 * @version 1.0
 */

#pragma once

#include <algorithm>
#include <functional>
#include <ostream>
#include <stack>
#include <stdexcept>
#include <treasure_labyrinth/algorithms/Node.hpp>
#include <treasure_labyrinth/utilities/inlineFunctions.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

template <typename FrontierType>
struct SearchContext {
  FrontierType frontier;              // will be used to keep track of the nodes to explore
  std::unordered_set<Node*> visited;  // will keep track of visited nodes to avoid cycles
  std::unordered_map<Node*, Node*>
      parentMap;          // map each node to its parent for path reconstruction
  std::vector<int> path;  // hold the path from the start node to the target node
};

class AlgorithmsUtilities {
 public:
  /**
   * @brief Gets only the neighbors that have not been visited yet.
   * @param currentNode Current node being evaluated.
   * @param getNeighbors Function used to retrieve the neighbors of a node.
   * @param visited Set containing the nodes that were already visited.
   * @return Vector containing only unvisited neighbors.
   */
  static inline std::vector<Node*> getUnvisitedNeighbors(
      Node* currentNode, const std::function<std::vector<Node*>(Node*)>& getNeighbors,
      const std::unordered_set<Node*>& visited) {
    tl::utilities::validateArguments(currentNode, getNeighbors);

    std::vector<Node*> neighbors = getNeighbors(currentNode);
    std::vector<Node*> unvisitedNeighbors;

    for (Node* neighbor : neighbors) {
      if (visited.find(neighbor) == visited.end()) {
        unvisitedNeighbors.push_back(neighbor);
      }
    }

    return unvisitedNeighbors;
  }

  /**
   * @brief Reconstructs a path from a parent map.
   * @param startNode First node in the path.
   * @param targetNode Last node in the path.
   * @param parentMap Map where each node points to its parent node.
   * @return Ordered path from start node to target node using node ids.
   */
  static inline std::vector<int> reconstructPath(
      Node* startNode, Node* targetNode, const std::unordered_map<Node*, Node*>& parentMap) {
    tl::utilities::validateArguments(startNode, targetNode);

    std::vector<int> path;

    for (Node* node = targetNode; node != startNode; node = parentMap.at(node)) {
      path.push_back(node->getId());
    }

    path.push_back(startNode->getId());
    std::reverse(path.begin(), path.end());

    return path;
  }

 private:
  AlgorithmsUtilities() = delete;
};

template <typename FrontierType>
inline std::ostream& operator<<(std::ostream& os, const SearchContext<FrontierType>& context) {
  if (context.path.empty()) {
    os << "<empty path>";
    return os;
  }

  for (std::size_t i = 0; i < context.path.size(); ++i) {
    os << context.path[i];

    if (i + 1 < context.path.size()) {
      os << " -> ";
    }
  }

  return os;
}