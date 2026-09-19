#include "utils/strings.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "common/error.hpp"

namespace wt::strings {

std::string_view trim(std::string_view text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return {};
  }
  const auto end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

std::string to_upper(std::string_view text) {
  std::string result(text);
  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return result;
}

bool is_blank(std::string_view text) { return trim(text).empty(); }

std::string require_text(std::string_view value, std::string_view field,
                         std::size_t max_length) {
  const auto trimmed = trim(value);
  if (trimmed.empty()) {
    throw invalid_request(std::string(field) + " is required");
  }
  if (trimmed.size() > max_length) {
    throw invalid_request(std::string(field) + " is too long (max " +
                          std::to_string(max_length) + " characters)");
  }
  return std::string(trimmed);
}

std::string optional_text(std::string_view value, std::string_view field,
                          std::size_t max_length) {
  const auto trimmed = trim(value);
  if (trimmed.size() > max_length) {
    throw invalid_request(std::string(field) + " is too long (max " +
                          std::to_string(max_length) + " characters)");
  }
  return std::string(trimmed);
}

}  // namespace wt::strings
