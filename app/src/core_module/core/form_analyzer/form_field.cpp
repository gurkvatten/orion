#include <core/form_analyzer/form_field.h>

namespace orion {

FormField::FormField(std::string name, std::string value)
    : name(std::move(name)), value(std::move(value))
{
}

const std::string&
FormField::get_name() const
{
    return name;
}

const std::string&
FormField::get_value() const
{
    return value;
}

} // namespace orion