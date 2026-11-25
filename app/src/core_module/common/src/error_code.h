#pragma once

#include <unordered_map>

namespace orion {

#define ERROR_CODES(X) \
    X(ok,                     0) /* success */ \
    X(argument,               1) /* argument error */ \
    X(invalid_state,          2) /* invalid state */ \
    X(db_no_rows,             3) /* no database rows */ \
    X(db_duplicate_migration, 4) /* duplicate database migration */ \
    X(db_prepare_statement,   5) /* failed to prepare database statement */ \
    X(db_bind_value,          6) /* failed to bind database value */ \
    X(db_unknown_variant,     7) /* unsupported DbVariant type */ \
    X(db_tx_active,           8) /* database transaction already active */ \
    X(db_no_tx,               9) /* failed because not in transaction */ \
    X(db_step,               10) /* failed to execute database step */ \
    X(open,                  11) /* failed to open (eg., file) */ \
    X(close,                 12) /* failed to close (eg., file) */ \
    X(io,                    13) /* I/O error */ \
    X(assert,                14) /* assertion failed */ \
    X(not_found,             15) /* no entity was found (and at least one was expected) */ \
    X(invalid_thread,        16) /* operation not allowed on this thread */ \
    X(circular_reference,    17) /* circular reference detected */ \
    X(invalid_type,          18) /* invalid type for operation */ \
    X(not_implemented,       19) /* operation not implemented */ \
    X(threading,             20) /* threading error */ \
    X(invalid_format,        21) /* invalid format */ \
    X(cancelled,             22) /* operation cancelled */ \
    X(compression,           23) /* compression error */ \
    X(exception,             24) /* exception thrown */ \
    X(timeout,               25) /* operation timed out */ \
    X(net_io,                30) /* operation timed out */ \

enum class ErrorCode {
#define ENTRY(name, value) name = value,
    ERROR_CODES(ENTRY)
#undef ENTRY
};

const std::unordered_map<ErrorCode, std::string> g_error_code_names = {
#define ENTRY(name, val) {ErrorCode::name, #name},
    ERROR_CODES(ENTRY)
#undef ENTRY
};

#undef ERROR_CODES

} // namespace orion