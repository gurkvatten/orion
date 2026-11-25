#include <glib/gi18n.h>

#include "toolbar_menu.h"
#include "util/menu_util.h"

void
create_toolbar_button_context_menu(GMenu* menu, orion::ToolbarItemType item_type, orion::ToolbarItemId item_id)
{
    // TODO: implement extension button context menu

    if (item_type == orion::ToolbarItemType::privacy) {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        // TODO: implement persistent setting
        orion::gtk::menu_append_item(section, _("Show Notification"), "win.show-privacy-notification");
    }

    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_item(section, _("Use Mini Toolbar"), "win.use-mini-toolbar");
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_action_with_string_target(section, _("Icon and Text"), "win.toolbar-item-style", "1");
        orion::gtk::menu_append_action_with_string_target(section, _("Icon Only"), "win.toolbar-item-style", "2");
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_action_with_int64_target(
            section, _("Move to Overflow Menu"), "win.move-toolbar-item-to-overflow-menu", item_id.value);
        orion::gtk::menu_append_action_with_int64_target(
            section, _("Remove from Toolbar"), "win.remove-toolbar-item", item_id.value);
        orion::gtk::menu_append_item(section, _("Customize Toolbar…"), "win.customize-menu");
        orion::gtk::menu_append_item(section, _("Reset Toolbar"), "win.reset-toolbar");
    }
}

void
create_empty_space_context_menu(GMenu* menu, orion::ToolbarItemType item_type, orion::ToolbarItemId item_id)
{
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_item(section, _("Use Mini Toolbar"), "win.use-mini-toolbar");
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_action_with_string_target(section, _("Icon and Text"), "win.toolbar-item-style", "1");
        orion::gtk::menu_append_action_with_string_target(section, _("Icon Only"), "win.toolbar-item-style", "2");
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_item(section, _("Bookmarks Bar"), "win.bookmarks-bar-visibility");
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_item(section, _("Customize Toolbar…"), "win.customize-menu");
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_item(section, _("Import Button from URL…"), "win.import-button-from-url-create");
        orion::gtk::menu_append_item(section, _("Import Button from File…"), "win.import-button-from-file-create");
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_item(section, _("Reset Toolbar"), "win.reset-toolbar");
    }
}