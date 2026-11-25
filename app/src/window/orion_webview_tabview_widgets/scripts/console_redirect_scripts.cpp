#include "console_redirect_scripts.h"

namespace orion::gtk {

std::string
get_console_redirect_script()
{
    return R"(
(function () {
    if (window.__consoleRedirected)
        return;
    window.__consoleRedirected = true;

    const levels = ["log", "warn", "error", "debug", "info"];

    // Suppress all webpage console messages (but this suppresses both stdout
    // and Web Inspector). Another option would be to use webkit web-extension:
    // compiled shared library that is packed inside flatpak browser app and
    // that is loaded using webkit_web_context_set_web_extensions_directory.
    // See: https://webkitgtk.org/reference/webkitgtk-web-process-extension/stable/signal.WebPage.console-message-sent.html
    // The problem with this approach is that console-message-sent signal
    // does not provide a way to suppress stdout output, it is only for observing.
    // So for now we just disable all console output here.

    // console.log = () => {};
    // console.warn = () => {};
    // console.error = () => {};
    // console.debug = () => {};
    // console.info = () => {};
    // return;

    // The code below redirects console messages to native C code but they are lost
    // for Web Inspector.

    // for (const level of levels) {
    //     const original = console[level];

    //     console[level] = function (...args) {
    //         try {
    //             window.webkit?.messageHandlers?.console?.postMessage({
    //                 level: level,
    //                 message: args.map(a =>
    //                     typeof a === "string" ? a : JSON.stringify(a)
    //                 ).join(" ")
    //             });
    //         } catch (e) {
    //             /* ignore */
    //         }

    //         // Suppress real output:
    //         // comment out if you want native console too
    //         // original.apply(console, args);
    //     };
    // }
})();
)";
}

} // namespace orion::gtk