#include "find_bar.h"
#include "util/action_util.h"
#include "util/text_util.h"
#include "window/main_window/main_window.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"

struct _FindBar
{
    AdwBin parent_type;

    GtkSearchEntry* find_bar_entry;
    GtkLabel* find_bar_count_label;
    GtkButton* find_bar_previous_button;
    GtkButton* find_bar_next_button;
    GtkButton* find_bar_done_button;

    guint find_total_matches;
};

G_DEFINE_TYPE(FindBar, find_bar, ADW_TYPE_BIN)

static void
find_bar_update_count_label(FindBar* self)
{
    if (self->find_total_matches == 0)
    {
        gtk_label_set_text(self->find_bar_count_label, _("No Results"));
    }
    else
    {
        std::string text = orion::text::ngettext_string("%u Result", "%u Results", self->find_total_matches);
        gtk_label_set_text(self->find_bar_count_label, text.c_str());
    }
}

static void
find_bar_search(FindBar* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    OrionWebviewTabContent* content = main_window_get_current_tab_content(win);
    if (!content) return;

    const char* text = gtk_editable_get_text(GTK_EDITABLE(self->find_bar_entry));
    if (text && text[0] != '\0')
    {
        gboolean ignore_case = orion::gtk::get_boolean_action_state(G_ACTION_MAP(win), "find-ignore-case");
        gboolean wrap_around = orion::gtk::get_boolean_action_state(G_ACTION_MAP(win), "find-wrap-around");
        std::string match_mode = orion::gtk::get_string_action_state(G_ACTION_MAP(win), "find-match-mode");
        gboolean starts_with = (match_mode == "starts-with");

        orion_webview_tab_content_set_find_result_callback(content, [self](guint match_count, gboolean found){
            self->find_total_matches = found ? match_count : 0;
            find_bar_update_count_label(self);
        });
        orion_webview_tab_content_find_search(content, text, ignore_case, wrap_around, starts_with);
    }
    else
    {
        orion_webview_tab_content_set_find_result_callback(content, nullptr);
        orion_webview_tab_content_find_finish(content);
        self->find_total_matches = 0;
        gtk_label_set_text(self->find_bar_count_label, "");
    }
}

static void
find_bar_next(FindBar* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    OrionWebviewTabContent* content = main_window_get_current_tab_content(win);
    if (!content) return;
    orion_webview_tab_content_find_next(content);
}

static void
find_bar_previous(FindBar* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    OrionWebviewTabContent* content = main_window_get_current_tab_content(win);
    if (!content) return;
    orion_webview_tab_content_find_previous(content);
}

static void
on_find_bar_entry_search_changed(GtkSearchEntry* entry, gpointer user_data)
{
    find_bar_search(APP_FIND_BAR(user_data));
}

static void
on_find_bar_entry_activate(GtkSearchEntry* entry, gpointer user_data)
{
    find_bar_next(APP_FIND_BAR(user_data));
}

static gboolean
on_find_bar_entry_key_pressed(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    // GtkSearchEntry's "activate" signal fires on plain Enter (handled above as "next"),
    // but it can't tell us about the Shift modifier, so Shift+Enter ("previous") is caught here.
    if (keyval == GDK_KEY_Return && (state & GDK_SHIFT_MASK))
    {
        find_bar_previous(APP_FIND_BAR(user_data));
        return TRUE;
    }
    return FALSE;
}

static void
on_find_bar_previous_button_clicked(GtkButton* button, gpointer user_data)
{
    find_bar_previous(APP_FIND_BAR(user_data));
}

static void
on_find_bar_next_button_clicked(GtkButton* button, gpointer user_data)
{
    find_bar_next(APP_FIND_BAR(user_data));
}

static void
on_find_bar_done_button_clicked(GtkButton* button, gpointer user_data)
{
    FindBar* self = APP_FIND_BAR(user_data);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    main_window_hide_find_bar(win);
}

static void
on_find_bar_entry_stop_search(GtkSearchEntry* entry, gpointer user_data)
{
    FindBar* self = APP_FIND_BAR(user_data);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    main_window_hide_find_bar(win);
}

void
find_bar_refresh_search(FindBar* self)
{
    find_bar_search(self);
}

void
find_bar_set_text_and_search(FindBar* self, const std::string& text)
{
    gtk_editable_set_text(GTK_EDITABLE(self->find_bar_entry), text.c_str());
    find_bar_search(self);
}

void
find_bar_focus_entry(FindBar* self)
{
    gtk_widget_grab_focus(GTK_WIDGET(self->find_bar_entry));
    gtk_editable_select_region(GTK_EDITABLE(self->find_bar_entry), 0, -1);
}

static void
find_bar_init(FindBar* self)
{
    gtk_widget_init_template(GTK_WIDGET(self));
}

static void
find_bar_class_init(FindBarClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/main_window/header/find_bar.ui");

    gtk_widget_class_bind_template_callback(widget_class, on_find_bar_entry_search_changed);
    gtk_widget_class_bind_template_callback(widget_class, on_find_bar_entry_activate);
    gtk_widget_class_bind_template_callback(widget_class, on_find_bar_entry_key_pressed);
    gtk_widget_class_bind_template_callback(widget_class, on_find_bar_previous_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_find_bar_next_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_find_bar_done_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_find_bar_entry_stop_search);

    gtk_widget_class_bind_template_child(widget_class, FindBar, find_bar_entry);
    gtk_widget_class_bind_template_child(widget_class, FindBar, find_bar_count_label);
    gtk_widget_class_bind_template_child(widget_class, FindBar, find_bar_previous_button);
    gtk_widget_class_bind_template_child(widget_class, FindBar, find_bar_next_button);
    gtk_widget_class_bind_template_child(widget_class, FindBar, find_bar_done_button);
}
