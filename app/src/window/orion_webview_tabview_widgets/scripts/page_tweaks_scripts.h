#include <string>

#include <common/src/string_util.h>

namespace orion::gtk {

constexpr const std::string_view get_disable_sticky_headers_trigger_script()
{
    return R"(
        (function() {
            function handler() {
                window.__epoch_sender_enabled = false;

                document.querySelectorAll("body *").forEach(function(node) {
                    if (["fixed", "sticky"].includes(getComputedStyle(node).position)) {
                        node.parentNode.removeChild(node)
                    }
                });
                document.querySelectorAll("html *").forEach(function(node) {
                    var s = getComputedStyle(node);
                    if ("hidden" === s["overflow"]) {
                        node.style["overflow"] = "visible"
                    }
                    if ("hidden" === s["overflow-x"]) {
                        node.style["overflow-x"] = "visible"
                    }
                    if ("hidden" === s["overflow-y"]) {
                        node.style["overflow-y"] = "visible"
                    }
                });
                var htmlNode = document.querySelector("html");
                htmlNode.style["overflow"] = "visible";
                htmlNode.style["overflow-x"] = "visible";
                htmlNode.style["overflow-y"] = "visible";
            }

            if(document.readyState === 'complete') {
                handler();
            }
            else
            {
                document.addEventListener("readystatechange", () => {
                    if (document.readyState === 'complete') {
                        handler();
                    }
                });
            }

            return window.__epoch;
        })();
    )";
}

std::string get_set_already_erased_selectors_script(std::string_view selectors);

}