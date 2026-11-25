#include "find_selection_scripts.h"

namespace orion::gtk {

std::string
get_selected_text_script()
{
    return R"(
        (function() {
            const sel = window.getSelection();
            return sel ? sel.toString() : "";
        })();
    )";
}

std::string
get_scroll_to_selection_script()
{
    return R"(
        (function() {
            const sel = window.getSelection();
            if (!sel || sel.rangeCount === 0) return false;
            const range = sel.getRangeAt(0);
            let el = range.startContainer;
            if (el.nodeType === Node.TEXT_NODE) el = el.parentElement;
            if (el && el.scrollIntoView) {
                el.scrollIntoView({ block: "center", inline: "nearest" });
                return true;
            }
            return false;
        })();
    )";
}

} // namespace orion::gtk
