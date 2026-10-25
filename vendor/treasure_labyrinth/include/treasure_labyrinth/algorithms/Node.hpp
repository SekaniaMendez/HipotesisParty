/**
 * @file Node.h
 * @brief Declaration of the Node class, which represents a node in a graph.
 * @author Giovanni Mendez Sanchez
 * @date 08-05-26
 * @version 1.0
 */

#pragma once

/**
 * @brief Represents a node in a graph.
 * The Node class contains an identifier, its position in a grid (row and column).
 */
class Node {
 public:
  Node(int id, int row, int col) : id(id), row(row), col(col) {}
  int getId() const { return id; }
  int getRow() const { return row; }
  int getCol() const { return col; }

 private:
  int id;
  int row;
  int col;
};