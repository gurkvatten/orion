#pragma once

#include <string>
#include <src/error.h>
#include <functional>

namespace orion {

std::string base64_encode(const unsigned char* data, size_t len);

// Convenience overload for std::string input
std::string base64_encode(const std::string& data);

// Decode Base64-encoded ASCII input. Returns raw binary data (may contain NULs).
std::string base64_decode(const std::string& input);

std::string base64url_decode(const std::string& encoded);
std::string base64url_encode(const std::string& data);

std::string csv_to_html(const std::string& csv_data, Error& error);

//processes one single json 2D array of rows The 1st row must be the header. The callabck is called for each row
Error json_array_to_csv(const std::string& json_str, char delimiter, std::function<void(const std::string& row)> row_cb);

} // namespace orion
