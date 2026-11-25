#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <common/src/log.h>
#include <common/src/defer.h>
#include <core/settings.h>
#include <core/toolbar_item_util.h>

#include "app.h"
#include "customize_toolbar_dialog.h"
#include "overflow_menu_popover.h"
#include "util/dialog_util.h"
#include "util/click_util.h"
#include "columnview/overflow_menu_listview.h"
#include "data_item/overflow_menu_item.h"
#include "data_item/toolbar_item.h"
#include "window/main_window/main_window.h"

struct _CustomizeToolbarDialog
{
	AdwDialog parent;

    GtkButton* done_button;
    GtkPopover* overflow_menu_popover;
    GtkListView* overflow_menu_listview;
    orion::ObserverId overflow_menu_items_observer_id;
    GtkBox* toolbar_items_box;
    GtkListBox* default_toolbar_listbox;
    GtkBox* default_toolbar_box;
    AdwWrapBox* available_toolbar_items_box;
    bool is_compact_tab_layout = false;
    GtkDropDown* toolbar_item_style_dropdown;
};

G_DEFINE_TYPE(CustomizeToolbarDialog, customize_toolbar_dialog, ADW_TYPE_DIALOG)

using DragSourcePrepareCb = GdkContentProvider*(*)(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data);

static void
set_toolbar_item_widget_id(GtkWidget* widget, orion::ToolbarItemId id)
{
    orion::ToolbarItemId* id_ptr = new orion::ToolbarItemId(id);
    g_object_set_data_full(G_OBJECT(widget), "toolbar-item-id", id_ptr, [](gpointer data){
        delete static_cast<orion::ToolbarItemId*>(data);
    });
}

// Returns 0 ID if the widget does not have a toolbar item ID set
static orion::ToolbarItemId
get_toolbar_item_widget_id(GtkWidget* widget)
{
    orion::ToolbarItemId* id_ptr = static_cast<orion::ToolbarItemId*>(
        g_object_get_data(G_OBJECT(widget), "toolbar-item-id")
    );
    if (!id_ptr) {
        return orion::ToolbarItemId{0};
    }
    return *id_ptr;
}

static GtkWidget*
create_toolbar_item_button(const char* icon_name, const char* label)
{
    GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 4));

    GtkButton* button = GTK_BUTTON(gtk_button_new_from_icon_name(icon_name));
    gtk_widget_set_hexpand(GTK_WIDGET(button), FALSE);
    gtk_widget_set_halign(GTK_WIDGET(button), GTK_ALIGN_CENTER);
    //gtk_widget_set_size_request(GTK_WIDGET(button), 32, -1);

    // Disable user interactions with the widget but do not change its visual appearance:
    gtk_widget_set_can_target(GTK_WIDGET(button), FALSE);

    gtk_box_append(box, GTK_WIDGET(button));

    GtkLabel* label_widget = GTK_LABEL(gtk_label_new(label));
    gtk_label_set_wrap(label_widget, TRUE);
    gtk_label_set_xalign(label_widget, 0.5);
    gtk_label_set_max_width_chars(label_widget, 8);
    gtk_label_set_justify(label_widget, GTK_JUSTIFY_CENTER);
    const char* classes[] = {"item-label", NULL};
    gtk_widget_set_css_classes(GTK_WIDGET(label_widget), classes);
    gtk_box_append(box, GTK_WIDGET(label_widget));

    return GTK_WIDGET(box);
}

static GtkWidget*
create_toolbar_item_button_from_dto(orion::ToolbarItemDto dto)
{
    ToolbarItem* item = toolbar_item_new_from_dto(dto);
    defer([item]{ g_object_unref(item); });
    ToolbarItemDetail detail = toolbar_item_get_toolbar_item_details(item);

    if (detail.translated_name.empty()) {
        orion::log_warning(
            "create_toolbar_item_button_from_dto: failed to get toolbar item details for item type %lld",
            dto.item_type
        );
        return NULL;
    }

    return create_toolbar_item_button(detail.icon_name.c_str(), detail.translated_name.c_str());
}

static GtkWidget*
create_toolbar_item_widget_impl(orion::ToolbarItemDto dto)
{
    switch (dto.item_type) {
        case orion::ToolbarItemType::sidebar: {
            GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 4));
            
            GtkBox* box2 = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2));
            gtk_widget_set_halign(GTK_WIDGET(box), GTK_ALIGN_CENTER);
            gtk_box_append(box, GTK_WIDGET(box2));

            GtkButton* button = GTK_BUTTON(gtk_button_new_from_icon_name("dock-left-symbolic"));
            gtk_widget_set_tooltip_text(GTK_WIDGET(button), _("Show Sidebar"));
            gtk_widget_set_can_target(GTK_WIDGET(button), FALSE);
            gtk_box_append(box2, GTK_WIDGET(button));

            GtkLabel* label = GTK_LABEL(gtk_label_new(_("Sidebar")));
            const char* classes[] = {"item-label", NULL};
            gtk_widget_set_css_classes(GTK_WIDGET(label), classes);
            gtk_box_append(box, GTK_WIDGET(label));

            return GTK_WIDGET(box);
        }
        case orion::ToolbarItemType::tab_groups: {
            GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 4));

            GtkBox* button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2));
            {
                // Nice-to-have: reflect the current tab group dynamically
                GtkLabel* label = GTK_LABEL(gtk_label_new(_("Tab Group 1")));
                const char* classes[] = {"window-title-label", NULL};
                gtk_widget_set_css_classes(GTK_WIDGET(label), classes);
                gtk_box_append(button_box, GTK_WIDGET(label));
            }

            GtkButton* button = GTK_BUTTON(gtk_button_new());
            gtk_widget_set_can_target(GTK_WIDGET(button), FALSE);
            gtk_button_set_child(button, GTK_WIDGET(button_box));
            gtk_box_append(box, GTK_WIDGET(button));

            {
                GtkLabel* label = GTK_LABEL(gtk_label_new(_("Tab Groups")));
                const char* classes[] = {"item-label", NULL};
                gtk_widget_set_css_classes(GTK_WIDGET(label), classes);
                gtk_box_append(box, GTK_WIDGET(label));
            }

            return GTK_WIDGET(box);
        }
        case orion::ToolbarItemType::back_forward: {
            GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 4));

            GtkBox* box2 = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2));
            gtk_widget_set_halign(GTK_WIDGET(box2), GTK_ALIGN_CENTER);
            gtk_box_append(box, GTK_WIDGET(box2));

            GtkButton* button = GTK_BUTTON(gtk_button_new_from_icon_name("left-symbolic"));
            gtk_widget_set_can_target(GTK_WIDGET(button), FALSE);
            gtk_box_append(box2, GTK_WIDGET(button));

            GtkButton* button2 = GTK_BUTTON(gtk_button_new_from_icon_name("right-symbolic"));
            gtk_widget_set_can_target(GTK_WIDGET(button2), FALSE);
            gtk_box_append(box2, GTK_WIDGET(button2));

            GtkLabel* label = GTK_LABEL(gtk_label_new(_("Back/Forward")));
            const char* classes[] = {"item-label", NULL};
            gtk_widget_set_css_classes(GTK_WIDGET(label), classes);
            gtk_box_append(box, GTK_WIDGET(label));

            return GTK_WIDGET(box);
        }
        case orion::ToolbarItemType::address_and_search: {
            GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 4));

            GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
            gtk_widget_set_hexpand(GTK_WIDGET(entry), FALSE);
            gtk_widget_set_halign(GTK_WIDGET(entry), GTK_ALIGN_CENTER);
            gtk_entry_set_placeholder_text(entry, _("Search or website name"));
            gtk_widget_set_can_target(GTK_WIDGET(entry), FALSE);
            gtk_box_append(box, GTK_WIDGET(entry));

            GtkLabel* label = GTK_LABEL(gtk_label_new(_("Address and Search")));
            gtk_label_set_wrap(label, TRUE);
            gtk_label_set_xalign(label, 0.5);
            gtk_label_set_max_width_chars(label, 8);
            gtk_label_set_justify(label, GTK_JUSTIFY_CENTER);
            const char* classes[] = {"item-label", NULL};
            gtk_widget_set_css_classes(GTK_WIDGET(label), classes);
            gtk_box_append(box, GTK_WIDGET(label));

            return GTK_WIDGET(box);
        }
        case orion::ToolbarItemType::zoom: {
            GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 4));

            GtkBox* box2 = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2));
            gtk_widget_set_halign(GTK_WIDGET(box2), GTK_ALIGN_CENTER);
            gtk_box_append(box, GTK_WIDGET(box2));

            GtkButton* button = GTK_BUTTON(gtk_button_new_from_icon_name("loupe-minus-large-symbolic"));
            gtk_widget_set_can_target(GTK_WIDGET(button), FALSE);
            gtk_box_append(box2, GTK_WIDGET(button));

            GtkButton* button2 = GTK_BUTTON(gtk_button_new_from_icon_name("loupe-plus-large-symbolic"));
            gtk_widget_set_can_target(GTK_WIDGET(button2), FALSE);
            gtk_box_append(box2, GTK_WIDGET(button2));

            GtkLabel* label = GTK_LABEL(gtk_label_new(_("Zoom")));
            const char* classes[] = {"item-label", NULL};
            gtk_widget_set_css_classes(GTK_WIDGET(label), classes);
            gtk_box_append(box, GTK_WIDGET(label));

            return GTK_WIDGET(box);
        }
        case orion::ToolbarItemType::programmable_button: {
            GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 4));

            // ❖ = Super key (roughly the equivalent of Mac Command key)
            GtkButton* button = GTK_BUTTON(gtk_button_new_with_label("❖"));
            gtk_widget_set_hexpand(GTK_WIDGET(button), FALSE);
            gtk_widget_set_halign(GTK_WIDGET(button), GTK_ALIGN_CENTER);
            const char* button_classes[] = {"title-2", NULL};
            gtk_widget_set_css_classes(GTK_WIDGET(button), button_classes);
            gtk_box_append(box, GTK_WIDGET(button));

            GtkLabel* label_widget = GTK_LABEL(gtk_label_new("Programmable Button"));
            gtk_label_set_wrap(label_widget, TRUE);
            gtk_label_set_xalign(label_widget, 0.5);
            gtk_label_set_max_width_chars(label_widget, 8);
            gtk_label_set_justify(label_widget, GTK_JUSTIFY_CENTER);
            const char* label_classes[] = {"item-label", NULL};
            gtk_widget_set_css_classes(GTK_WIDGET(label_widget), label_classes);
            gtk_box_append(box, GTK_WIDGET(label_widget));

            return GTK_WIDGET(box);
        }
        case orion::ToolbarItemType::privacy:
        case orion::ToolbarItemType::page_tweaks:
        case orion::ToolbarItemType::flexible_space:
        case orion::ToolbarItemType::website_settings:
        case orion::ToolbarItemType::bookmark:
        case orion::ToolbarItemType::new_tab:
        case orion::ToolbarItemType::history:
        case orion::ToolbarItemType::focus_mode:
        case orion::ToolbarItemType::share:
        case orion::ToolbarItemType::tab_overview:
        case orion::ToolbarItemType::downloads:
        case orion::ToolbarItemType::overflow_menu:
        case orion::ToolbarItemType::home:
        case orion::ToolbarItemType::start_page:
        case orion::ToolbarItemType::notes:
        case orion::ToolbarItemType::new_window:
        case orion::ToolbarItemType::new_private_window:
        case orion::ToolbarItemType::print:
        case orion::ToolbarItemType::web_inspector:
        case orion::ToolbarItemType::extensions:
        case orion::ToolbarItemType::toggle_reader:
        case orion::ToolbarItemType::summarize_page:
        case orion::ToolbarItemType::kagi_privacy_pass:
            return create_toolbar_item_button_from_dto(dto);
        default:
            orion::log_verbose(
                "customize_toolbar_dialog_map: unknown toolbar item type %lld, ignoring",
                static_cast<int64_t>(dto.item_type)
            );
            return NULL;
    }
}

static GtkDragSource*
create_drag_source(ToolbarItem* item, DragSourcePrepareCb prepare_cb)
{
    GtkDragSource* drag_source = gtk_drag_source_new();
    gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);
    g_signal_connect_data(drag_source, "prepare", G_CALLBACK(prepare_cb), item,
        [](gpointer data, GClosure* closure){
            g_object_unref(data);
        }, G_CONNECT_DEFAULT
    );
    return drag_source;
}

static void
on_done_button_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_destroy(CustomizeToolbarDialog* win)
{
    if (win->overflow_menu_popover != NULL) {
	    gtk_widget_unparent(GTK_WIDGET(win->overflow_menu_popover));
        win->overflow_menu_popover = NULL;
    }
}

AdwDialog*
customize_toolbar_dialog_new(bool is_compact_tab_layout)
{
	CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(g_object_new(CUSTOMIZE_TOOLBAR_DIALOG_TYPE, NULL));
    dialog->is_compact_tab_layout = is_compact_tab_layout;

    if (is_compact_tab_layout) {
        gtk_widget_set_sensitive(GTK_WIDGET(dialog->toolbar_item_style_dropdown), FALSE);
    }
    return ADW_DIALOG(dialog);
}

static void
on_overflow_menu_listview_activate(GtkColumnView* view, guint pos, gpointer user_data)
{
}

GdkContentProvider*
on_available_box_item_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("on_available_box_item_drag_source_prepare");
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
    std::string name = gtk_widget_get_name(widget);

    auto it = orion::global_toolbar_item_types.find(name);
    if (it == orion::global_toolbar_item_types.end()) {
        orion::log_warning("on_available_box_item_drag_source_prepare: unknown toolbar item name: %s", name.c_str());
        return NULL;
    }
    orion::ToolbarItemType item_type = it->second;

    GtkRoot* root = gtk_widget_get_root(widget);
    if (!MAIN_IS_WINDOW(root)) {
        orion::log_warning("on_available_box_item_drag_source_prepare: root widget is not MainWindow");
        return NULL;
    }
    orion::ProfileId profile_id = main_window_get_current_profile_id(MAIN_WINDOW(root));
    
    orion::ToolbarItemDto dto{
        .profile_id = profile_id,
        .item_type = item_type,
    };
    ToolbarItem* item = toolbar_item_new_from_dto(dto);
    defer([item]{ g_object_unref(item); }); // should the item be unreffed here?
    toolbar_item_set_source(item, ToolbarItemSource::available_toolbar_box);
    GdkContentProvider* prov = gdk_content_provider_new_typed(TOOLBAR_ITEM_TYPE, item);
    return prov;
}

GdkContentProvider*
on_default_toolbar_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("on_default_toolbar_drag_source_prepare: user_data_type=%s", G_OBJECT_TYPE_NAME(user_data));
    if (!CUSTOMIZE_TOOLBAR_IS_DIALOG(user_data)) {
        orion::log_warning("on_default_toolbar_drag_source_prepare: user_data is not CustomizeToolbarDialog");
        return NULL;
    }
    CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(user_data);
    GdkContentProvider* prov = gdk_content_provider_new_typed(
        GTK_TYPE_LIST_BOX,
        dialog->default_toolbar_listbox
    );
    return prov;
}

GdkContentProvider*
on_overflow_menu_toolbar_item_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("on_overflow_menu_toolbar_item_drag_source_prepare");
    if (!GTK_IS_LIST_ITEM(user_data)) {
        orion::log_warning("on_overflow_menu_toolbar_item_drag_source_prepare: user_data is not GtkListItem");
        return NULL;
    }
    ToolbarItem* item = APP_TOOLBAR_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(user_data)));
    GdkContentProvider* prov = gdk_content_provider_new_typed(TOOLBAR_ITEM_TYPE, item);
    return prov;
}

GdkContentProvider*
on_toolbar_box_toolbar_item_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("on_toolbar_box_toolbar_item_drag_source_prepare");
    if (!APP_IS_TOOLBAR_ITEM(user_data)) {
        orion::log_warning("on_toolbar_box_toolbar_item_drag_source_prepare: user_data is not ToolbarItem");
        return NULL;
    }
    ToolbarItem* item = APP_TOOLBAR_ITEM(user_data);
    GdkContentProvider* prov = gdk_content_provider_new_typed(TOOLBAR_ITEM_TYPE, item);
    return prov;
}

static void
handle_toolbar_item_dnd_drop_to_current_toolbar(
    CustomizeToolbarDialog* dialog,
    GtkWidget* widget,
    ToolbarItem* dropped_item,
    gdouble x, 
    gdouble y)
{
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(widget));
    if (!win) {
        orion::log_warning("handle_toolbar_item_dnd_drop_to_current_toolbar: root widget is not MainWindow");
        return;
    }
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    graphene_rect_t rect{};
    if (!gtk_widget_compute_bounds(widget, widget, &rect)) {
        orion::log_warning("handle_toolbar_item_dnd_drop_to_current_toolbar: failed to compute bounds");
        return;
    }

    std::string dropped_name = orion::toolbar_item_type_to_string(toolbar_item_get_item_type(dropped_item));

    orion::ToolbarItemId target_id = get_toolbar_item_widget_id(widget);

    // If both target_id and dropped_item id are 0, this means that the dropped item is from available box
    // and is being dropped on current toolbar box
    if (target_id != orion::ToolbarItemId{0} && toolbar_item_get_id(dropped_item) != orion::ToolbarItemId{0}) {
        if (target_id == toolbar_item_get_id(dropped_item)) {
            orion::log_verbose(
                "handle_toolbar_item_dnd_drop_to_current_toolbar: dropped item is the same as target item, ignoring"
            );
            return;
        }
    }

    orion::log_verbose(
        "handle_toolbar_item_dnd_drop_to_current_toolbar: dropped_item_name=%s target_id=%lld",
        dropped_name.c_str(),
        target_id.value
    );

    // If true, then move the dropped item before the target item, otherwise move it after
    bool move_before = x < (rect.size.width / 2);

    // We must take a strong ref to the dropped item, weak ref is not enough (the item would be freed)
    orion::gtk::StrongRefWrapper dropped_item_strong(dropped_item);

    orion::GetToolbarItemsQuery query{
        .profile_id = profile_id,
        .exclude_in_overflow_menu = true,
        .is_compact_tab_layout = dialog->is_compact_tab_layout,
    };

    orion::gtk::WeakRefWrapper dialog_weak(dialog);

    global_core_api->get_toolbar_items(query,
    [move_before,target_id,dropped_item_strong,profile_id,dialog_weak](orion::GetToolbarItemsReply reply){
        VERIFY_LOG(reply.err);

        auto dialog_strong = dialog_weak.lock();
        if (!dialog_strong) return;
        CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(dialog_strong.get());

        std::set<orion::ToolbarItemType> current_types;
        for (const orion::ToolbarItemDto& dto : reply.items) {
            current_types.insert(dto.item_type);
        }

        ToolbarItem* dropped_item = APP_TOOLBAR_ITEM(dropped_item_strong.get());

        orion::ToolbarItemDto target_item{};
        size_t n_items = reply.items.size();
        size_t target_item_index = 0;
        for (size_t i = 0; i < n_items; i++) {
            if (reply.items[i].id == target_id) {
                target_item_index = i;
                target_item = reply.items[i];
                break;
            }
        }

        orion::ToolbarItemId next_item_id = target_id;

        if (target_item.id != orion::ToolbarItemId{0}) {
            if (!move_before) {
                // If moving after, the dropped item should be placed after the target item,
                // so we need to get the next item and use its ID as next_item_id
                if (target_item_index + 1 < n_items) {
                    const orion::ToolbarItemDto& next_item = reply.items[target_item_index + 1];
                    next_item_id = next_item.id;
                } else {
                    // If there is no next item, it means that the dropped item should be moved to the end
                    next_item_id = orion::ToolbarItemId{0};
                }
            }
        }
        
        orion::log_verbose(
            "handle_toolbar_item_dnd_drop_to_current_toolbar: dropped_item: id=%lld db_position=%lld source=%lld | "
            "drop_target: id=%lld db_position=%lld db_category_position=%lld index=%zu | "
            "move_before=%d next_item_id=%lld",
            toolbar_item_get_id(dropped_item),
            toolbar_item_get_position(dropped_item),
            toolbar_item_get_source(dropped_item),
            target_item.id.value,
            target_item.position,
            target_item.category_position,
            target_item_index,
            move_before,
            next_item_id.value
        );

        switch (toolbar_item_get_source(dropped_item)) {
            case ToolbarItemSource::current_toolbar_box: {
                // Reorder the item within the current toolbar
                orion::MoveToolbarItemQuery move_query{
                    .id = toolbar_item_get_id(dropped_item),
                    .next_item_id = next_item_id,
                };

                global_core_api->move_toolbar_item(move_query, [](orion::Error err){
                    VERIFY_LOG(err);
                });
                break;
            }
            case ToolbarItemSource::available_toolbar_box: {
                // Check if the toolbar item can be created in the toolbar
                bool cannot_create = current_types.find(toolbar_item_get_item_type(dropped_item)) != current_types.end()
                    && !toolbar_item_get_can_be_in_toolbar_multiple_times(dropped_item);
                
                if (cannot_create) {
                    orion::log_verbose("handle_toolbar_item_dnd_drop_to_current_toolbar: item already present in toolbar, ignoring");
                    return;
                }

                // Create a new toolbar item in the current toolbar
                orion::CreateToolbarItemQuery create_query{
                    .profile_id = profile_id,
                    .item_type = toolbar_item_get_item_type(dropped_item),
                    .next_item_id = next_item_id,
                    .is_compact_tab_layout = dialog->is_compact_tab_layout,
                    .in_overflow_menu = false,
                };

                global_core_api->create_toolbar_item(create_query, [](orion::CreateToolbarItemReply reply){
                    VERIFY_LOG(reply.err);
                });
                break;
            }
            default:
                orion::log_warning(
                    "handle_toolbar_item_dnd_drop_to_current_toolbar: unknown source %lld for dropped item",
                    static_cast<int64_t>(toolbar_item_get_source(dropped_item))
                );
                break;
        }
    });
}

gboolean
on_current_toolbar_drop_target_drop(GtkDropTarget* self, const GValue* val, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("on_current_toolbar_drop_target_drop");

    if (!CUSTOMIZE_TOOLBAR_IS_DIALOG(user_data)) {
        orion::log_warning("on_current_toolbar_drop_target_drop: user_data is not CustomizeToolbarDialog");
        return FALSE;
    }

    CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(user_data);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(dialog));
    if (!win) {
        orion::log_warning("on_current_toolbar_drop_target_drop: root widget is not MainWindow");
        return FALSE;
    }
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    if (G_VALUE_HOLDS(val, gtk_list_box_get_type())) {
        orion::log_verbose("on_current_toolbar_drop_target_drop: dropped content is GtkListBox");
        orion::ResetToolbarItemsQuery query{
            .profile_id = profile_id,
            .is_compact_tab_layout = dialog->is_compact_tab_layout,
            .in_toolbar = true,
            .in_overflow_menu = false, // do not reset overflow menu items
        };
        global_core_api->reset_toolbar_items(query, [](orion::Error err){
            VERIFY_LOG(err);
        });
        return TRUE;
    } else if (G_VALUE_HOLDS(val, TOOLBAR_ITEM_TYPE)) {
        ToolbarItem* item = APP_TOOLBAR_ITEM(g_value_get_object(val));
        ToolbarItemSource source = toolbar_item_get_source(item);
        orion::log_verbose(
            "on_current_toolbar_drop_target_drop: dropped content is ToolbarItem: source=%lld",
            static_cast<int64_t>(source)
        );
        switch (source) {
            case ToolbarItemSource::current_toolbar_box:
            case ToolbarItemSource::available_toolbar_box: {
                GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
                if (!widget) {
                    orion::log_warning("on_current_toolbar_drop_target_drop: failed to get widget from event controller");
                    return FALSE;
                }
                handle_toolbar_item_dnd_drop_to_current_toolbar(dialog, widget, item, x, y);
                break;
            }
            default:
                orion::log_verbose("on_current_toolbar_drop_target_drop: dropped item is from unsupported source, ignoring");
                break;
        }
        return TRUE;
    }

    orion::log_verbose(
        "on_current_toolbar_drop_target_drop: unsupported dropped content type: %s",
        G_OBJECT_TYPE_NAME(g_value_get_object(val))
    );
    return FALSE;
}

gboolean
on_available_box_drop_target_drop(GtkDropTarget* self, const GValue* val, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("on_available_box_drop_target_drop");

    // val = item thas is being dropped
    if (!G_VALUE_HOLDS(val, TOOLBAR_ITEM_TYPE)) {
        orion::log_verbose("on_available_box_drop_target_drop: dropped content does not have expected type");
        return FALSE;
    }

    ToolbarItem* dropped_item = APP_TOOLBAR_ITEM(g_value_get_object(val));
    orion::ToolbarItemId id = toolbar_item_get_id(dropped_item);

    if (id == orion::ToolbarItemId{0}) {
        // The dropped item comes from the available box, so we do nothing
        orion::log_verbose("on_available_box_drop_target_drop: dropped item has id 0, ignoring drop");
        return FALSE;
    }

    orion::log_verbose(
        "on_available_box_drop_target_drop: deleting toolbar item: id=%lld item_type=%s position=%lld",
        id.value,
        orion::toolbar_item_type_to_string(toolbar_item_get_item_type(dropped_item)).c_str(),
        toolbar_item_get_position(dropped_item)
    );

    global_core_api->delete_toolbar_items({id}, [](orion::Error err){
        VERIFY_LOG(err);
    });

    return TRUE;
}

gboolean
on_overflow_menu_toolbar_item_drop_target_drop(
    GtkDropTarget* self, const GValue* val, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("on_overflow_menu_toolbar_item_drop_target_drop");

    // val = item thas is being dropped
    if (!G_VALUE_HOLDS(val, TOOLBAR_ITEM_TYPE)) {
        orion::log_verbose("on_overflow_menu_toolbar_item_drop_target_drop: dropped content does not have expected type");
        return FALSE;
    }

    ToolbarItem* dropped_item = APP_TOOLBAR_ITEM(g_value_get_object(val));
    orion::ToolbarItemId next_item_id{0};
    
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
    AdwDialog* adw_dialog = orion::gtk::get_parent_adw_dialog(widget);
    if (!adw_dialog || !CUSTOMIZE_TOOLBAR_IS_DIALOG(adw_dialog)) {
        orion::log_warning("on_overflow_menu_toolbar_item_drop_target_drop: failed to get parent CustomizeToolbarDialog");
        return FALSE;
    }
    CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(adw_dialog);

    if (GTK_IS_LIST_ITEM(user_data)) {
        // We are dropping on an existing item in the list
        GtkListItem* listitem = GTK_LIST_ITEM(user_data);

        GtkWidget* button = gtk_list_item_get_child(listitem);
        graphene_rect_t row_rectangle;
        if (!gtk_widget_compute_bounds(button, button, &row_rectangle)) {
            orion::log_warning("on_overflow_menu_toolbar_item_drop_target_drop: failed to compute bounds");
            return FALSE;
        }

        ToolbarItem* target_item = APP_TOOLBAR_ITEM(gtk_list_item_get_item(listitem));

        if (toolbar_item_get_id(dropped_item) == toolbar_item_get_id(target_item)) {
            orion::log_verbose("on_overflow_menu_toolbar_item_drop_target_drop: source and target same, ignoring drop");
            return FALSE;
        }

        uint list_index = 0;

        GtkListView* view = GTK_LIST_VIEW(gtk_widget_get_ancestor(button, GTK_TYPE_LIST_VIEW));
        GListModel* store = G_LIST_MODEL(gtk_no_selection_get_model(GTK_NO_SELECTION(gtk_list_view_get_model(view))));

        uint n_items = g_list_model_get_n_items(store);
        uint i = 0;
        for (; i < n_items; i++) {
            orion::gtk::GObjectPtr ref(G_OBJECT(g_list_model_get_item(store, i)));
            if ((gpointer)ref.get() == target_item) {
                list_index = i;
                break;
            }
        }

        if (i == n_items) {
            //only reachable if this_item is somehow not part of the glistmodel, should never happen
            orion::log_warning("on_overflow_menu_toolbar_item_drop_target_drop: target item not found in list model");
            return FALSE;
        }

        // If true, then move the dropped item above the target item, otherwise move it below
        bool move_above = y < (row_rectangle.size.height / 2);

        next_item_id = toolbar_item_get_id(target_item);

        if (!move_above) {
            // If moving below, the dropped item should be placed after the target item,
            // so we need to get the next item and use its ID as next_item_id
            if (list_index + 1 < n_items) {
                orion::gtk::GObjectPtr ref(G_OBJECT(g_list_model_get_item(store, list_index + 1)));
                ToolbarItem* next_item = APP_TOOLBAR_ITEM(ref.get());
                next_item_id = toolbar_item_get_id(next_item);
            } else {
                // If there is no next item, it means that the dropped item should be moved to the end
                next_item_id = orion::ToolbarItemId{0};
            }
        }

        orion::log_verbose(
            "on_overflow_menu_toolbar_item_drop_target_drop: source: id=%lld db_position=%lld | "
            "drop_target: id=%lld db_position=%lld list_index=%u | move_above=%d next_item_id=%lld",
            toolbar_item_get_id(dropped_item),
            toolbar_item_get_position(dropped_item),
            toolbar_item_get_id(target_item),
            toolbar_item_get_position(target_item),
            list_index,
            move_above,
            next_item_id.value
        );
    } else if (CUSTOMIZE_TOOLBAR_IS_DIALOG(user_data)) {
        // We are dropping on the blank space at the end of the list, so we move the item to the end
        orion::log_verbose("on_overflow_menu_toolbar_item_drop_target_drop: dropping on blank space of listview");
        next_item_id = orion::ToolbarItemId{0};
    } else {
        orion::log_warning("on_overflow_menu_toolbar_item_drop_target_drop: unexpected drop target type");
        return FALSE;
    }

    if (toolbar_item_get_id(dropped_item) == orion::ToolbarItemId(0)) {
        // The dropped item comes from the available box

        if (!toolbar_item_get_can_be_in_overflow_menu(dropped_item)) {
            return FALSE;
        }

        orion::ProfileId profile_id = toolbar_item_get_profile_id(dropped_item);
        orion::GetToolbarItemsQuery query{
            .profile_id = profile_id,
            .is_compact_tab_layout = dialog->is_compact_tab_layout,
        };

        orion::gtk::WeakRefWrapper dropped_item_weak(dropped_item);
        // We need to use a strong ref because otherwise the dropped item could already be destroyed
        // in async callbacks below
        orion::gtk::StrongRefWrapper dropped_item_strong(dropped_item);
        orion::gtk::WeakRefWrapper dialog_weak(dialog);

        global_core_api->get_toolbar_items(query,
        [profile_id,next_item_id,dropped_item_weak,dropped_item_strong=std::move(dropped_item_strong),dialog_weak]
        (orion::GetToolbarItemsReply reply){
            VERIFY_LOG(reply.err);

            auto dialog_strong = dialog_weak.lock();
            if (!dialog_strong) return;
            CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(dialog_strong.get());

            ToolbarItem* dropped_item = APP_TOOLBAR_ITEM(dropped_item_strong.get());

            orion::log_verbose(
                "on_overflow_menu_toolbar_item_drop_target_drop: creating new toolbar item with action=%s",
                orion::toolbar_item_type_to_string(toolbar_item_get_item_type(dropped_item)).c_str()
            );

            if (!toolbar_item_get_can_be_in_overflow_menu_multiple_times(dropped_item)) {
                for (const orion::ToolbarItemDto& dto : reply.items) {
                    if (!dto.in_overflow_menu) continue; // ignore items that are not in the overflow menu
                    if (dto.item_type == toolbar_item_get_item_type(dropped_item)) {
                        orion::log_verbose(
                            "on_overflow_menu_toolbar_item_drop_target_drop: item with action=%s already exists, not creating another one",
                            orion::toolbar_item_type_to_string(toolbar_item_get_item_type(dropped_item)).c_str()
                        );
                        return;
                    }
                }
            }

            orion::CreateToolbarItemQuery query{
                .profile_id = profile_id,
                .item_type = toolbar_item_get_item_type(dropped_item),
                .next_item_id = next_item_id,
                .is_compact_tab_layout = dialog->is_compact_tab_layout,
                .in_overflow_menu = true, // new item is added to the overflow menu by default
            };

            global_core_api->create_toolbar_item(query,
            [item_type=toolbar_item_get_item_type(dropped_item)](orion::CreateToolbarItemReply reply){
                VERIFY_LOG(reply.err);
                orion::log_verbose(
                    "created toolbar item: id=%lld item_type=%s",
                    reply.id.value,
                    orion::toolbar_item_type_to_string(item_type).c_str()
                );
            });
        });
        return TRUE;
    }
    else
    {
        orion::MoveToolbarItemQuery query{
            .id = toolbar_item_get_id(dropped_item),
            .next_item_id = next_item_id,
        };

        global_core_api->move_toolbar_item(query, [](orion::Error err){
            VERIFY_LOG(err);
        });
        return TRUE;
    }
}

GdkDragAction
on_overflow_menu_toolbar_item_drop_target_motion(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("on_overflow_menu_toolbar_item_drop_target_motion");
    
    GtkListItem* listitem = GTK_LIST_ITEM(user_data);
    GtkWidget* row_widget = gtk_list_item_get_child(listitem);

    ToolbarItem* this_item = APP_TOOLBAR_ITEM(gtk_list_item_get_item(listitem));

    graphene_rect_t row_rectangle{};
    const char* css_classes[] = {NULL, NULL}; // two elements to allow for a NULL terminator after possible first element

    if (gtk_widget_compute_bounds(row_widget, row_widget, &row_rectangle)) {
        if (y < (row_rectangle.size.height / 2)) {
            css_classes[0] = "dnd-highlight-top";
        } else {
            css_classes[0] = "dnd-highlight-bottom";
        }
    }

    gtk_widget_set_css_classes(row_widget, css_classes);

    return GDK_ACTION_MOVE;
}

GdkDragAction
on_current_toolbar_drop_target_motion(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("on_current_toolbar_drop_target_motion");

    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));

    if (!widget) {
        orion::log_warning("on_current_toolbar_drop_target_motion: failed to get widget from event controller");
        return GDK_ACTION_MOVE;
    }

    graphene_rect_t rect{};
    const char* css_classes[] = {NULL, NULL}; // two elements to allow for a NULL terminator after possible first element

    if (gtk_widget_compute_bounds(widget, widget, &rect)) {
        if (x < (rect.size.width / 2)) {
            css_classes[0] = "dnd-highlight-left-static";
        } else {
            css_classes[0] = "dnd-highlight-right-static";
        }
    }

    gtk_widget_set_css_classes(widget, css_classes);

    return GDK_ACTION_MOVE;
}

GdkDragAction
on_overflow_menu_toolbar_item_drop_target_leave(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("on_overflow_menu_toolbar_item_drop_target_leave");
    GtkListItem* listitem = GTK_LIST_ITEM(user_data);

    const char* empty_string_array[1] = {NULL};
    gtk_widget_set_css_classes(gtk_list_item_get_child(listitem), empty_string_array);
    return GDK_ACTION_MOVE;
}

GdkDragAction
on_current_toolbar_drop_target_leave(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("on_current_toolbar_drop_target_leave");

    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));

    if (!widget) {
        orion::log_warning("on_current_toolbar_drop_target_leave: failed to get widget from event controller");
        return GDK_ACTION_MOVE;
    }

    const char* empty_string_array[1] = {NULL};
    gtk_widget_set_css_classes(widget, empty_string_array);
    return GDK_ACTION_MOVE;
}

gboolean
on_overflow_menu_toolbar_item_drop_target_accept(GtkDropTarget* self, GdkDrop* drop, gpointer user_data)
{
    orion::log_verbose("on_overflow_menu_toolbar_item_drop_target_accept: user_data_type=%s", G_OBJECT_TYPE_NAME(user_data));

    // Filter out default toolbar items list box - only individual toolbar items are accepted as drag content
    GdkContentProvider* provider = gdk_drag_get_content(gdk_drop_get_drag(drop));
    GValue drop_value = G_VALUE_INIT;
    g_value_init(&drop_value, TOOLBAR_ITEM_TYPE);
    GError* error = NULL;
    if (!gdk_content_provider_get_value(provider, &drop_value, &error)) {
        defer([error]{ g_error_free(error); });
        defer([&drop_value]{ g_value_unset(&drop_value); });
        orion::log_warning(
            "on_overflow_menu_toolbar_item_drop_target_accept: failed to get value from content provider: %s",
            error->message
        );
        return FALSE;
    }

    ToolbarItem* item = APP_TOOLBAR_ITEM(g_value_get_object(&drop_value));
    ToolbarItemSource source = toolbar_item_get_source(item);
    if (source != ToolbarItemSource::overflow_menu_listview && source != ToolbarItemSource::available_toolbar_box) {
        orion::log_verbose(
            "on_overflow_menu_toolbar_item_drop_target_accept: dropped item source is not overflow menu nor available box, ignoring drop"
        );
        return FALSE;
    }

    return TRUE;
}

gboolean
on_current_toolbar_drop_target_accept(GtkDropTarget* self, GdkDrop* drop, gpointer user_data)
{
    orion::log_verbose("on_current_toolbar_drop_target_accept");

    // Check if the dragged item is from default toolbar list box
    GdkContentProvider* provider = gdk_drag_get_content(gdk_drop_get_drag(drop));
    {
        GValue drop_value = G_VALUE_INIT;
        g_value_init(&drop_value, gtk_list_box_get_type());
        defer([&drop_value]{ g_value_unset(&drop_value); });

        if (gdk_content_provider_get_value(provider, &drop_value, NULL)) {
            // The dragged item is from default toolbar list box, accept it
            return TRUE;
        }
    }

    orion::log_verbose(
        "on_current_toolbar_drop_target_accept: unsupported dropped content type"
    );
    return FALSE;
}

gboolean
on_current_toolbar_toolbar_item_drop_target_accept(GtkDropTarget* self, GdkDrop* drop, gpointer user_data)
{
    orion::log_verbose("on_current_toolbar_toolbar_item_drop_target_accept");

    GdkContentProvider* provider = gdk_drag_get_content(gdk_drop_get_drag(drop));
    {
        GValue drop_value = G_VALUE_INIT;
        g_value_init(&drop_value, TOOLBAR_ITEM_TYPE);
        defer([&drop_value]{ g_value_unset(&drop_value); });

        if (gdk_content_provider_get_value(provider, &drop_value, NULL)) {
            // The dragged item is from current or available toolbar box
            ToolbarItem* item = APP_TOOLBAR_ITEM(g_value_get_object(&drop_value));
            ToolbarItemSource source = toolbar_item_get_source(item);
            orion::log_verbose(
                "on_current_toolbar_toolbar_item_drop_target_accept: dragged item source=%d",
                source
            );
            return TRUE;
        }
    }
    return FALSE;
}

gboolean
on_available_box_drop_target_accept(GtkDropTarget* self, GdkDrop* drop, gpointer user_data)
{
    orion::log_verbose("on_available_box_drop_target_accept: user_data_type=%s", G_OBJECT_TYPE_NAME(user_data));

    GdkContentProvider* provider = gdk_drag_get_content(gdk_drop_get_drag(drop));
    GValue drop_value = G_VALUE_INIT;
    g_value_init(&drop_value, TOOLBAR_ITEM_TYPE);
    GError* error = NULL;
    if (!gdk_content_provider_get_value(provider, &drop_value, &error)) {
        defer([error]{ g_error_free(error); });
        defer([&drop_value]{ g_value_unset(&drop_value); });
        orion::log_warning(
            "on_available_box_drop_target_accept: failed to get value from content provider: %s",
            error->message
        );
        return FALSE;
    }
 
    ToolbarItem* item = APP_TOOLBAR_ITEM(g_value_get_object(&drop_value));
    ToolbarItemSource source = toolbar_item_get_source(item);
    if (source != ToolbarItemSource::current_toolbar_box && source != ToolbarItemSource::overflow_menu_listview) {
        orion::log_verbose(
            "on_available_box_drop_target_accept: dragged item source is not current toolbar nor overflow menu, refusing to accept"
        );
        return FALSE;
    }

    return TRUE;
}

static GtkDropTarget*
current_toolbar_create_drop_target(CustomizeToolbarDialog* dialog)
{
    GtkDropTarget* drop_target = gtk_drop_target_new(TOOLBAR_ITEM_TYPE, GDK_ACTION_MOVE);
    g_signal_connect(
        drop_target, "drop", G_CALLBACK(on_current_toolbar_drop_target_drop), dialog
    );
    g_signal_connect(
        drop_target, "motion", G_CALLBACK(on_current_toolbar_drop_target_motion), NULL
    );
    g_signal_connect(
        drop_target, "leave", G_CALLBACK(on_current_toolbar_drop_target_leave), NULL
    );
    g_signal_connect(
        drop_target, "accept", G_CALLBACK(on_current_toolbar_toolbar_item_drop_target_accept), NULL
    );
    //gtk_drop_target_async_set_formats()
    return drop_target;
}

static GtkWidget*
create_toolbar_item_widget(CustomizeToolbarDialog* dialog, orion::ToolbarItemDto dto, ToolbarItemSource source)
{
    GtkWidget* widget = create_toolbar_item_widget_impl(dto);
    if (!widget) return NULL;

    gtk_widget_set_name(widget, orion::toolbar_item_type_to_string(dto.item_type).c_str());

    // Store toolbar item ID in widget data
    set_toolbar_item_widget_id(widget, dto.id);

    ToolbarItem* item = toolbar_item_new_from_dto(dto);
    toolbar_item_set_source(item, source);

    switch (source) {
        case ToolbarItemSource::current_toolbar_box: {
            GtkDragSource* drag_source = create_drag_source(item, on_toolbar_box_toolbar_item_drag_source_prepare);
            gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(drag_source));
            GtkDropTarget* drag_target = current_toolbar_create_drop_target(dialog);
            gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(drag_target));
            break;
        }
        case ToolbarItemSource::available_toolbar_box: {
            GtkDragSource* drag_source = create_drag_source(item, on_available_box_item_drag_source_prepare);
            gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(drag_source));
            break;
        }
        default:
            // Do not set drag source nor drag target
            break;
    }
    return widget;
}

static GtkWidget*
create_current_toolbar_item_widget(CustomizeToolbarDialog* dialog, orion::ToolbarItemDto dto)
{
    GtkWidget* widget = create_toolbar_item_widget(dialog, dto, ToolbarItemSource::current_toolbar_box);
    if (!widget) {
        orion::log_warning(
            "create_current_toolbar_item_widget: failed to create widget for toolbar item id=%lld item_type=%s",
            dto.id.value,
            orion::toolbar_item_type_to_string(dto.item_type).c_str()
        );
        return NULL;
    }
    return widget;
}

static void
insert_current_toolbar_item_widget_at_position(CustomizeToolbarDialog* self, GtkWidget* widget, int64_t position)
{
    if (!widget) return;

    GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(self->toolbar_items_box));
    if (!child) {
        gtk_box_append(self->toolbar_items_box, widget);
        return;
    }

    if (position <= 0) {
        // Insert at the beginning
        gtk_box_insert_child_after(self->toolbar_items_box, widget, NULL);
        return;
    }

    int64_t n = 0;
    while (child) {
        n++;
        if (n == position) {
            gtk_box_insert_child_after(self->toolbar_items_box, widget, child);
            return;
        }
        child = gtk_widget_get_next_sibling(child);
    }

    gtk_box_append(self->toolbar_items_box, widget);
}

static void
sync_available_toolbar_item_disabled_state(
    GtkWidget* widget,
    orion::ToolbarItemDto dto,
    const std::vector<orion::ToolbarItemDto>& current_items)
{
    std::set<orion::ToolbarItemType> current_toolbar_item_types;
    for (const orion::ToolbarItemDto& dto : current_items) {
        current_toolbar_item_types.insert(dto.item_type);
    }

    ToolbarItem* item = toolbar_item_new_from_dto(dto);
    defer([item]{ g_object_unref(item); });
    
    // Mark all items that are already in the current toolbar and may not be in it more than once
    bool should_disable = current_toolbar_item_types.find(dto.item_type) != current_toolbar_item_types.end()
        && !toolbar_item_get_can_be_in_toolbar_multiple_times(APP_TOOLBAR_ITEM(item));

    if (should_disable) {
        // Do not set sensitive=FALSE, it would make the item not draggable
        gtk_widget_add_css_class(widget, "fake-disabled");
    } else {
        gtk_widget_remove_css_class(widget, "fake-disabled");
    }
}

static void
update_available_toolbar_item_disabled_state(
    CustomizeToolbarDialog* dialog,
    GtkWidget* widget,
    orion::ToolbarItemDto dto)
{
    if (!widget) return;

    orion::GetToolbarItemsQuery query{
        .profile_id = dto.profile_id,
        .exclude_in_overflow_menu = true,
        .is_compact_tab_layout = dialog->is_compact_tab_layout,
    };

    orion::gtk::WeakRefWrapper widget_weak(widget);

    global_core_api->get_toolbar_items(query, [widget_weak,dto](orion::GetToolbarItemsReply reply){
        VERIFY_LOG(reply.err);

        auto widget_strong = widget_weak.lock();
        if (!widget_strong) return;
        GtkWidget* widget = GTK_WIDGET(widget_strong.get());
        
        sync_available_toolbar_item_disabled_state(widget, dto, reply.items);
    });
}

static GtkWidget*
find_first_available_toolbar_item_widget_by_type(CustomizeToolbarDialog* self, orion::ToolbarItemType item_type)
{
    for (GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(self->available_toolbar_items_box));
         child != NULL;
         child = gtk_widget_get_next_sibling(child))
    {
        const char* name = gtk_widget_get_name(child);
        if (!name) {
            orion::log_warning("find_first_available_toolbar_item_widget_by_type: child widget has no name, skipping");
            continue;
        }
        orion::ToolbarItemType child_item_type = orion::global_toolbar_item_types.at(name);
        if (child_item_type == item_type) {
            return child;
        }
    }
    return NULL;
}

static void
handle_toolbar_item_create(CustomizeToolbarDialog* self, orion::ToolbarItemDto dto)
{
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!win) {
        orion::log_warning("handle_toolbar_item_create: root widget is not MainWindow");
        return;
    }
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    if (dto.in_overflow_menu) {
        GtkSelectionModel* selection = gtk_list_view_get_model(self->overflow_menu_listview);
        GListModel* list = gtk_no_selection_get_model(GTK_NO_SELECTION(selection));

        ToolbarItem* item = toolbar_item_new_from_dto(dto);
        defer([item]{ g_object_unref(item); });
        toolbar_item_set_source(item, ToolbarItemSource::overflow_menu_listview);

        // Insert new item at the correct position
        uint n_items = g_list_model_get_n_items(list);
        uint i = 0;
        for (; i < n_items; i++) {
            orion::gtk::GObjectPtr ref(G_OBJECT(g_list_model_get_item(list, i)));
            if (toolbar_item_get_position(APP_TOOLBAR_ITEM(ref.get())) > dto.position) {
                break;
            }
        }
        orion::gtk::WeakRefWrapper item_weak(item);
        orion::GetProgrammableButtonsQuery query{};
        query.ids = {PROGRAMMABLE_BUTTON_ID(dto.id)};
        global_core_api->get_programmable_buttons(query, [item_weak](orion::GetProgrammableButtonsReply reply){
            VERIFY_LOG(reply.err);

            if(!(reply.items.size() == 1)) return;
            auto item_strong = item_weak.lock();
            if(!item_strong) return;

            std::string title = reply.items.at(0).title;
            if (title.empty()) {
                // Fallback to default title
                title = _("Programmable Button");
            }
            g_object_set(item_strong.get(), "translated-name", title.c_str(), NULL);
        });

        g_list_store_insert(G_LIST_STORE(list), i, item);
    } else {
        // Add the item current toolbar items box
        {
            GtkWidget* widget = create_current_toolbar_item_widget(self, dto);
            if (!widget) return;
            insert_current_toolbar_item_widget_at_position(self, widget, dto.position);
        }

        // Update disabled state of corresponding available toolbar item
        {
            GtkWidget* widget = find_first_available_toolbar_item_widget_by_type(self, dto.item_type);
            if (!widget) return;

            orion::GetToolbarItemsQuery query{
                .profile_id = profile_id,
                .exclude_in_overflow_menu = true,
                .is_compact_tab_layout = self->is_compact_tab_layout,
            };

            orion::gtk::WeakRefWrapper widget_weak(widget);

            global_core_api->get_toolbar_items(query,
            [profile_id,dto,widget_weak](orion::GetToolbarItemsReply reply){
                VERIFY_LOG(reply.err);

                auto widget_strong = widget_weak.lock();
                if (!widget_strong) return;
                GtkWidget* widget = GTK_WIDGET(widget_strong.get());

                sync_available_toolbar_item_disabled_state(widget, dto, reply.items);
            });
        }
    }
}

static void
handle_toolbar_item_delete_from_overflow_menu(CustomizeToolbarDialog* self, orion::ToolbarItemId id)
{
    GtkSelectionModel* selection = gtk_list_view_get_model(self->overflow_menu_listview);
    GListModel* list = gtk_no_selection_get_model(GTK_NO_SELECTION(selection));

    uint n_items = g_list_model_get_n_items(list);
    for (uint i = 0; i < n_items; i++) {
        orion::gtk::GObjectPtr ref(G_OBJECT(g_list_model_get_item(list, i)));
        if (toolbar_item_get_id(APP_TOOLBAR_ITEM(ref.get())) == id) {
            g_list_store_remove(G_LIST_STORE(list), i);
            break;
        }
    }
}

static void
handle_toolbar_item_delete_from_toolbar(CustomizeToolbarDialog* self, orion::ToolbarItemDto dto)
{
    // Remove the item from the current toolbar box
    {
        for (GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(self->toolbar_items_box));
             child != NULL;
             child = gtk_widget_get_next_sibling(child))
        {
            orion::ToolbarItemId widget_id = get_toolbar_item_widget_id(child);
            if (widget_id == dto.id) {
                orion::log_verbose(
                    "handle_toolbar_item_delete_from_toolbar: removing widget with id=%lld",
                    dto.id.value
                );
                gtk_box_remove(self->toolbar_items_box, child);
                break;
            }
        }
    }

    // Refresh the item state in available toolbar box
    {
        for (GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(self->available_toolbar_items_box));
             child != NULL;
             child = gtk_widget_get_next_sibling(child))
        {
            const char* name = gtk_widget_get_name(child);
            if (!name) {
                orion::log_warning("handle_toolbar_item_delete_from_toolbar: child widget has no name, skipping");
                continue;
            }
            orion::ToolbarItemType item_type = orion::global_toolbar_item_types.at(name);
            if (item_type == dto.item_type) {
                update_available_toolbar_item_disabled_state(self, child, dto);
            }
        }
    }
}

static void
register_toolbar_items_observer(CustomizeToolbarDialog* self, orion::ProfileId profile_id)
{
    global_core_api->toolbar_items_change().clear_observer(&self->overflow_menu_items_observer_id);

    orion::gtk::WeakRefWrapper self_weak(self);

    self->overflow_menu_items_observer_id = global_core_api->toolbar_items_change().add_observer(
    [self_weak,profile_id](const orion::ToolbarItemsChange& change){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        CustomizeToolbarDialog* self = CUSTOMIZE_TOOLBAR_DIALOG(self_strong.get());

        for (const orion::ToolbarItemDto& dto : change.updated) {
            if (dto.profile_id != profile_id) continue;
            if (dto.is_compact_tab_layout != self->is_compact_tab_layout) continue;

            orion::log_verbose(
                "register_toolbar_items_observer: updated item id=%lld position=%lld category_position=%lld item_type=%s",
                dto.id.value,
                dto.position,
                dto.category_position,
                orion::toolbar_item_type_to_string(dto.item_type).c_str()
            );

            // Try to delete the item from both Overflow Menu and Current Toolbar (does nothing in both cases if
            // the item is missing in the list already)
            handle_toolbar_item_delete_from_overflow_menu(self, dto.id);
            handle_toolbar_item_delete_from_toolbar(self, dto);

            // Recreate the item in the appropriate list
            handle_toolbar_item_create(self, dto);
        }

        for (const orion::ToolbarItemDto& dto : change.deleted) {
            orion::log_verbose("toolbar_items_change: deleted item id=%lld", dto.id.value);
            handle_toolbar_item_delete_from_overflow_menu(self, dto.id);
            handle_toolbar_item_delete_from_toolbar(self, dto);
        }

        for (const orion::ToolbarItemDto& dto : change.created) {
            orion::log_verbose(
                "toolbar_items_change: created item id=%lld position=%lld category_position=%lld item_type=%s",
                dto.id.value,
                dto.position,
                dto.category_position,
                orion::toolbar_item_type_to_string(dto.item_type).c_str()
            );
            handle_toolbar_item_create(self, dto);
        }
    });
}

static void
on_toolbar_item_style_dropdown_map(GtkWidget* widget, gpointer user_data)
{
    MainWindow* win = orion::gtk::get_root_main_window(widget);
    if (!win) {
        orion::log_warning("on_toolbar_item_style_dropdown_map: root widget is not MainWindow");
        return;
    }
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper dropdown_weak(GTK_DROP_DOWN(widget));
    CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(orion::gtk::get_parent_adw_dialog(widget));
    
    if (dialog->is_compact_tab_layout) {
        gtk_drop_down_set_selected(dialog->toolbar_item_style_dropdown, 1); // 1 = Icon Only (always in Compact Tabs mode)
        return;
    }

    global_core_api->get_setting(profile_id, orion::settings::appearance::toolbar_item_style,
    [dropdown_weak](orion::GetSettingReply<orion::settings::ToolbarItemStyle> reply){
        VERIFY_LOG(reply.err);

        auto dropdown_strong = dropdown_weak.lock();
        if (!dropdown_strong) return;
        GtkDropDown* dropdown = GTK_DROP_DOWN(dropdown_strong.get());

        gtk_drop_down_set_selected(dropdown, static_cast<guint>(reply.value) - 1);
    });
}

static void
on_toolbar_item_style_dropdown_notify_selected(GObject* object, GParamSpec* pspec, gpointer user_data)
{
    GtkDropDown* dropdown = GTK_DROP_DOWN(object);
    guint selected = gtk_drop_down_get_selected(dropdown) + 1;
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(dropdown));
    if (!win) {
        orion::log_warning("on_toolbar_item_style_dropdown_notify_selected: root widget is not MainWindow");
        return;
    }
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    global_core_api->set_setting(
        profile_id,
        orion::settings::appearance::toolbar_item_style,
        static_cast<orion::settings::ToolbarItemStyle>(selected),
    [](orion::SetSettingReply<orion::settings::ToolbarItemStyle> reply){
        VERIFY_LOG(reply.err);
    });
}

static void
customize_toolbar_dialog_map(GtkWidget* widget, gpointer user_data)
{
    orion::log_verbose("customize_toolbar_dialog_map");

    GtkRoot* root = gtk_widget_get_root(widget);

    if (!MAIN_IS_WINDOW(root)) {
        orion::log_warning("customize_toolbar_dialog_map: cannot get main window");
        return;
    }

    MainWindow* main_window = MAIN_WINDOW(root);
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);
    CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(widget);
    orion::gtk::WeakRefWrapper dialog_weak(dialog);

    // Ensure that the first (and only) row is selected by default. This is just a cosmetic thing.
    GtkListBoxRow* first_row = gtk_list_box_get_row_at_index(dialog->default_toolbar_listbox, 0);
    if (first_row) {
        gtk_list_box_select_row(GTK_LIST_BOX(dialog->default_toolbar_listbox), first_row);
    }

    orion::GetDefaultToolbarItemsQuery query{
        .is_compact_tab_layout = dialog->is_compact_tab_layout,
        .in_toolbar = true,
        .in_overflow_menu = true,
    };

    global_core_api->get_default_toolbar_items(query,
    [profile_id,dialog_weak](orion::GetDefaultToolbarItemsReply reply){
        VERIFY_LOG(reply.err);

        // Fill the default toolbar items box
        for (const orion::ToolbarItemDto& dto : reply.items) {
            if (dto.in_overflow_menu) continue;
            
            auto dialog_strong = dialog_weak.lock();
            if (!dialog_strong) return;
            CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(dialog_strong.get());

            // Default toolbar items are not draggable (and cannot be removed) one by one, so we do not need
            // to set drag source prepare callback nor do we need to set the source of the toolbar item
            GtkWidget* item_widget = create_toolbar_item_widget(dialog, dto, ToolbarItemSource::not_set);
            if (!item_widget) {
                orion::log_warning(
                    "customize_toolbar_dialog_map: failed to create widget for default toolbar item id=%lld item_type=%s",
                    dto.id.value,
                    orion::toolbar_item_type_to_string(dto.item_type).c_str()
                );
                continue;
            }

            gtk_box_append(dialog->default_toolbar_box, item_widget);
        }

        auto dialog_strong = dialog_weak.lock();
        if (!dialog_strong) return;
        CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(dialog_strong.get());

        orion::GetAvailableToolbarItemsQuery query{
            .profile_id = profile_id,
            .is_compact_tab_layout = dialog->is_compact_tab_layout,
        };

        // Fill available toolbar items box
        global_core_api->get_available_toolbar_items(query,
        [profile_id,dialog_weak](orion::GetAvailableToolbarItemsReply reply){
            VERIFY_LOG(reply.err);

            const std::vector<orion::ToolbarItemDto>& available_items = reply.items;

            auto dialog_strong = dialog_weak.lock();
            if (!dialog_strong) return;
            CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(dialog_strong.get());

            orion::GetToolbarItemsQuery query{
                .profile_id = profile_id,
                .exclude_in_overflow_menu = true,
                .is_compact_tab_layout = dialog->is_compact_tab_layout,
            };

            global_core_api->get_toolbar_items(query,
            [profile_id,dialog_weak,available_items](orion::GetToolbarItemsReply reply){
                VERIFY_LOG(reply.err);

                auto dialog_strong = dialog_weak.lock();
                if (!dialog_strong) return;
                CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(dialog_strong.get());

                for (const orion::ToolbarItemDto& dto : available_items) {
                    if (dto.in_overflow_menu) continue;
                    GtkWidget* widget = create_toolbar_item_widget(dialog, dto, ToolbarItemSource::available_toolbar_box);
                    if (!widget) {
                        orion::log_warning(
                            "customize_toolbar_dialog_map: failed to create widget for available toolbar item id=%lld item_type=%s",
                            dto.id.value,
                            orion::toolbar_item_type_to_string(dto.item_type).c_str()
                        );
                        continue;
                    }

                    GtkDragSource* drag_source = gtk_drag_source_new();
                    gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);
                    g_signal_connect(drag_source, "prepare", G_CALLBACK(on_available_box_item_drag_source_prepare), NULL);
                    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(drag_source));

                    sync_available_toolbar_item_disabled_state(widget, dto, reply.items);

                    adw_wrap_box_append(dialog->available_toolbar_items_box, widget);
                }

                // Fill current toolbar items box
                orion::GetToolbarItemsQuery query{
                    .profile_id = profile_id,
                    .is_compact_tab_layout = dialog->is_compact_tab_layout,
                };

                global_core_api->get_toolbar_items(query, [profile_id,dialog_weak](orion::GetToolbarItemsReply reply){
                    VERIFY_LOG(reply.err);

                    orion::log_verbose(
                        "customize_toolbar_dialog_map: got %zu toolbar items for profile_id=%lld",
                        reply.items.size(), profile_id.value
                    );

                    auto dialog_strong = dialog_weak.lock();
                    if (!dialog_strong) return;
                    CustomizeToolbarDialog* dialog = CUSTOMIZE_TOOLBAR_DIALOG(dialog_strong.get());

                    for (const orion::ToolbarItemDto& dto : reply.items) {
                        if (dto.in_overflow_menu) continue;
                        GtkWidget* widget = create_current_toolbar_item_widget(dialog, dto);
                        if (widget) {
                            gtk_box_append(dialog->toolbar_items_box, widget);
                        }
                    }
                });
            });
        });
    });
}

static void
customize_toolbar_dialog_unmap(GtkWidget* widget, gpointer user_data)
{
    orion::log_verbose("customize_toolbar_dialog_unmap");
}

static void
customize_toolbar_dialog_finalize(GObject* object)
{
    CustomizeToolbarDialog* self = CUSTOMIZE_TOOLBAR_DIALOG(object);
    overflow_menu_listview_finalize(self->overflow_menu_listview);

    global_core_api->toolbar_items_change().clear_observer(&self->overflow_menu_items_observer_id);

    std::destroy_at(&self->overflow_menu_items_observer_id);

    G_OBJECT_CLASS(customize_toolbar_dialog_parent_class)->finalize(object);
}

static void
customize_toolbar_dialog_root(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(customize_toolbar_dialog_parent_class)->root(widget); // important: always call up

    CustomizeToolbarDialog* self = CUSTOMIZE_TOOLBAR_DIALOG(widget);
    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(self));
    if (!GTK_IS_ROOT(root) || !MAIN_IS_WINDOW(root)) {
        orion::log_warning("register_overflow_menu_items_observer: failed to get main window for dialog");
        return;
    }

    orion::ProfileId profile_id = main_window_get_current_profile_id(MAIN_WINDOW(root));
    
    register_toolbar_items_observer(self, profile_id);
}

static void
customize_toolbar_dialog_init(CustomizeToolbarDialog* self)
{
    std::construct_at(&self->overflow_menu_items_observer_id);

    g_type_ensure(OVERFLOW_MENU_ITEM_TYPE);
    g_type_ensure(TOOLBAR_ITEM_TYPE);

	gtk_widget_init_template(GTK_WIDGET(self));
    
    g_signal_connect(self, "destroy", G_CALLBACK(on_destroy), NULL);

    overflow_menu_listview_init(self->overflow_menu_listview);
}

static void
customize_toolbar_dialog_class_init(CustomizeToolbarDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/customize_toolbar_dialog.ui");

    G_OBJECT_CLASS(klass)->finalize = customize_toolbar_dialog_finalize;

    GTK_WIDGET_CLASS(klass)->root = customize_toolbar_dialog_root;

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_done_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_overflow_menu_listview_activate);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_overflow_menu_listview_map);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_overflow_menu_toolbar_item_drag_source_prepare);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_overflow_menu_toolbar_item_drop_target_drop);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_overflow_menu_toolbar_item_drop_target_motion);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_overflow_menu_toolbar_item_drop_target_leave);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_overflow_menu_toolbar_item_drop_target_accept);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_available_box_item_drag_source_prepare);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_available_box_drop_target_drop);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_available_box_drop_target_accept);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_default_toolbar_drag_source_prepare);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_current_toolbar_drop_target_drop);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_current_toolbar_drop_target_accept);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_current_toolbar_toolbar_item_drop_target_accept);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), customize_toolbar_dialog_map);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), customize_toolbar_dialog_unmap);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_toolbar_item_style_dropdown_map);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_toolbar_item_style_dropdown_notify_selected);

    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomizeToolbarDialog, done_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomizeToolbarDialog, overflow_menu_listview);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomizeToolbarDialog, toolbar_items_box);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomizeToolbarDialog, default_toolbar_listbox);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomizeToolbarDialog, default_toolbar_box);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomizeToolbarDialog, available_toolbar_items_box);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomizeToolbarDialog, toolbar_item_style_dropdown);
}
