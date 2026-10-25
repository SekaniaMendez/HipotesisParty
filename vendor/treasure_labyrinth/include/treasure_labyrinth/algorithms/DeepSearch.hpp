/**
 * @file DeepSearch.h
 * @brief Declaration of the DeepSearch class, which provides a method for performing a deep search
 * on a graph.
 * @author Giovanni Mendez Sanchez
 * @date 08-05-26
 * @version 1.0
 */

#pragma once

#include <functional>
#include <ostream>
#include <stack>
#include <treasure_labyrinth/algorithms/Node.hpp>
#include <treasure_labyrinth/utilities/AlgorithmsUtilities.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Node;

/**
 * @brief Implements a depth-first search algorithm to explore a graph.
 * The DeepSearch class provides methods to generate a path through the graph and to find a specific
 * path between two nodes using depth-first search.
 */
class DeepSearch {
 public:
  // Generates a path from the start node until all nodes are visited.
  static std::vector<int> generatePath(Node* startNode,
                                       const std::function<std::vector<Node*>(Node*)>& getNeighbors,
                                       const std::function<void(Node*, Node*)>& onPathOpened);
  // Finds a path between two nodes using depth-first search.
  static std::vector<int> findPath(Node* startNode, Node* targetNode,
                                   const std::function<std::vector<Node*>(Node*)>& getNeighbors);
};
