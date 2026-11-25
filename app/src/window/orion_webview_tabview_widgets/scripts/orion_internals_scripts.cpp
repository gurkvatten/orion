#include "orion_internals_scripts.h"

#include <format>

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

#include <common/src/defer.h>
#include <common/src/log.h>

#include "util/ref_util.h"


namespace orion::gtk {

std::string get_orion_internals_script(std::string_view original_script, int32_t epoch)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(original_script.empty()) return ""; 
    
    JSCContext* ctx = jsc_context_new();
    defer([ctx]{ g_object_unref(ctx); });
    
    JSCException* exception = NULL;
    JSCCheckSyntaxResult syntax_check_res = jsc_context_check_syntax(
        ctx,
        original_script.data(),
        original_script.length(),
        JSC_CHECK_SYNTAX_MODE_SCRIPT,
        "programmable_button_script",
        0,
        &exception
    );

    if(syntax_check_res != JSC_CHECK_SYNTAX_RESULT_SUCCESS)
    {
        
        orion::log_error("%s", jsc_exception_get_message(exception));
        g_object_unref(exception);
        return "";
    }

    return std::format(R"(
        (function (){{
            OrionInternals = {{
                setSidebarContent(content) {{
                    window.webkit.messageHandlers.orionInternals.postMessage({{
                        epoch: {0},
                        method: "setSidebarContent",
                        content: String(content)
                    }});
                }},

                addSidebarContent(content) {{
                    window.webkit.messageHandlers.orionInternals.postMessage({{
                        epoch: {0},
                        method: "addSidebarContent",
                        content: String(content)
                    }});
                }},

                clearSidebarContent() {{
                    window.webkit.messageHandlers.orionInternals.postMessage({{
                        epoch: {0},
                        method: "clearSidebarContent",
                    }});
                }},

                openSidebarContent() {{
                    window.webkit.messageHandlers.orionInternals.postMessage({{
                        epoch: {0},
                        method: "openSidebarContent",
                    }});
                }},

                closeSidebarContent() {{
                    window.webkit.messageHandlers.orionInternals.postMessage({{
                        epoch: {0},
                        method: "closeSidebarContent",
                    }});
                }},

                async getSidebarContent() {{
                    return await window.webkit.messageHandlers.orionInternalsAsync.postMessage({{
                        epoch: {0},
                        method: "getSidebarContent",
                    }});
                }},

                async readerContent() {{
                    return await window.webkit.messageHandlers.orionInternalsAsync.postMessage({{
                        epoch: {0},
                        method: "readerContent",
                    }});
                }},
            }}
            {1}
        }}) ();)",
        epoch, original_script);


#else
    return "";
#endif
}

}