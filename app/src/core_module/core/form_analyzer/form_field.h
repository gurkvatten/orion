#include <string>

/*
Represents a single field of a form
*/
namespace orion {


class FormField{
private:
    std::string name;
    std::string value;
public:
    FormField(std::string name, std::string value);

    const std::string& get_name() const;
    const std::string& get_value() const;
};

} // namespace orion