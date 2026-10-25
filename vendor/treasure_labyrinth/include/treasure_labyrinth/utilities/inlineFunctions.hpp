/*******************************
 * @file inlineFunctions.h
 * @brief Header file containing inline functions for the Treasure Labyrinth project.
 * @author Giovanni Mendez Sanchez
 * @date 08-05-26
 * @version 1.0
 */

#pragma once
#include <functional>
#include <random>
#include <stdexcept>
#include <vector>

namespace tl::utilities {
/** @brief Validates a single argument.
 * @tparam T The type of the argument.
 * @param data The argument to validate.
 * @throws std::invalid_argument if the argument is invalid.
 */
template <typename T>
inline void validateArgument(T* data) {
  if (data == nullptr) {
    throw std::invalid_argument("Error: One or more input data pointers are null.");
  }
}

/** @brief Validates a std::function callback.
 * @tparam ReturnType The return type of the callback.
 * @tparam Args The argument types of the callback.
 * @param callback The callback to validate.
 * @throws std::invalid_argument if the callback is null.
 */
template <typename ReturnType, typename... Args>
inline void validateArgument(const std::function<ReturnType(Args...)>& callback) {
  if (!callback) {
    throw std::invalid_argument("Error: callback function is null.");
  }
}

/** @brief Validates a container with an empty() method.
 * @tparam T The type of the container.
 * @param container The container to validate.
 * @throws std::invalid_argument if the container is empty.
 */
template <typename T>
  requires requires(const T& value) { value.empty(); }
inline void validateArgument(const T& container) {
  if (container.empty()) {
    throw std::invalid_argument("Error: Container is empty.");
  }
}

/**
 * @brief Validates all supported arguments in a single call.
 *
 * Supported argument types:
 * - raw pointers, validated against nullptr
 * - std::function callbacks, validated against empty callbacks
 * - containers with an empty() method, validated against empty containers
 *
 * @tparam Args Argument types to validate.
 * @param args Arguments to validate.
 * @throws std::invalid_argument if any supported argument is invalid.
 */
template <typename... Args>
inline void validateArguments(const Args&... args) {
  (validateArgument(args), ...);
}

/**
 * @brief Chooses a random element from a vector of possible data.
 * @tparam T The type of the elements in the vector.
 * @param possibleData A vector containing the possible data to choose from.
 * @return A randomly chosen element from the vector.
 * @throws std::invalid_argument if the vector is empty.
 */
template <typename T>
inline const T& chooseRandom(const std::vector<T>& possibleData) {
  if (possibleData.empty()) {
    throw std::invalid_argument("Error: Possible data list is empty.");
  }

  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<std::size_t> dis(0, possibleData.size() - 1);

  return possibleData[dis(gen)];
}

/** @brief Shuffles the elements of a vector randomly.
 * @tparam T The type of the elements in the vector.
 * @param data The vector to shuffle.
 */
template <typename T>
inline void shuffleVector(std::vector<T>& data) {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::shuffle(data.begin(), data.end(), gen);
}

}  // namespace tl::utilities
