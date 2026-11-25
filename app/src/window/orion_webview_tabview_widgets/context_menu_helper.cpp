#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include "context_menu_helper.h"

#include <webkit/webkit.h>
#include <string>
#include <vector>
#include <set>

#include "util/variant_util.h"

static void
push_back_item_from_gaction(std::vector<WebKitContextMenuItem*>& vec, 
    GActionMap* map, std::string_view action_name, std::string_view label, orion::gtk::VariantWrapper target)
{
    GAction* action = g_action_map_lookup_action(map, action_name.data());

    if(action)
    {
        vec.push_back(webkit_context_menu_item_new_from_gaction(action,
            label.data(), target.get()));
    }
}

ContextMenuHelper::ContextMenuHelper(OrionWebkitWebview* view, OrionWindow* win, const std::string& link_uri, double x, double y, bool enable_link_preview_window)
{
    GActionMap* map = G_ACTION_MAP(win);

    editable_items = {
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_CUT),
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_COPY),
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_PASTE),
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_DELETE),
        webkit_context_menu_item_new_separator(),
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_SELECT_ALL),
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_INSERT_EMOJI),
        webkit_context_menu_item_new_separator(),
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_UNICODE),
    };

    image_items = {
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_OPEN_IMAGE_IN_NEW_WINDOW),
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_IMAGE_TO_DISK),
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_TO_CLIPBOARD),
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_URL_TO_CLIPBOARD),
    };

    {
        link_items = {
            webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_OPEN_LINK),
        };

        push_back_item_from_gaction(link_items, map, "open-uri-in-new-tab", _("Open Link in New Tab"), orion::gtk::VariantWrapper(link_uri));
        push_back_item_from_gaction(link_items, map, "open-uri-in-new-window", _("Open Link in New Window"), orion::gtk::VariantWrapper(link_uri));
        push_back_item_from_gaction(link_items, map, "open-uri-in-new-private-window", _("Open Link in New Private Window"), orion::gtk::VariantWrapper(link_uri));
        if(enable_link_preview_window) 
        {
            push_back_item_from_gaction(link_items, map, "open-uri-in-preview-window", _("Open Link in Preview Window"), orion::gtk::VariantWrapper::Tuple(link_uri, x, y));
        }

        link_items.push_back(webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_LINK_TO_DISK));
        link_items.push_back(webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_COPY_LINK_TO_CLIPBOARD));
    }

    media_items = {
        webkit_context_menu_item_new_from_stock_action_with_label(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_GO_BACK, "NOT IMPLEMENTED")
        //TODO cant play media
    };

    scrollbar_items = {
        webkit_context_menu_item_new_from_stock_action_with_label(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_GO_BACK, "NOT IMPLEMENTED")
        //TODO wont show a contextmenu for some reason
    };

    selection_items = {
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_COPY),
        webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_SELECT_ALL),
        // TODO: add more actions here if needed, like "Search Kagi", "Translate", "Copy Link to Highlight", etc.
    };

    {
        GAction* action = orion_webkit_webview_get_go_back_action(view);
        always_items.push_back(
            webkit_context_menu_item_new_from_gaction(action, _("Go Back"), NULL)
        );
        action = orion_webkit_webview_get_go_forward_action(view);
        always_items.push_back(
            webkit_context_menu_item_new_from_gaction(action, _("Go Forward"), NULL)
        );
        always_items.push_back(
            webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_STOP)
        );
        always_items.push_back(
            webkit_context_menu_item_new_from_stock_action(WebKitContextMenuAction::WEBKIT_CONTEXT_MENU_ACTION_RELOAD)
        );
    }
}

void
ContextMenuHelper::populate(WebKitContextMenu* menu, const std::vector<HitTestUtil::Element>& elements, bool inspector)
{
    webkit_context_menu_remove_all(menu);

    for (size_t element_index = 0; element_index < elements.size(); element_index++)
    {
        const auto & vector = get_correct_vector(elements.at(element_index));
        for (size_t i = 0; i < vector.size(); i++)
        {
            webkit_context_menu_append(menu, vector.at(i));
        }

        if (element_index < elements.size() - 1)
        {
            webkit_context_menu_append(menu, webkit_context_menu_item_new_separator());
        }
        
    }

    if (inspector) {
        webkit_context_menu_append(menu, webkit_context_menu_item_new_separator());
        webkit_context_menu_append(menu, webkit_context_menu_item_new_from_stock_action(WEBKIT_CONTEXT_MENU_ACTION_INSPECT_ELEMENT));
    }
}

std::vector<WebKitContextMenuItem*>&
ContextMenuHelper::get_correct_vector(HitTestUtil::Element element)
{
    switch (element)
    {
    case HitTestUtil::Element::editable:
        return editable_items;
        break;
    case HitTestUtil::Element::image:
        return image_items;
        break;
    case HitTestUtil::Element::link:
        return link_items;
        break;
    case HitTestUtil::Element::media:
        orion::log_warning("TODO: Implement this context menu");
        return media_items;
        break;
    case HitTestUtil::Element::scrollbar:
        orion::log_warning("TODO: Implement this context menu");
        return scrollbar_items;
        break;
    case HitTestUtil::Element::selection:
        return selection_items;
        break;
    case HitTestUtil::Element::all:
        return always_items;
        break;
    }
    return always_items;
}

void
ContextMenuHelper::print_context_menu_actions(WebKitContextMenu* menu)
{
    if (!menu) {
        return;
    }

    GList* items = webkit_context_menu_get_items(menu);
    for (GList* l = items; l != nullptr; l = l->next) {
        WebKitContextMenuItem* item = static_cast<WebKitContextMenuItem*>(l->data);
        WebKitContextMenuAction action = webkit_context_menu_item_get_stock_action(item);

        orion::log_debug("Action ID: %d", static_cast<int>(action));
    }
}

#endif