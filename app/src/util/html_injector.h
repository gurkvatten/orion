
#include <map>       
#include <string>    
#include <glib.h>

namespace orion {

class HtmlInjector
{
private:
    std::map<std::string, std::string> values;
    std::string output;   

public:
    HtmlInjector&
    set(const std::string& key, const std::string& value);

    std::string
    process(const std::string& input);
};

} // namespace orion