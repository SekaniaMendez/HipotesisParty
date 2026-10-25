#pragma once

#include <sstream>
#include <string>

namespace tl::strings {

/**
 * @brief Converts a value of any type that supports stream insertion to a string.
 * This function uses a stringstream to convert the given value to a string.
 */
template <typename T>
inline std::string toString(const T& value) {
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

}  // namespace tl::strings