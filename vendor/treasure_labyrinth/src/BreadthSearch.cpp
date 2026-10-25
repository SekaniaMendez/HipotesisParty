/**
 * @file BreadthSearch.cpp
 * @brief Implementation of the BreadthSearch class, which provides a method for performing a
 * BreadthSearch search on a graph.
 * @author Giovanni Mendez Sanchez
 * @date 13-05-26
 * @version 1.0
 */

#include "treasure_labyrinth/algorithms/BreadthSearch.hpp"

std::vector<int> BreadthSearch::generatePath(
    Node* startNode, const std::function<std::vector<Node*>(Node*)>& getNeighbors,
    const std::function<void(Node*, Node*)>& onPathOpened) {
  // validate input arguments to ensure they are not null and the callback is valid.
  tl::utilities::validateArguments(startNode, getNeighbors, onPathOpened);
  // Using a SearchContext to manage the state of the search, including the frontier, visited nodes.
  SearchContext<std::queue<Node*>> context;

  // Initialize the search by adding the start node to the frontier and marking it as visited.
  context.frontier.push(startNode);
  context.visited.insert(startNode);
  context.path.push_back(startNode->getId());

  // Continue the search until there are no more nodes to explore in the frontier.
  while (!context.frontier.empty()) {
    Node* currentNode = context.frontier.front();
    context.frontier.pop();

    // Get the unvisited neighbors of the current node.
    std::vector<Node*> unvisitedNeighbors =
        AlgorithmsUtilities::getUnvisitedNeighbors(currentNode, getNeighbors, context.visited);

    // Shuffle the unvisited neighbors to ensure a more randomized traversal of the graph.
    tl::utilities::shuffleVector(unvisitedNeighbors);

    // Add each unvisited neighbor to the frontier, mark it as visited, and record its parent.
    for (Node* neighbor : unvisitedNeighbors) {
      onPathOpened(currentNode, neighbor);

      context.frontier.push(neighbor);
      context.visited.insert(neighbor);
      context.parentMap[neighbor] = currentNode;
      context.path.push_back(neighbor->getId());
    }
  }

  return context.path;
}

std::vector<int> BreadthSearch::findPath(
    Node* startNode, Node* targetNode,
    const std::function<std::vector<Node*>(Node*)>& getNeighbors) {
  // validate input arguments to ensure they are not null and the callback is valid.
  tl::utilities::validateArguments(startNode, targetNode, getNeighbors);

  // Using a SearchContext to manage the state of the search, including the frontier, visited nodes.
  SearchContext<std::queue<Node*>> context;

  // Initialize the search by adding the start node to the frontier and marking it as visited.
  context.frontier.push(startNode);
  context.visited.insert(startNode);

  // Continue the search until there are no more nodes to explore in the frontier.
  while (!context.frontier.empty()) {
    Node* currentNode = context.frontier.front();
    context.frontier.pop();

    // If we have reached the target node, reconstruct and return the path from start to target.
    if (currentNode == targetNode) {
      return AlgorithmsUtilities::reconstructPath(startNode, targetNode, context.parentMap);
    }

    // Get the unvisited neighbors of the current node.
    std::vector<Node*> unvisitedNeighbors =
        AlgorithmsUtilities::getUnvisitedNeighbors(currentNode, getNeighbors, context.visited);

    // Add each unvisited neighbor to the frontier, mark it as visited, and record its parent.
    for (Node* neighbor : unvisitedNeighbors) {
      context.frontier.push(neighbor);
      context.visited.insert(neighbor);
      context.parentMap[neighbor] = currentNode;
    }
  }

  // If we exhaust the frontier without finding the target node, return an empty path.
  return {};
}