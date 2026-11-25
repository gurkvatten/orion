#include <format>

#include <core/scripts/edit_text_on_page_scripts.h>

namespace orion::scripts {

std::string
get_edit_text_on_page_script(bool enable)
{
    return std::format(R"(document.body.contentEditable = {};)", enable ? "true" : "false");
}

} // namespace orion::scripts