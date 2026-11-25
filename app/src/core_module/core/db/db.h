#pragma once

#include <string>
#include <sqlite3.h>
#include <memory>

#include <common/src/error.h>
#include <core/db/db_variant.h>
#include <core/db/db_tx.h>
#include <core/db/db_stmt.h>

namespace orion {

class DbStmt;
class DbTx;

class Db : public std::enable_shared_from_this<Db> {
public:
    static std::shared_ptr<Db> create();
    ~Db();

    Error open(const std::string& path);
    Error open(const std::string& path, int flags);
    Error close();
    bool is_open() const;

    // Returns the path to the database file.
    const std::string& path() const { return m_path; }
    
    // Prepares and executes SQL statement without returning rows. If multiple statements are provided,
    // they are executed in order. If the statement is empty, it is skipped.
    // If rows_affected is provided, it accumulates the number of rows affected by all.
    Error exec(
        const std::string& sql, const std::vector<DbVariant>& args = {}, int64_t* rows_affected = nullptr,
        int* result_code = nullptr
    );

    // Prepares DB statement using the first specified SQL statement, returning a prepared statement object.
    // If the statement is empty, it is skipped. If multiple statements are provided, the next statement is
    // returned in tail_sql.
    Error query(const std::string& sql, DbStmt* stmt, std::string* tail_sql = nullptr, int* result_code = nullptr);

    // Prepares DB statement using the first specified SQL statement, returning a prepared statement object.
    // If the statement is empty, it is skipped. If multiple statements are provided, the next statement is
    // returned in tail_sql.
    Error query(
        const std::string& sql, const std::vector<DbVariant>& args, DbStmt* stmt, std::string* tail_sql = nullptr,
        int* result_code = nullptr
    );

    // Prepares and executes the first SQL statement, returning a single row. If no row is found,
    // it returns ErrorCode::db_no_rows. If more than one row is found, it returns the first row.
    // If multiple SQL statements are provided, all but the first one are ignored.
    Error query_row(const std::string& sql, DbRow* row, int* result_code = nullptr);

    // Prepares and executes SQL statement, returning a single row. If no row is found,
    // it returns ErrorCode::db_no_rows. If more than one row is found, it returns the first row.
    // If multiple SQL statements are provided, all but the first one are ignored.
    Error query_row(const std::string& sql, const std::vector<DbVariant>& args, DbRow* row, int* result_code = nullptr);

    // If multiple SQL statements are provided, all but the first one are ignored.
    Error query_rows(const std::string& sql, std::vector<DbRow>* rows, int* result_code = nullptr);
    
    // If multiple SQL statements are provided, all but the first one are ignored.
    Error query_rows(
        const std::string& sql, const std::vector<DbVariant>& args, std::vector<DbRow>* rows,
        int* result_code = nullptr
    );

    // If the SQL statement returns 0 rows, then ErrorCode::db_no_rows error is returned.
    Error query_integer(
        const std::string& sql, const std::vector<DbVariant>& args, int64_t* value, int* result_code = nullptr
    );

    // If the SQL statement returns 0 rows, then ErrorCode::db_no_rows error is returned.
    // Returns ErrorCode::invalid_type if the returned value is NULL.
    Error query_real(
        const std::string& sql, const std::vector<DbVariant>& args, double* value, int* result_code = nullptr
    );

    // If the SQL statement returns 0 rows, then ErrorCode::db_no_rows error is returned.
    Error query_real_or_null(
        const std::string& sql, const std::vector<DbVariant>& args, std::optional<double>* value,
        int* result_code = nullptr
    );

    // If the SQL statement returns 0 rows, then ErrorCode::db_no_rows error is returned.
    Error query_bool(
        const std::string& sql, const std::vector<DbVariant>& args, bool* value, int* result_code = nullptr
    );

    // If the SQL statement returns 0 rows, then ErrorCode::db_no_rows error is returned.
    Error query_text(
        const std::string& sql, const std::vector<DbVariant>& args, std::string* value, int* result_code = nullptr
    );

    sqlite3* raw_db() const { return m_db; }
    std::shared_ptr<Db> shared_from_this();
    Error table_exists(const std::string& table_name, bool* exists = nullptr, int* result_code = nullptr);
    const char* error_message() const;

    // Begins a transaction. The transaction must be manually committed, otherwise it is automatically rolled back.
    Error begin(DbTx* tx, int* result_code = nullptr);

    // Begins a transaction if not already in a transaction. If already in a transaction, does nothing.
    Error try_begin(DbTx* tx, int* result_code = nullptr);

    bool in_transaction() const;

    static std::string error_name(int code);

    // Generates a string with the specified number of placeholders for SQL queries in the form of for example:
    // "?,?,?" (for count == 3)
    static std::string make_placeholders(size_t count);
    static std::string join_strings(const std::vector<std::string>& parts, const std::string& sep);

    // Converts column names for example {"col1", "col2"} into "col1=?, col2=?"
    static std::string join_into_set_clause(const std::vector<std::string>& columns);

    // Escapes a string for use in LIKE queries, replacing '%', '_', and escape_char with their escaped versions.
    static std::string escape_like(const std::string& str, char escape_char);

    static void log_sql(const std::string& sql, const std::vector<DbVariant>& args, const std::string& prefix = "");

private:
    Db() = default; // private constructor to enforce the use of create() method
    Error query_rows_internal(
        bool first_row_only, const std::string& sql, const std::vector<DbVariant>& args, std::vector<DbRow>* rows,
        int* result_code = nullptr
    );
    
    std::string m_path;
    int m_flags = 0;
    sqlite3* m_db = nullptr;
};

std::string to_string(const std::vector<DbVariant>& args);
void debug_log_sql(const std::string& sql, const std::vector<DbVariant>& args);

} // namespace orion