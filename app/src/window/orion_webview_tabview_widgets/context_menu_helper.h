#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <vector>
#include <webkit/webkit.h>
#include <string>
#include <functional>
#include <algorithm>
#include <set>
#include <optional>

#include "util/hit_test_util.h"
#include "window/main_window/orion_window_iface.h"
#include "window/orion_webview_tabview_widgets/orion_webkit_webview.h"

class ContextMenuHelper{
public:
    // main_window is owned by the caller of the method.
    ContextMenuHelper(OrionWebkitWebview* view, OrionWindow* win, const std::string& link_uri, double x, double y, bool enable_link_preview_window);

    void populate(WebKitContextMenu* menu, const std::vector<HitTestUtil::Element>& elements, bool inspector);
    void print_context_menu_actions(WebKitContextMenu* menu);

private:
    std::vector<WebKitContextMenuItem*> editable_items;
    std::vector<WebKitContextMenuItem*> image_items;
    std::vector<WebKitContextMenuItem*> link_items;
    
    std::vector<WebKitContextMenuItem*> media_items;
    std::vector<WebKitContextMenuItem*> scrollbar_items;
    std::vector<WebKitContextMenuItem*> selection_items;
    
    std::vector<WebKitContextMenuItem*> always_items;

    std::vector<WebKitContextMenuItem*>& get_correct_vector(HitTestUtil::Element);
};

#endif