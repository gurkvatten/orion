#include <core/form_analyzer/form.h>
#include <string>

namespace orion {

Form::Form(std::string action_url)
    : action_url(std::move(action_url))
{
}

Form&
Form::add_field(FormField field)
{
    fields.push_back(std::move(field));
    return *this;
}

const std::vector<FormField>&
Form::get_fields() const
{
    return fields;
}

const
std::string& Form::get_action_url() const
{
    return action_url;
}

//TODO optimize for O(logn)
const FormField*
Form::get_field_by_name(const std::string& name) const
{
    for (const auto& field : fields) {
        if (field.get_name() == name) {
            return &field;
        }
    }
    return nullptr;
}

} // namespace orion