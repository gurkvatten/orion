#include "safeguard_scripts.h"

#include <format>

namespace orion::gtk {

std::string
get_reader_epoch_script(const std::string_view message_handler)
{
    return std::format(R"(
        window.__epoch = crypto.randomUUID();
        addEventListener("pageshow", (event) => {{
            window.webkit.messageHandlers.{}.postMessage(window.__epoch);
        }})
    )", message_handler);
}

std::string
get_page_tweaks_epoch_script(const std::string_view message_handler)
{
    return std::format(R"(
        window.__epoch = crypto.randomUUID();
        window.__epoch_sender_enabled = true;
        window.webkit.messageHandlers.{}.postMessage(window.__epoch);
        addEventListener("pageshow", (event) => {{
            if(event.persisted) {{
                if(window.__epoch_sender_enabled) {{
                    window.webkit.messageHandlers.{}.postMessage(window.__epoch);
                }}
                else {{
                    window.webkit.messageHandlers.{}.postMessage(null);
                }}
            }}
        }})
    )", message_handler, message_handler, message_handler);
}

std::string run_against_epoch(const std::string_view script, const std::string_view epoch)
{
    constexpr std::string_view template_string = R"(
        if(window.__epoch === '{}') {{
            {}
        }}
    )";
    size_t unescaped_template_length = orion::str::static_format_length(template_string);

    std::string result;
    result.reserve(unescaped_template_length + script.length() + epoch.length());
    std::format_to(std::back_inserter(result), template_string, epoch, script);
    return result;
}

} // namespace orion::gtk