#pragma once

#include <cstdint>
#include <unordered_map>
#include <memory>
#include <variant>
#include <optional>

#include <common/src/error.h>
#include <core/db/db.h>

namespace orion {

class Db;

// DbStmt class represents a prepared statement in the database.
class DbStmt {
public:
    DbStmt() = default;
    DbStmt(const DbStmt&) = delete;
    DbStmt& operator=(const DbStmt&) = delete;
    DbStmt(DbStmt&&) = default;
    DbStmt& operator=(DbStmt&&) = default;

    ~DbStmt();
    Error finalize();

    // Performs the next step in the prepared statement. Returns true if a row is available,
    // false if no more rows are available. If error occurs, it sets the error state (that
    // can be retrieved using error() call) and returns false. If error occurs, it sets
    // error message (output of sqlite3_errmsg call).
    bool next(int* result_code = nullptr, std::string* error_message = nullptr);

    // Returns error state of the statement. This should be called after next() returns false
    // to check if an error occurred during the step execution.
    const Error& error() const { return m_err; }

    Error prepare(
        const std::shared_ptr<Db> db, const std::string& sql, const std::vector<DbVariant>& args = {},
        std::string* tail_sql = nullptr, int* result_code = nullptr
    );
    std::shared_ptr<Db> db() const { return m_db; }
    std::string sql() const { return m_sql; }
    int column_count() const;
    const char* column_name(int index) const;
    operator bool() const { return m_stmt != nullptr; }

    bool is_null(int col_index) const;
    bool is_null(const std::string& col_name) const;

    std::string text(int col_index) const;
    std::string text(const std::string& col_name) const;

    std::optional<std::string> text_optional(int col_index) const;
    std::optional<std::string> text_optional(const std::string& col_name) const;
    
    int64_t integer(int col_index) const;
    int64_t integer(const std::string& col_name) const;

    std::optional<int64_t> integer_optional(int col_index) const;
    std::optional<int64_t> integer_optional(const std::string& col_name) const;

    double real(int col_index) const;
    double real(const std::string& col_name) const;

    std::optional<double> real_optional(int col_index) const;
    std::optional<double> real_optional(const std::string& col_name) const;

    std::vector<uint8_t> blob(int col_index) const;
    std::vector<uint8_t> blob(const std::string& col_name) const;

    std::optional<std::vector<uint8_t>> blob_optional(int col_index) const;
    std::optional<std::vector<uint8_t>> blob_optional(const std::string& col_name) const;

    DbVariant variant(int col_index) const;
    DbVariant variant(const std::string& col_name) const;

    Error row(DbRow* row) const;

private:
    // Returns -1 if the column does not exist.
    int index_of_column(const std::string& col_name) const;

    std::shared_ptr<Db> m_db;
    sqlite3_stmt* m_stmt = nullptr;
    std::string m_sql; // for trace purposes
    std::vector<DbVariant> m_args; // bound arguments (for trace purposes)
    std::unordered_map<std::string, int> m_col_name_to_index;
    Error m_err;
};

} // namespace orion