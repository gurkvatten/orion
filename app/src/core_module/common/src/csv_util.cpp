#include <src/csv_util.h>
#include <functional>

namespace orion {

std::string
escape_csv_field(const std::string& field, char delimiter)
{
    bool needs_quotes = false;

    for (char c : field) {
        if (c == delimiter || c == '"' || c == '\n' || c == '\r') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        return field;  // No escaping required
    }

    // Escape quotes by doubling them
    std::string escaped;
    escaped.reserve(field.size() + 2); // slight optimization
    escaped.push_back('"');

    for (char c : field) {
        if (c == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(c);
        }
    }

    escaped.push_back('"');
    return escaped;
}

Error
parse_csv(const std::string& input, char delimiter, std::function<void(const std::vector<std::string>& row)> row_cb)
{
    std::vector<std::string> row;
    std::string field;

    enum State { Normal, InQuotes, QuoteClosed };
    State state = Normal;

    for (size_t i = 0; i < input.size(); i++) {
        char c = input[i];

        switch (state) {

        case Normal:
            if (c == '"') {
                if (!field.empty()) {
                    RAISE(ErrorCode::invalid_format, "unexpected quote inside unquoted field");
                }
                state = InQuotes;
            }
            else if (c == delimiter) {
                row.push_back(field);
                field.clear();
                state = Normal;
            }
            else if (c == '\n') {
                row.push_back(field);
                row_cb(row);
                row.clear();
                field.clear();
            }
            else if (c == '\r') {
                // ignore CR
            }
            else {
                field += c;
            }
            break;

        case InQuotes:
            if (c == '"') {
                // possible escape quote
                if (i + 1 < input.size() && input[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    // closing quote
                    state = QuoteClosed;
                }
            } else {
                field += c;
            }
            break;

        case QuoteClosed:
            if (c == delimiter) {
                row.push_back(field);
                field.clear();
                state = Normal;
            } else if (c == '\n') {
                row.push_back(field);
                row_cb(row);
                row.clear();
                field.clear();
                state = Normal;
            } else if (c == '\r') {
                // ignore CR
            } else {
                RAISE(ErrorCode::invalid_format, "unexpected character after closing quote");
            }
            break;
        }
    }

    // Final checks
    if (state == InQuotes) {
        RAISE(ErrorCode::invalid_format, "unclosed quote at end of input");
    }

    row.push_back(field);
    row_cb(row);
    SUCCEED();
}

Error
parse_csv(const std::string& input, char delimiter, std::vector<std::vector<std::string>>* out_rows)
{
    RAISE_CHECK(out_rows);

    auto push_row = [out_rows](const std::vector<std::string>& row) {
        if (row.empty() || (row.size() == 1 && row[0].empty())) {
            return; // skip empty rows
        }
        out_rows->push_back(row);
    };

    return parse_csv(input, delimiter, push_row);
}

//naive implementation assuming no \n except at the end of row
Error
split_csv_to_rows(const std::string& input, std::function<void(const std::string& row)> row_cb)
{
    size_t row_nr;
    return split_csv_to_rows(input, [row_cb](const std::string& row, size_t cow_nr) -> Error {
        row_cb(row);
        SUCCEED();
    });
}

Error
split_csv_to_rows(const std::string& input, std::function<Error(const std::string& row, size_t row_nr)> row_cb)
{
    RAISE_CHECK(row_cb);

    Error e;
    size_t row_nr = 0;
    size_t end = 0;
    size_t start = 0;
    while ((end = input.find('\n', start)) != std::string::npos) {
        e = row_cb(input.substr(start/*skip \n*/, end - start), row_nr++);
        if (e) {
            return e;
        }
        start = end + 1;
    }
    //last line
    if (start < input.size()) {
        if ((e = row_cb(input.substr(start), row_nr))) {
            return e;
        }
    }

    SUCCEED();
}

} // namespace orion