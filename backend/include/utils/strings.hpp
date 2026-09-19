#pragma once

#include <string>
#include <string_view>

namespace wt::strings {

std::string_view trim(std::string_view text);
std::string to_upper(std::string_view text);
bool is_blank(std::string_view text);

// Returns a trimmed copy, throwing ApiError(invalid request) when blank or
// longer than max_length.
std::string require_text(std::string_view value, std::string_view field,
                         std::size_t max_length);
std::string optional_text(std::string_view value, std::string_view field,
                          std::size_t max_length);

}  // namespace wt::strings
