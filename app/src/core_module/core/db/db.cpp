#include <core/db/db.h>
#include <core/db/db_stmt.h>
#include <common/src/log.h>
#include <common/src/error.h>
#include <core/db/db_sqlite_error_names.h>
#include <unordered_map>
#include <sstream>

#define ENABLE_SQL_LOG 0

namespace orion {

std::shared_ptr<Db>
Db::create()
{
    return std::shared_ptr<Db>(new Db()); // the new operator is used to call the private constructor
}

Error
Db::open(const std::string& path)
{
    VERIFY(open(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE));
    SUCCEED();
}

Error
Db::open(const std::string& path, int flags)
{
    m_path = path;
    m_flags = flags;
    m_db = nullptr;

    int rc = sqlite3_open_v2(m_path.c_str(), &m_db, m_flags, NULL);
    if (rc != SQLITE_OK) {
        RAISE(ErrorCode::open, "failed to open database %s (%s): %s", m_path.c_str(), error_name(rc).c_str(), error_message());
    }

    VERIFY(exec("PRAGMA foreign_keys = ON;"));    // enables foreign keys (they are disabled by default)
    //VERIFY(exec("PRAGMA strict = ON;"));          // does not do anything (must be used in CREATE TABLE statements instead)

    VERIFY(exec("PRAGMA journal_mode = WAL;"));   // better concurrency and performance (can still DROP/CREATE indexes mid-TX so DELETE mode not needed)
    //VERIFY(exec("PRAGMA journal_mode = DELETE;"));   // can DROP/CREATE indexes mid-TX, but less performant than WAL mode
    
    VERIFY(exec("PRAGMA synchronous = FULL;"));   // slower, but very durable and safe
    //VERIFY(exec("PRAGMA synchronous = NORMAL;")); // faster but less durable than FULL mode
    //VERIFY(exec("PRAGMA synchronous = OFF;")); // fastest, but not durable (can lose data on crash)

    VERIFY(exec("PRAGMA temp_store = MEMORY;"));  // faster temp tables (used for sorting, etc.), uses RAM
    VERIFY(exec("PRAGMA cache_size = -2000;"));   // sets page cache to 2 MB in memory (minus sign means KB)
    
    //VERIFY(exec("PRAGMA locking_mode = EXCLUSIVE;")); // avoids "database is locked" errors, but requires exclusive access

    SUCCEED();
}

bool
Db::is_open() const
{
    return m_db != nullptr;
}

Error
Db::close()
{
    if (m_db) {
        int rc = sqlite3_close(m_db);
        if (rc != SQLITE_OK) {
            RAISE(ErrorCode::close, "failed to close database %s (%s): %s", m_path.c_str(), error_name(rc).c_str(), error_message());
        }
        m_db = nullptr;
    }
    SUCCEED();
}

Db::~Db()
{
    close();
}

std::string
Db::error_name(int code)
{
    auto it = global_sqlite_error_names.find(code);
    if (it != global_sqlite_error_names.end()) {
        return it->second;
    }
    return "SQLiteError::" + std::to_string(code);
}

std::shared_ptr<Db>
Db::shared_from_this()
{
    return std::static_pointer_cast<Db>(std::enable_shared_from_this<Db>::shared_from_this());
}

Error
Db::exec(const std::string& sql, const std::vector<DbVariant>& args, int64_t* rows_affected, int* result_code)
{
    if (!is_open()) {
        RAISE(ErrorCode::invalid_state, "database %s is not open: %s", m_path.c_str(), sql.c_str());
    }

    if (rows_affected) {
        *rows_affected = 0; // initialize to zero because we want to accumulate changes
    }

    std::string tail_sql;
    auto next_sql = sql;
    do {
        DbStmt stmt;
        VERIFY(stmt.prepare(shared_from_this(), next_sql, args, &tail_sql, result_code));
        if (!stmt) {
            // this means the statement is empty, so we can skip it
            break;
        }

        debug_log_sql(next_sql, args);

        stmt.next(result_code); // Step the statement to execute it
        VERIFY(stmt.error());

        if (rows_affected) {
            *rows_affected += sqlite3_changes64(m_db);
        }
        next_sql = tail_sql;
    } while (!next_sql.empty());
    SUCCEED();
}

Error
Db::query(const std::string& sql, DbStmt* stmt, std::string* tail_sql, int* result_code)
{
    VERIFY(query(sql, {}, stmt, tail_sql, result_code));
    SUCCEED();
}

Error
Db::query(
    const std::string& sql, const std::vector<DbVariant>& args, DbStmt* stmt, std::string* tail_sql, int* result_code)
{
    if (!is_open()) {
        RAISE(ErrorCode::invalid_state, "database %s is not open: %s", m_path.c_str(), sql.c_str());
    }
    if (!stmt) {
        RAISE_LOG(ErrorCode::argument, "DbStmt pointer is null");
    }

    VERIFY(stmt->prepare(shared_from_this(), sql, args, tail_sql, result_code));
    debug_log_sql(sql, args);
    SUCCEED();
}

Error
Db::query_rows_internal(
    bool first_row_only, const std::string& sql, const std::vector<DbVariant>& args, std::vector<DbRow>* rows,
    int* result_code)
{
    if (!rows) {
        RAISE_LOG(ErrorCode::argument, "DbRow vector pointer is null");
    }
    if (!is_open()) {
        RAISE(ErrorCode::invalid_state, "database %s is not open: %s", m_path.c_str(), sql.c_str());
    }

    DbStmt stmt;
    VERIFY(stmt.prepare(shared_from_this(), sql, args, nullptr, result_code));
    debug_log_sql(sql, args);

    while (stmt.next(result_code)) {
        if (!rows->empty() && first_row_only) {
            break;
        }

        DbRow row;
        VERIFY(stmt.row(&row));
        rows->emplace_back(row);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
Db::query_row(const std::string& sql, DbRow* row, int* result_code)
{
    VERIFY(query_row(sql, {}, row, result_code));
    SUCCEED();
}

Error
Db::query_row(const std::string& sql, const std::vector<DbVariant>& args, DbRow* row, int* result_code)
{
    if (!row) {
        RAISE_LOG(ErrorCode::argument, "DbRow pointer is null");
    }
    if (!is_open()) {
        RAISE(ErrorCode::invalid_state, "database %s is not open: %s", m_path.c_str(), sql.c_str());
    }

    auto first_row_only = true;
    std::vector<DbRow> rows;
    VERIFY(query_rows_internal(first_row_only, sql, args, &rows, result_code));
    
    if (rows.empty()) {
        RAISE(ErrorCode::db_no_rows, "query returned no rows: %s", sql.c_str());
    }
    row->insert(row->end(), rows[0].begin(), rows[0].end());
    SUCCEED();
}

Error
Db::query_rows(const std::string& sql, std::vector<DbRow>* rows, int* result_code)
{
    VERIFY(query_rows_internal(false, sql, {}, rows, result_code));
    SUCCEED();
}

Error
Db::query_rows(const std::string& sql, const std::vector<DbVariant>& args, std::vector<DbRow>* rows, int* result_code)
{
    VERIFY(query_rows_internal(false, sql, args, rows, result_code));
    SUCCEED();
}

Error
Db::query_integer(const std::string& sql, const std::vector<DbVariant>& args, int64_t* value, int* result_code)
{
    if (!value) {
        RAISE_LOG(ErrorCode::argument, "int64_t pointer is null");
    }
    if (!is_open()) {
        RAISE(ErrorCode::invalid_state, "database %s is not open: %s", m_path.c_str(), sql.c_str());
    }

    DbStmt stmt;
    VERIFY(query(sql, args, &stmt, nullptr, result_code));

    bool has_row = stmt.next(result_code);
    VERIFY(stmt.error());

    if (!has_row) {
        RAISE(ErrorCode::db_no_rows, "query returned no rows: %s", sql.c_str());
    }

    *value = stmt.integer(0);
    SUCCEED();
}

Error
Db::query_real(const std::string& sql, const std::vector<DbVariant>& args, double* value, int* result_code)
{
    if (!value) {
        RAISE_LOG(ErrorCode::argument, "double pointer is null");
    }
    
    std::optional<double> optional_value;
    VERIFY(query_real_or_null(sql, args, &optional_value, result_code));
    if (!optional_value.has_value()) {
        RAISE(ErrorCode::invalid_type, "query returned NULL instead of REAL value: %s", sql.c_str());
    }

    *value = optional_value.value();
    SUCCEED();
}

Error
Db::query_real_or_null(
    const std::string& sql, const std::vector<DbVariant>& args, std::optional<double>* value, int* result_code)
{
    if (!value) {
        RAISE_LOG(ErrorCode::argument, "optional<double> pointer is null");
    }
    if (!is_open()) {
        RAISE(ErrorCode::invalid_state, "database %s is not open: %s", m_path.c_str(), sql.c_str());
    }

    DbStmt stmt;
    VERIFY(query(sql, args, &stmt, nullptr, result_code));

    bool has_row = stmt.next(result_code);
    VERIFY(stmt.error());

    if (!has_row) {
        RAISE(ErrorCode::db_no_rows, "query returned no rows: %s", sql.c_str());
    }

    *value = stmt.real_optional(0);
    SUCCEED();
}

Error
Db::query_bool(const std::string& sql, const std::vector<DbVariant>& args, bool* value, int* result_code)
{
    if (!value) {
        RAISE_LOG(ErrorCode::argument, "bool pointer is null");
    }
    if (!is_open()) {
        RAISE(ErrorCode::invalid_state, "database %s is not open: %s", m_path.c_str(), sql.c_str());
    }

    DbStmt stmt;
    VERIFY(query(sql, args, &stmt, nullptr, result_code));

    bool has_row = stmt.next(result_code);
    VERIFY(stmt.error());

    if (!has_row) {
        RAISE(ErrorCode::db_no_rows, "query returned no rows: %s", sql.c_str());
    }

    *value = stmt.integer(0) != 0; // convert integer to bool
    SUCCEED();
}

Error
Db::query_text(const std::string& sql, const std::vector<DbVariant>& args, std::string* value, int* result_code)
{
    if (!value) {
        RAISE_LOG(ErrorCode::argument, "std::string pointer is null");
    }
    if (!is_open()) {
        RAISE(ErrorCode::invalid_state, "database %s is not open: %s", m_path.c_str(), sql.c_str());
    }

    DbStmt stmt;
    VERIFY(query(sql, args, &stmt, nullptr, result_code));

    bool has_row = stmt.next(result_code);
    VERIFY(stmt.error());

    if (!has_row) {
        RAISE(ErrorCode::db_no_rows, "query returned no rows: %s", sql.c_str());
    }

    *value = stmt.text(0);
    SUCCEED();
}

Error
Db::table_exists(const std::string& table_name, bool* exists, int* result_code)
{
    if (!exists) {
        RAISE_LOG(ErrorCode::argument, "bool exists is null");
    }
    if (!is_open()) {
        RAISE(ErrorCode::invalid_state, "database %s is not open", m_path.c_str());
    }

    DbRow row;
    Error err = query_row("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;", { table_name }, &row, result_code);
    if (err) {
        if (err.code() == ErrorCode::db_no_rows) {
            // Table does not exist
            *exists = false;
            SUCCEED();
        }
        VERIFY(err); // propagate error
    }

    *exists = true;
    SUCCEED();
}

const char*
Db::error_message() const
{
    return sqlite3_errmsg(m_db);
}

Error
Db::begin(DbTx* tx, int* result_code)
{
    if (!is_open()) {
        RAISE_LOG(ErrorCode::invalid_state, "database %s is not open", m_path.c_str());
    }
    if (!tx) {
        RAISE_LOG(ErrorCode::argument, "transaction pointer is null");
    }

    VERIFY(tx->begin(shared_from_this(), result_code));
    SUCCEED();
}

Error
Db::try_begin(DbTx* tx, int* result_code)
{
    if (in_transaction()) {
        // already in a transaction, do nothing
        SUCCEED();
    }
    VERIFY(begin(tx, result_code));
    SUCCEED();
}

bool
Db::in_transaction() const
{
    return sqlite3_get_autocommit(m_db) == 0;
}

std::string
to_string(const std::vector<DbVariant>& args)
{
    std::string result;
    for (const auto& arg : args) {
        if (!result.empty()) {
            result += ", ";
        }
        result += arg.to_string();
    }
    return "[" + result + "]";
}

void
Db::log_sql(const std::string& sql, const std::vector<DbVariant>& args, const std::string& prefix)
{
    std::string args_str;
    if (args.size() > 0) {
        args_str = " -- " + to_string(args);
    }
    log_debug("%ssql: %s%s", prefix.c_str(), sql.c_str(), args_str.c_str());
}

void
debug_log_sql(const std::string& sql, const std::vector<DbVariant>& args)
{
#if ENABLE_SQL_LOG
    Db::log_sql(sql, args);
#endif // ENABLE_SQL_LOG
}

std::string
Db::make_placeholders(size_t count)
{
    if (count == 0) {
        return "";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) oss << ",";
        oss << "?";
    }
    return oss.str();
}

std::string
Db::join_strings(const std::vector<std::string>& parts, const std::string& sep)
{
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        result += parts[i];
        if (i + 1 < parts.size()) {
            result += sep;
        }
    }
    return result;
}

std::string
Db::join_into_set_clause(const std::vector<std::string>& columns)
{
    std::string result;
    for (size_t i = 0; i < columns.size(); ++i) {
        result += columns[i] + "=?";
        if (i + 1 < columns.size()) {
            result += ",";
        }
    }
    return result;
}

std::string
Db::escape_like(const std::string& str, char escape_char)
{
    std::ostringstream escaped;
    for (char c : str) {
        if (c == escape_char || c == '%' || c == '_') {
            escaped << escape_char;
        }
        escaped << c;
    }
    return escaped.str();
}

} // namespace orion