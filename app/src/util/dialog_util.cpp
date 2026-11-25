#include "dialog_util.h"
#include <algorithm>
#include <common/src/log.h>
#include <string>
#include <common/src/uuid.h>
#include <common/src/defer.h>

namespace orion::gtk {

GtkWindow*
get_parent_window(GtkWidget* widget)
{
    if (!widget) return NULL;
    GtkWidget* current = GTK_WIDGET(widget);
    do {
        current = gtk_widget_get_parent(current);
        if (GTK_IS_WINDOW(current)) {
            return GTK_WINDOW(current);
        }
    } while (current != NULL);
    return NULL;
}

AdwDialog*
get_parent_adw_dialog(GtkWidget* widget)
{
    if (!widget) return NULL;
    GtkWidget* current = GTK_WIDGET(widget);
    do {
        current = gtk_widget_get_parent(current);
        if (ADW_IS_DIALOG(current)) {
            return ADW_DIALOG(current);
        }
    } while (current != NULL);
    return NULL;
}

GtkWidget*
get_child_by_name(GtkWidget* parent, const char* name)
{
    GtkWidget* child = gtk_widget_get_first_child(parent);
    while (child) {
        const char* child_name = gtk_widget_get_name(child);
        if (g_strcmp0(child_name, name) == 0) {
            return child;
        }
        child = gtk_widget_get_next_sibling(child);
    }
    return NULL;
}

std::vector<GtkWidget*>
get_descendants_by_name(GtkWidget* root, const char* type_name)
{
    std::vector<GtkWidget*> result;

    GtkWidget* child = gtk_widget_get_first_child(root);
    while (child != NULL) {
        if (strcmp(gtk_widget_get_name(child), type_name) == 0) {
            result.push_back(child);
        }

        auto sub_result = get_descendants_by_name(child, type_name);
        result.insert(result.end(), sub_result.begin(), sub_result.end());

        child = gtk_widget_get_next_sibling(child);
    }

    return result;
}

void
get_descendants(GtkWidget* root, std::vector<GtkWidget*>* out_result)
{
    GtkWidget* child = gtk_widget_get_first_child(root);
    while (child != NULL) {
        out_result->push_back(child);

        get_descendants(child, out_result);

        child = gtk_widget_get_next_sibling(child);
    }
}

gboolean
is_widget_at_position(GtkWidget* widget, double x, double y, GtkWidget* origin)
{
    GtkWidget *picked = gtk_widget_pick(origin, x, y, GTK_PICK_DEFAULT);
    while (picked) {
        if (picked == widget) {
            return TRUE;
        }
        picked = gtk_widget_get_parent(picked);
    }
    return FALSE;
}

int
get_children_count(GtkWidget* parent)
{
    int count = 0;

    for (GtkWidget *child = gtk_widget_get_first_child(parent);
         child != NULL;
         child = gtk_widget_get_next_sibling(child))
    {
        count++;
    }

    return count;
}

// Note that unfortunately AdwTab widgets do not have to be ordered in the widget tree in
// the same order as adw_tab_view_get_nth_page() order - when the user reorders tabs
// by dragging them, AdwTab widgets in the tree may stay at the same position.
int
get_adw_tab_index(GtkWidget* adw_tab, GtkWidget* root)
{
    std::vector<GtkWidget*> adw_tabs = get_descendants_by_name(root, "AdwTab");

    // order vector by x position
    std::sort(adw_tabs.begin(), adw_tabs.end(), [root](GtkWidget* a, GtkWidget* b) {
        graphene_rect_t a_bounds{}, b_bounds{};

        if (!gtk_widget_compute_bounds(a, root, &a_bounds)) {
            log_error("get_adw_tab_index: failed to compute bounds for tab");
            return false;
        }

        if (!gtk_widget_compute_bounds(b, root, &b_bounds)) {
            log_error("get_adw_tab_index: failed to compute bounds for tab");
            return false;
        }

        if (a_bounds.origin.x == b_bounds.origin.x) {
            //log_warning("get_adw_tab_index: two tabs have the same x position");
            return false;
        }
        return a_bounds.origin.x < b_bounds.origin.x;
    });

    for (size_t i = 0; i < adw_tabs.size(); i++) {
        if (adw_tabs[i] == adw_tab) {
            return static_cast<int>(i);
        }
    }
    return -1; // not found
}

// Uses a trick to find the AdwTabPage corresponding to the given AdwTab widget - we
// temporarily set a unique tooltip on each AdwTabPage (one by one) and then we check
// which AdwTab has that tooltip. Then we restore the original tooltip.
AdwTabPage*
get_adw_tab_page_by_adw_tab(GtkWidget* adw_tab, AdwTabBar* tab_bar)
{
    AdwTabView* tab_view = adw_tab_bar_get_view(tab_bar);
    int n_pages = adw_tab_view_get_n_pages(tab_view);
    std::vector<GtkWidget*> adw_tabs = get_descendants_by_name(GTK_WIDGET(tab_bar), "AdwTab");

    for (int i = 0; i < n_pages; i++) {
        AdwTabPage* page = adw_tab_view_get_nth_page(tab_view, i);
        std::string original_tooltip = std::string(adw_tab_page_get_tooltip(page));
        std::string uuid = orion::generate_uuid_v4(); // arbitrary unique identifier
        adw_tab_page_set_tooltip(page, uuid.c_str());
        
        for (auto tab : adw_tabs) {
            log_nop("AdwTab: addr=%p tooltip=%s", tab, gtk_widget_get_tooltip_text(tab));
        }

        if (strcmp(gtk_widget_get_tooltip_text(adw_tab), uuid.c_str()) == 0) {
            adw_tab_page_set_tooltip(page, original_tooltip.c_str()); // restore original tooltip
            return page;
        }

        adw_tab_page_set_tooltip(page, original_tooltip.c_str()); // restore original tooltip
    }

    return NULL; // page not found
}

bool
is_widget_safe_for_popover_parent(GtkWidget* widget)
{
    if (!GTK_IS_WIDGET(widget))
        return false;

    GtkRoot* root = gtk_widget_get_root(widget);
    if (!GTK_IS_WINDOW(root))  // Must be inside a real toplevel window
        return false;

    GtkWidget* parent = gtk_widget_get_parent(widget);
    if (parent && GTK_IS_POPOVER(parent))
        return false;  // Avoid attaching popover to a widget inside another popover

    // Additional optional checks:
    // - Ensure the widget is realized and mapped (visible)
    if (!gtk_widget_get_realized(widget) || !gtk_widget_get_mapped(widget))
        return false;

    // - Ensure the toplevel window is visible
    GtkWindow* window = GTK_WINDOW(root);
    if (!gtk_window_is_active(window) || !gtk_widget_get_visible(GTK_WIDGET(window)))
        return false;

    return true;
}

void
widget_unparent_wrapper(gpointer ptr)
{
    gtk_widget_unparent(GTK_WIDGET(ptr));
}

void
remove_all_children_from_box(GtkBox* box)
{
    GtkWidget* child = NULL;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(box))) != NULL) {
        gtk_box_remove(box, child);
    }
}

enum class MoveChildPosition {
    append_to_end,
    prepend_to_start,
    after_sibling,
    before_sibling,
};

struct MoveChildQuery {
    GtkBox* source{};
    GtkWidget* child{};
    GtkBox* target{};
    MoveChildPosition position{};
    GtkWidget* sibling{}; // used only for after_sibling and before_sibling positions
};

static void
move_child_from_box_to_box_impl(const MoveChildQuery& query)
{
    if (!query.child) {
        orion::log_warning("%s: child is NULL", __func__);
        return;
    }

    // We have to take a reference to the child before removing it from the box, otherwise
    // it can be destroyed prematurely if it is not referenced anywhere else.
    g_object_ref(query.child);
    defer([query]{ g_object_unref(query.child); });

    gtk_box_remove(query.source, query.child);
    if (query.position == MoveChildPosition::append_to_end) {
        gtk_box_append(query.target, query.child);
    } else if (query.position == MoveChildPosition::prepend_to_start) {
        gtk_box_prepend(query.target, query.child);
    } else if (query.position == MoveChildPosition::after_sibling) {
        gtk_box_insert_child_after(query.target, query.child, query.sibling);
    } else if (query.position == MoveChildPosition::before_sibling) {
        // Should be equivalent to non-existent gtk_box_insert_child_before function
        gtk_widget_insert_before(query.child, GTK_WIDGET(query.target), query.sibling);
    }
}

void
move_child_from_box_to_box(GtkBox* source, GtkWidget* child, GtkBox* target)
{
    move_child_from_box_to_box_impl(MoveChildQuery{
        .source = source, 
        .child = child,
        .target = target,
        .position = MoveChildPosition::append_to_end,
    });
}

void
move_child_from_box_to_box_start(GtkBox* source, GtkWidget* child, GtkBox* target)
{
    move_child_from_box_to_box_impl(MoveChildQuery{
        .source = source,
        .child = child,
        .target = target,
        .position = MoveChildPosition::prepend_to_start,
    });
}

void
move_child_from_box_to_box_after_sibling(GtkBox* source, GtkWidget* child, GtkBox* target, GtkWidget* sibling)
{
    move_child_from_box_to_box_impl(MoveChildQuery{
        .source = source,
        .child = child,
        .target = target,
        .position = MoveChildPosition::after_sibling,
        .sibling = sibling,
    });
}

void
move_child_from_box_to_box_before_sibling(GtkBox* source, GtkWidget* child, GtkBox* target, GtkWidget* sibling)
{
    move_child_from_box_to_box_impl(MoveChildQuery{
        .source = source,
        .child = child,
        .target = target,
        .position = MoveChildPosition::before_sibling,
        .sibling = sibling,
    });
}

void
move_all_children_from_box_to_box(GtkBox* source, GtkBox* target)
{
    GtkWidget* child = NULL;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(source))) != NULL) {
        move_child_from_box_to_box(source, child, target);
    }
}

} // namespace orion::gtk