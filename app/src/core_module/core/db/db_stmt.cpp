#include <core/db/db_stmt.h>

namespace orion {

DbStmt::~DbStmt()
{
    finalize();
}

Error
DbStmt::finalize()
{
    if (m_stmt) {
        int rc = sqlite3_finalize(m_stmt);
        if (rc != SQLITE_OK) {
            RAISE(ErrorCode::db_prepare_statement, "failed to finalize statement (%s): %s",
                Db::error_name(rc).c_str(), m_sql.c_str());
        }
        m_stmt = nullptr;
    }
    SUCCEED();
}

bool
DbStmt::next(int* result_code, std::string* error_message)
{
    int rc = sqlite3_step(m_stmt);

    if (result_code) {
        *result_code = rc;
    }

    if (rc == SQLITE_DONE) {
        return false; // no more rows
    }
    if (rc == SQLITE_ROW) {
        return true; // row available
    }

    // Note that we have to call sqlite3_errmsg() immediately after sqlite3_step(),
    // otherwise we could get an error message of some other sqlite3 operation.
    const char* errmsg = sqlite3_errmsg(m_db->raw_db());
    if (error_message) {
        *error_message = errmsg;
    }

    // May be NULL
    const char* sqlite_err_str = sqlite3_errstr(rc);
    
    m_err = MAKE_ERROR(
        ErrorCode::db_step,
        "failed to execute sqlite step: %s (sqlite result code %d): %s:\n%s\n  -- args: %s",
        sqlite_err_str,
        rc,
        errmsg,
        m_sql.c_str(),
        to_string(m_args).c_str()
    );
    
    return false;
}

Error
DbStmt::prepare(
    const std::shared_ptr<Db> db, const std::string& sql, const std::vector<DbVariant>& args, std::string* tail_sql,
    int* result_code)
{
    m_db = db;
    m_sql = sql;
    m_args = args;
    const char* tail = nullptr;

    int rc = sqlite3_prepare_v2(m_db->raw_db(), sql.c_str(), -1, &m_stmt, &tail);

    if (result_code) {
        *result_code = rc;
    }

    if (rc != SQLITE_OK) {
        RAISE(ErrorCode::db_prepare_statement, "failed to prepare statement (%s): %s: %s",
            Db::error_name(rc).c_str(), m_db->error_message(), sql.c_str());
    }
    if (!m_stmt) {
        SUCCEED(); // nothing to prepare, empty statement
    }

    if (tail_sql) {
        *tail_sql = tail ? tail : "";
    }

    auto check = [](const std::string& sql, const std::vector<DbVariant>& args, int rc, const std::string& type, int idx, int* result_code) -> Error {
        if (result_code) {
            *result_code = rc;
        }

        if (rc != SQLITE_OK) {
            RAISE(ErrorCode::db_bind_value, "failed to bind %s value to parameter %d (%s): %s\n  -- args: %s",
                type.c_str(),
                idx,
                Db::error_name(rc).c_str(),
                sql.c_str(),
                to_string(args).c_str()
            );
        }
        SUCCEED();
    };

    // Bind arguments
    int i = 0;
    for (const auto& value : args) {
        i++;

        switch (value.type()) {
            case DbVariant::Type::null:
                VERIFY(check(sql, args, sqlite3_bind_null(m_stmt, i), "null", i, result_code));
                break;
            case DbVariant::Type::integer:
                VERIFY(check(sql, args, sqlite3_bind_int64(m_stmt, i, value.integer()), "integer", i, result_code));
                break;
            case DbVariant::Type::real:
                VERIFY(check(sql, args, sqlite3_bind_double(m_stmt, i, value.real()), "real", i, result_code));
                break;
            case DbVariant::Type::text:
                VERIFY(check(sql, args, sqlite3_bind_text(m_stmt, i, value.text().c_str(), -1, SQLITE_TRANSIENT), "text", i, result_code));
                break;
            case DbVariant::Type::blob: {
                const auto& blob = value.blob();
                // note: vector's data() method returns nullptr if the vector is empty (even if the vector is initialized)
                if (blob.empty()) {
                    uint8_t dummy = 0; // we need just a non-null address pointer for binding
                    VERIFY(check(sql, args, sqlite3_bind_blob(m_stmt, i, &dummy, 0, SQLITE_STATIC), "blob", i, result_code));
                } else {
                    VERIFY(check(sql, args, sqlite3_bind_blob(m_stmt, i, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT), "blob", i, result_code));
                }
                break;
            }
            default:
                RAISE(ErrorCode::db_unknown_variant, "unsupported DbVariant type for binding (%d): %s", value.type(), sql.c_str());
        }
    }

    // Prepare result columns
    int col_count = column_count();
    for (int i = 0; i < col_count; ++i) {
        const char* name = column_name(i);
        if (name) {
            m_col_name_to_index[name] = i;
        }
    }

    SUCCEED();
}

int
DbStmt::column_count() const
{
    return sqlite3_column_count(m_stmt);
}

const char*
DbStmt::column_name(int index) const
{
    return sqlite3_column_name(m_stmt, index);
}

bool
DbStmt::is_null(int col_index) const
{
    return sqlite3_column_type(m_stmt, col_index) == SQLITE_NULL;
}

bool
DbStmt::is_null(const std::string& col_name) const
{
    int idx = index_of_column(col_name);
    return sqlite3_column_type(m_stmt, idx) == SQLITE_NULL;
}

std::string
DbStmt::text(int col_index) const
{
    const unsigned char* text = sqlite3_column_text(m_stmt, col_index);
    return text ? reinterpret_cast<const char*>(text) : std::string();
}

std::string
DbStmt::text(const std::string& col_name) const
{
    int idx = index_of_column(col_name);
    const unsigned char* text = sqlite3_column_text(m_stmt, idx);
    return text ? reinterpret_cast<const char*>(text) : std::string();
}

std::optional<std::string>
DbStmt::text_optional(int col_index) const
{
    if (is_null(col_index)) {
        return std::nullopt;
    }
    return text(col_index);
}

std::optional<std::string>
DbStmt::text_optional(const std::string& col_name) const
{
    if (is_null(col_name)) {
        return std::nullopt;
    }
    return text(col_name);
}

int64_t
DbStmt::integer(int col_index) const
{
    return sqlite3_column_int64(m_stmt, col_index);
}

int64_t
DbStmt::integer(const std::string& col_name) const
{
    return sqlite3_column_int64(m_stmt, index_of_column(col_name));
}

std::optional<int64_t>
DbStmt::integer_optional(int col_index) const
{
    if (is_null(col_index)) {
        return std::nullopt;
    }
    return integer(col_index);
}

std::optional<int64_t>
DbStmt::integer_optional(const std::string& col_name) const
{
    if (is_null(col_name)) {
        return std::nullopt;
    }
    return integer(col_name);
}

double
DbStmt::real(int col_index) const
{
    return sqlite3_column_double(m_stmt, col_index);
}

double
DbStmt::real(const std::string& col_name) const
{
    return sqlite3_column_double(m_stmt, index_of_column(col_name));
}

std::optional<double>
DbStmt::real_optional(int col_index) const
{
    if (is_null(col_index)) {
        return std::nullopt;
    }
    return real(col_index);
}

std::optional<double>
DbStmt::real_optional(const std::string& col_name) const
{
    if (is_null(col_name)) {
        return std::nullopt;
    }
    return real(col_name);
}

std::vector<uint8_t>
DbStmt::blob(int col_index) const
{
    const void* blob = sqlite3_column_blob(m_stmt, col_index);
    int size = sqlite3_column_bytes(m_stmt, col_index);
    if (blob && size > 0) {
        const uint8_t* data = static_cast<const uint8_t*>(blob);
        return std::vector<uint8_t>(data, data + size);
    }
    return {}; // empty vector for NULL or empty BLOB
}

std::vector<uint8_t>
DbStmt::blob(const std::string& col_name) const
{
    int idx = index_of_column(col_name);
    const void* blob = sqlite3_column_blob(m_stmt, idx);
    int size = sqlite3_column_bytes(m_stmt, idx);
    if (blob && size > 0) {
        const uint8_t* data = static_cast<const uint8_t*>(blob);
        return std::vector<uint8_t>(data, data + size);
    }
    return {}; // empty vector for NULL or empty BLOB
}

std::optional<std::vector<uint8_t>>
DbStmt::blob_optional(int col_index) const
{
    if (is_null(col_index)) {
        return std::nullopt;
    }
    return blob(col_index);
}

std::optional<std::vector<uint8_t>>
DbStmt::blob_optional(const std::string& col_name) const
{
    if (is_null(col_name)) {
        return std::nullopt;
    }
    return blob(col_name);
}

DbVariant
DbStmt::variant(int col_index) const
{
    int type = sqlite3_column_type(m_stmt, col_index);
    switch (type) {
        case SQLITE_INTEGER:
            return integer(col_index);
        case SQLITE_FLOAT:
            return real(col_index);
        case SQLITE_TEXT:
            return text(col_index);
        case SQLITE_BLOB: {
            const void* blob = sqlite3_column_blob(m_stmt, col_index);
            int size = sqlite3_column_bytes(m_stmt, col_index);
            const uint8_t* data = static_cast<const uint8_t*>(blob);
            return std::vector<uint8_t>(data, data + size);
        }
        case SQLITE_NULL:
            return nullptr;
    }
    return nullptr; // unsupported type
}

DbVariant
DbStmt::variant(const std::string& col_name) const
{
    int idx = index_of_column(col_name);
    return variant(idx);
}

int
DbStmt::index_of_column(const std::string& col_name) const
{
    auto it = m_col_name_to_index.find(col_name);
    if (it == m_col_name_to_index.end()) {
        return -1;
    }
    return it->second;
}

Error
DbStmt::row(DbRow* row) const
{
    if (!row) {
        RAISE_LOG(ErrorCode::argument, "DbRow pointer is null");
    }

    int column_count = sqlite3_column_count(m_stmt);

    for (int i = 0; i < column_count; ++i) {    
        row->emplace_back(variant(i));
    }
    SUCCEED();
}

} // namespace orion