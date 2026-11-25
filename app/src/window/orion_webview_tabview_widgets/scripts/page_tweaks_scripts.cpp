#include <format>

#include "page_tweaks_scripts.h"

#include <common/src/string_util.h>

namespace orion::gtk {

std::string get_set_already_erased_selectors_script(std::string_view selectors) {
    return std::format(R"(
        (function () {{
            if(window.__orion_element_picker)
            {{
                window.__orion_element_picker.setErasedSelectors({});
            }}
        }})();
    )", orion::str::escape_string_for_js(selectors));
}

}