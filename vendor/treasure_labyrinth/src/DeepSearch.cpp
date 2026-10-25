/**
 * @file DeepSearch.cpp
 * @brief Implementation of the DeepSearch class, which provides a method for performing a deep
 * search on a graph.
 * @author Giovanni Mendez Sanchez
 * @date 08-05-26
 * @version 1.0
 */

#include <treasure_labyrinth/algorithms/DeepSearch.hpp>
#include <treasure_labyrinth/algorithms/Node.hpp>
#include <treasure_labyrinth/utilities/AlgorithmsUtilities.hpp>
#include <treasure_labyrinth/utilities/inlineFunctions.hpp>

std::vector<int> DeepSearch::generatePath(
    Node* startNode, const std::function<std::vector<Node*>(Node*)>& getNeighbors,
    const std::function<void(Node*, Node*)>& onPathOpened) {
  tl::utilities::validateArguments(startNode, getNeighbors);

  SearchContext<std::stack<Node*>> context;  // Create a search context to hold the data.

  // Initialize the frontier with the starting node
  context.frontier.push(startNode);
  context.path.push_back(startNode->getId());
  context.visited.insert(startNode);

  // Main loop for the depth-first search.
  while (!context.frontier.empty()) {
    Node* currentNode = context.frontier.top();

    // Store only the neighbors that have not been visited yet.
    std::vector<Node*> unvisitedNeighbors =
        AlgorithmsUtilities::getUnvisitedNeighbors(currentNode, getNeighbors, context.visited);

    // If there are valid neighbors available, choose one randomly and go deeper.
    if (!unvisitedNeighbors.empty()) {
      Node* nextNode = tl::utilities::chooseRandom(unvisitedNeighbors);

      onPathOpened(currentNode, nextNode);

      context.visited.insert(nextNode);
      context.frontier.push(nextNode);
      context.path.push_back(nextNode->getId());
      continue;
    }

    // If there are no unvisited neighbors, backtrack to the previous node.
    context.frontier.pop();
  }

  return context.path;
}

std::vector<int> DeepSearch::findPath(
    Node* startNode, Node* targetNode,
    const std::function<std::vector<Node*>(Node*)>& getNeighbors) {
  tl::utilities::validateArguments(startNode, targetNode, getNeighbors);

  SearchContext<std::stack<Node*>> context;  // Create a search context to hold the data.

  // Initialize the frontier with the starting node
  context.frontier.push(startNode);
  context.visited.insert(startNode);

  // Main loop for the depth-first search.
  while (!context.frontier.empty()) {
    Node* currentNode = context.frontier.top();
    context.frontier.pop();

    // If we have reached the target node, reconstruct the path and return it.
    if (currentNode == targetNode) {
      return AlgorithmsUtilities::reconstructPath(startNode, targetNode, context.parentMap);
    }

    // Check each unvisited neighbor and add it to the frontier.
    std::vector<Node*> unvisitedNeighbors =
        AlgorithmsUtilities::getUnvisitedNeighbors(currentNode, getNeighbors, context.visited);

    for (Node* neighbor : unvisitedNeighbors) {
      context.visited.insert(neighbor);
      context.frontier.push(neighbor);
      context.parentMap[neighbor] = currentNode;
    }
  }

  return {};  // Return an empty path if no path is found
}
