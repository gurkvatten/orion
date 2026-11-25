#include <vector>
#include <string>
#include <core/form_analyzer/form_field.h>

namespace orion {

class Form {
private:
    std::string action_url;
    std::vector<FormField> fields;
public:
    Form(std::string action_url);

    //Fluent API
    Form& add_field(FormField field);
    
    const std::vector<FormField>& get_fields() const;
    const std::string& get_action_url() const;

    const FormField* get_field_by_name(const std::string& name) const;
};

} // namespace orion