#include "webextensions/clipboard_host.h"

#include <optional>
#include <string>

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <gtk/gtk.h>
#endif

namespace orion {

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

namespace {

class GtkClipboardHost final : public bridge::ClipboardHost {
public:
    struct State {
        std::weak_ptr<bridge::ClipboardBridge> bridge;
    };

    explicit GtkClipboardHost(bridge::ClipboardBridge& bridge)
        : m_attachment(bridge.attach(*this))
        , m_state(std::make_shared<State>(State {bridge.weak_from_this()}))
    {
    }

    std::optional<std::string>
    clipboard_write_text(const std::string& text) override
    {
        GdkDisplay* display = gdk_display_get_default();
        if(!display) return "system clipboard is unavailable";
        gdk_clipboard_set_text(gdk_display_get_clipboard(display), text.c_str());
        return std::nullopt;
    }

    void
    clipboard_read_text(ReadRequest request) override
    {
        GdkDisplay* display = gdk_display_get_default();
        if(!display) {
            if(auto bridge = m_state->bridge.lock()) {
                bridge->clipboard_read_text_completed(
                    request, std::nullopt, "system clipboard is unavailable");
            }
            return;
        }
        struct ReadData {
            std::shared_ptr<State> state;
            ReadRequest request;
        };
        gdk_clipboard_read_text_async(
            gdk_display_get_clipboard(display), nullptr,
            [](GObject* source, GAsyncResult* result, gpointer user_data) {
                std::unique_ptr<ReadData> data(static_cast<ReadData*>(user_data));
                GError* error = nullptr;
                gchar* text = gdk_clipboard_read_text_finish(
                    GDK_CLIPBOARD(source), result, &error);
                auto bridge = data->state->bridge.lock();
                if(!bridge) {
                    if(error) g_error_free(error);
                    g_free(text);
                    return;
                }
                if(error) {
                    bridge->clipboard_read_text_completed(
                        data->request, std::nullopt, error->message);
                    g_error_free(error);
                    return;
                }
                bridge->clipboard_read_text_completed(
                    data->request, std::string(text ? text : ""), std::nullopt);
                g_free(text);
            },
            new ReadData {m_state, request});
    }

private:
    bridge::HostAttachment m_attachment;
    std::shared_ptr<State> m_state;
};

} // namespace

std::unique_ptr<bridge::ClipboardHost>
create_app_clipboard_host(bridge::ClipboardBridge& bridge)
{
    return std::make_unique<GtkClipboardHost>(bridge);
}

#else

std::unique_ptr<bridge::ClipboardHost>
create_app_clipboard_host(bridge::ClipboardBridge&)
{
    return nullptr;
}

#endif

} // namespace orion
