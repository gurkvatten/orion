#pragma once

#include <string>
#include <vector>
#include <functional>

#include <src/error.h>

namespace orion {

std::string escape_csv_field(const std::string& field, char delimiter);
Error parse_csv(const std::string& input, char delimiter, std::function<void(const std::vector<std::string>& row)> row_cb);
Error parse_csv(const std::string& csv_text, char delimiter, std::vector<std::vector<std::string>>* out_rows);
Error split_csv_to_rows(const std::string& input, std::function<void(const std::string& row)> row_cb);
Error split_csv_to_rows(const std::string& input, std::function<Error(const std::string& row, size_t row_nr)> row_cb);

} // namespace orion