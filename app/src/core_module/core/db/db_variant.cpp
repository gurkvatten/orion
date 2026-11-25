#include <core/db/db_variant.h>
#include <common/src/log.h>

namespace orion {

void
DbVariant::check_type(Type expected_type) const
{
    if (m_type != expected_type) {
        log_error("DbVariant::check_type: expected type %d, got %d", static_cast<int>(expected_type), static_cast<int>(m_type));
    }
}

std::nullptr_t
DbVariant::null() const
{
    check_type(Type::null);
    if (m_type != Type::null) {
        return nullptr;
    }
    return m_value.m_null;
}

int64_t
DbVariant::integer() const
{
    check_type(Type::integer);
    if (m_type != Type::integer) {
        return 0;
    }
    return m_value.m_integer;
}

std::optional<int64_t>
DbVariant::integer_optional() const
{
    if (m_type == Type::null) {
        return std::nullopt;
    }
    check_type(Type::integer);
    return m_value.m_integer;
}

double
DbVariant::real() const
{
    check_type(Type::real);
    if (m_type != Type::real) {
        return 0.0;
    }
    return m_value.m_real;
}

const std::string&
DbVariant::text() const
{
    check_type(Type::text);
    if (m_type != Type::text) {
        static const std::string empty_string;
        return empty_string;
    }
    return m_value.m_text;
}

const
std::vector<uint8_t>& DbVariant::blob() const
{
    check_type(Type::blob);
    if (m_type != Type::blob) {
        static const std::vector<uint8_t> empty_blob;
        return empty_blob;
    }
    return m_value.m_blob;
}

DbVariant::Type
DbVariant::type() const
{
    return m_type;
}

std::string
DbVariant::to_string() const
{
    switch (type()) {
        case Type::null:
            return "NULL";
        case Type::integer:
            return std::to_string(integer());
        case Type::real:
            return std::to_string(real());
        case Type::text:
            return "\"" + text() + "\"";
        case Type::blob:
            return "BLOB(" + std::to_string(blob().size()) + ")";
        default:
            return "UNKNOWN";
    }
}

} // namespace orion