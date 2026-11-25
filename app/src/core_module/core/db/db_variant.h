#pragma once

#include <variant>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <common/src/dto/dto_member.h>
#include <atomic>

namespace orion {

class DbVariant {
public:    
    enum class Type {
        null             = 0,
        integer          = 1,
        real             = 2,
        text             = 3,
        blob             = 4,
};

public:
    DbVariant() {
        m_type = Type::null;
        m_value.m_null = nullptr;
    }

    DbVariant(std::nullptr_t null)
    {
        m_type = Type::null;
        m_value.m_null = null;
    }

    DbVariant(int64_t integer)
    {
        m_type = Type::integer;
        m_value.m_integer = integer;
    }

    DbVariant(double real)
    {
        m_type = Type::real;
        m_value.m_real = real;
    }

    DbVariant(const std::string& text)
    {
        m_type = Type::text;
        std::construct_at(&m_value.m_text, text); // better than placement new in C++20 and onwards
    }

    DbVariant(const char* text)
    {
        m_type = Type::text;
        std::construct_at(&m_value.m_text, std::string(text)); // better than placement new in C++20 and onwards
    }
    
    DbVariant(const std::vector<uint8_t>& blob)
    {
        m_type = Type::blob;
        std::construct_at(&m_value.m_blob, std::vector<uint8_t>(blob)); // better than placement new in C++20 and onwards
    }

    // Avoids ambiguity between int64_t and double constructors
    DbVariant(bool boolean)
    {
        m_type = Type::integer;
        m_value.m_integer = static_cast<int64_t>(boolean ? 1 : 0);
    }

    DbVariant(const DbVariant& other)
    {
        copy_from(other);
    }

    DbVariant& operator=(const DbVariant& other)
    {
        if (this != &other) {
            destroy();
            copy_from(other);
        }
        return *this;
    }

    DbVariant(DbVariant&& other) noexcept
    {
        move_from(std::move(other));
    }

    DbVariant& operator=(DbVariant&& other) noexcept
    {
        if (this != &other) {
            destroy();
            move_from(std::move(other));
        }
        return *this;
    }

    // DTO convenience constructors
    DbVariant(DtoValue<std::nullptr_t> null)
    {
        m_type = Type::null;
        m_value.m_null = null.value();
    }

    DbVariant(DtoValue<int64_t> integer)
    {
        m_type = Type::integer;
        m_value.m_integer = integer.value();
    }

    DbVariant(DtoValue<double> real)
    {
        m_type = Type::real;
        m_value.m_real = real.value();
    }
    
    DbVariant(DtoValue<std::string> text)
    {
        m_type = Type::text;
        std::construct_at(&m_value.m_text, std::string(text.value())); // better than placement new in C++20 and onwards
    }

    DbVariant(DtoValue<std::vector<uint8_t>> blob)
    {
        m_type = Type::blob;
        std::construct_at(&m_value.m_blob, std::vector<uint8_t>(blob.value())); // better than placement new in C++20 and onwards
    }
    
    DbVariant(DtoValue<bool> boolean)
    {
        m_type = Type::integer;
        m_value.m_integer = static_cast<int64_t>(boolean.value() ? 1 : 0);
    }

    ~DbVariant()
    {
        destroy();
    }

    std::nullptr_t null() const;
    int64_t integer() const;
    std::optional<int64_t> integer_optional() const;
    double real() const;
    const std::string& text() const;
    const std::vector<uint8_t>& blob() const;

    Type type() const;
    //bool is_null() const { return std::holds_alternative<std::nullptr_t>(m_value); }
    bool is_null() const { return m_type == Type::null; }
    
    std::string to_string() const;

private:
    void
    destroy()
    {
        switch (m_type) {
        case Type::null:
            m_value.m_null = nullptr;
            break;
        case Type::integer:
            m_value.m_integer = 0;
            break;
        case Type::real:
            m_value.m_real = 0.0;
            break;
        case Type::text:
            std::destroy_at(&m_value.m_text); // better than calling destructor manually in C++20 and onwards
            break;
        case Type::blob:
            std::destroy_at(&m_value.m_blob); // better than calling destructor manually in C++20 and onwards
            break;
        }
        m_type = Type::null; // reset type to null
    }

    void
    copy_from(const DbVariant& other)
    {
        m_type = other.m_type;
        switch (m_type) {
        case Type::null:
            m_value.m_null = other.m_value.m_null;
            break;
        case Type::integer:
            m_value.m_integer = other.m_value.m_integer;
            break;
        case Type::real:
            m_value.m_real = other.m_value.m_real;
            break;
        case Type::text:
            std::construct_at(&m_value.m_text, other.m_value.m_text); // better than placement new in C++20 and onwards
            break;
        case Type::blob:
            std::construct_at(&m_value.m_blob, std::vector<uint8_t>(other.m_value.m_blob)); // better than placement new in C++20 and onwards
            break;
        }
    }

    void
    move_from(DbVariant&& other)
    {
        m_type = other.m_type;
        switch (m_type) {
        case Type::null:
            m_value.m_null = other.m_value.m_null;
            break;
        case Type::integer:
            m_value.m_integer = other.m_value.m_integer;
            break;
        case Type::real:
            m_value.m_real = other.m_value.m_real;
            break;
        case Type::text:
            std::construct_at(&m_value.m_text, std::string(std::move(other.m_value.m_text))); // better than placement new in C++20 and onwards
            break;
        case Type::blob:
            std::construct_at(&m_value.m_blob, std::vector<uint8_t>(std::move(other.m_value.m_blob))); // better than placement new in C++20 and onwards
            break;
        }
        other.m_type = Type::null; // reset the moved-from variant
        other.m_value.m_null = nullptr; // reset the moved-from value
    }

    void check_type(Type expected_type) const;

private:
    // Note that usage of std::variant<> can cause maybe-uninitialized warnings (errors)
    // with gcc compiler in release due to optimizations (SSO: small string optimization)
    // when trying to move from a variant that contains a small string. Instead, we just
    // use a union approach and avoid these issues completely.
    Type m_type = Type::null;
    
    // We need to use std::construct_at to initialize the union members and std::destroy_at
    // to correctly destroy them - this is the recommended way in C++20 and onwards instead
    // of using placement new and manual destructor calls.
    union DbValue {
        std::nullptr_t m_null;
        int64_t m_integer;
        double m_real;
        std::string m_text;
        std::vector<uint8_t> m_blob;

        DbValue() {}  // do nothing
        ~DbValue() {} // do nothing, destructed manually in DbVariant
    } m_value;
};

typedef std::vector<DbVariant> DbRow;

} // namespace orion