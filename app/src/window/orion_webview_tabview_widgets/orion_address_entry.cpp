#include <gtk/gtk.h>
#include "app.h"

#include <common/src/log.h>

#include "orion_address_entry.h"

#include "util/dialog_util.h"
#include "util/search_engines.h"
#include "util/url_or_search_detector.h"

#include "window/main_window/main_window.h"
#include "window/main_window/orion_window_iface.h"

#include "window/search_completion_popover.h"
#include "window/search_popover.h"
#include "window/set_default_search_engine_dialog.h"

struct _OrionAddressEntryClass
{
    GtkEntryClass parent_class;

    //TODO: rename this
};

struct _OrionAddressEntry
{
	GtkEntry parent;

    GtkPopover* search_completion_popover;
    GtkPopover* search_popover;

    bool focus_within;
};

G_DEFINE_TYPE(OrionAddressEntry, orion_address_entry, GTK_TYPE_ENTRY)

[[maybe_unused]] static void
orion_address_entry_popover_popup(OrionAddressEntry* self, GtkPopover* popover)
{
    GtkWidget* widget = GTK_WIDGET(self);
    GtkWidget* native = GTK_WIDGET(gtk_widget_get_native(widget));
    if(native)
    {
        gtk_popover_popup(popover);
    }
}

static void
orion_address_entry_popdown(OrionAddressEntry* self)
{
    gtk_popover_popdown(self->search_completion_popover);
    gtk_popover_popdown(self->search_popover);
}

static void
select_all_when_focused(gpointer data)
{
    OrionAddressEntry* self = ORION_ADDRESS_ENTRY(data);
    if(self->focus_within)
    {
        gtk_editable_select_region(GTK_EDITABLE(self), 0, -1);
    }
    g_object_unref(self);
}

static void
on_focus_enter(GtkEventControllerFocus* controller, OrionAddressEntry* self)
{
    //gtk_editable_set_alignment(GTK_EDITABLE(self), 0.0f);
    self->focus_within = true;

    // GtkEntry processes the click that gave it focus after this callback and
    // would otherwise replace the selection with a cursor at the click point.
    g_idle_add_once(select_all_when_focused, g_object_ref(self));

    // TODO:
    //orion_address_entry_popover_popup(self, self->search_popover);
}

static void
on_focus_leave(GtkEventControllerFocus* controller, OrionAddressEntry* self)
{
    //gtk_editable_set_alignment(GTK_EDITABLE(self), 0.5f);
    self->focus_within = false;

    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    std::string uri = orion_window_get_uri(win);

    gtk_editable_set_text(GTK_EDITABLE(self), uri.c_str());
    
    orion_address_entry_popdown(self);
}

static void
on_changed(OrionAddressEntry* self, gpointer user_data)
{
    const gchar* text = gtk_editable_get_text(GTK_EDITABLE(self));
    orion::log_verbose("orion_address_entry: on_changed: text=%s", text);

    if(gtk_entry_get_text_length(GTK_ENTRY(self)) && self->focus_within)
    {
        // TODO:
        //orion_address_entry_popover_popup(self, self->search_completion_popover);
        gtk_popover_popdown(self->search_popover);
    }
    else
    {
        gtk_popover_popdown(self->search_completion_popover);
    }
}

static void
on_activate(OrionAddressEntry* self, gpointer user_data)
{
    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    orion_address_entry_popdown(self);

    
    std::string user_input = gtk_editable_get_text(GTK_EDITABLE(self));

    orion::log_debug("orion_webview_tab_content_load_uri: is_probably_url=%d user_input=%s",
        orion::is_probably_url(user_input), user_input.c_str());

        
    orion::Uri uri{};
    if (orion::is_probably_url(user_input)) {
        uri = orion::Uri::parse_user_input(user_input.c_str());
        g_object_set(win, "uri", uri.uri().c_str(), NULL);
        return;
    }

    // This is probably not a URL, treat it as a search query
    orion::ProfileId profile_id = orion_window_get_profile_id(win);
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_setting(profile_id, orion::settings::search::search_engine,
        [user_input,win_weak,profile_id](orion::GetSettingReply<orion::settings::SearchEngine> reply){
            VERIFY_LOG(reply.err);

            orion::settings::SearchEngine default_engine = reply.value;

            orion::MatchSearchEnginesQuery query{
                .query_string = user_input,
                .profile_id = profile_id,
            };

            global_core_api->match_search_engines(query,
                [default_engine,win_weak,user_input](orion::MatchSearchEnginesReply reply){
                    VERIFY_LOG(reply.err);

                    auto win_strong = win_weak.lock();
                    if (!win_strong) return;
                    OrionWindow* win = ORION_WINDOW(win_strong.get());

                    if (reply.matches.empty()) {
                        // Try to search using default search engine (if it is set)
                        if (default_engine == orion::settings::SearchEngine::none) {
                            //TODO: callback to then actually start the search
                            AdwDialog* dialog = set_default_search_engine_dialog_new(win);
                            adw_dialog_present(dialog, GTK_WIDGET(win));
                            return;
                        }

                        orion::SearchEngineInfo info{};
                        VERIFY_LOG(orion::get_search_engine_info(default_engine, &info));
                        
                        orion::Uri uri = orion::get_search_uri(info.search_uri_template, user_input);
                        
                        g_object_set(win, "uri", uri.uri().c_str(), NULL);
                    } else {
                        // Use the first matched search engine
                        const orion::SearchEngineMatch& match = reply.matches[0];
                        
                        orion::Uri uri = orion::get_search_uri(match.search_url_template, match.search_query);
                        g_object_set(win, "uri", uri.uri().c_str(), NULL);
                    }
                });
        });
    
    
    gtk_window_set_focus(GTK_WINDOW(win), NULL);
}



OrionAddressEntry*
orion_address_entry_new()
{
    OrionAddressEntry* entry = ORION_ADDRESS_ENTRY(g_object_new(ORION_ADDRESS_ENTRY_TYPE, NULL));
	return entry;
}

static void
widget_unparent_wrapper(gpointer ptr)
{
    gtk_widget_unparent(GTK_WIDGET(ptr));
}

static void
orion_address_entry_dispose(GObject* object)
{
    OrionAddressEntry* self = ORION_ADDRESS_ENTRY(object);

    g_clear_pointer(&self->search_completion_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->search_popover, orion::gtk::widget_unparent_wrapper);

    G_OBJECT_CLASS(orion_address_entry_parent_class)->dispose(object);
}

static void
on_main_window_primary_pressed(MainWindow* win, gdouble x, gdouble y, OrionAddressEntry* entry)
{
    graphene_point_t win_point = {.x = (float)x, .y = (float)y}, entry_point{};
    if(gtk_widget_compute_point(GTK_WIDGET(win), GTK_WIDGET(entry), &win_point, &entry_point))
    {
        if(!gtk_widget_contains(GTK_WIDGET(entry), entry_point.x, entry_point.y))
        {
            orion_address_entry_popdown(entry);
        }
        else
        {
            // TODO:
            //orion_address_entry_popover_popup(entry, entry->search_popover);
        }
    }
}

static void
on_main_window_notify_active(MainWindow* win, GParamSpec* pspec, OrionAddressEntry* entry)
{
    if(!gtk_window_is_active(GTK_WINDOW(win)))
    {
        orion_address_entry_popdown(entry);
    }
}

static void
orion_address_entry_root(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(orion_address_entry_parent_class)->root(widget);

    OrionAddressEntry* self = ORION_ADDRESS_ENTRY(widget);

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(widget));

    g_signal_connect_object(win, "primary-pressed", G_CALLBACK(on_main_window_primary_pressed), self, G_CONNECT_DEFAULT);
    g_signal_connect_object(win, "notify::is-active", G_CALLBACK(on_main_window_notify_active), self, G_CONNECT_DEFAULT);
}

static void
on_root_uri_notify(gpointer data)
{
    OrionAddressEntry* self = ORION_ADDRESS_ENTRY(data);
    
    if(!self->focus_within)
    {
        OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
        if(!win) return;

        std::string uri = orion_window_get_uri(win);

        gtk_editable_set_text(GTK_EDITABLE(self), uri.c_str());
    }
}

static void
orion_address_entry_init(OrionAddressEntry* self)
{
    gtk_widget_set_name(GTK_WIDGET(self),"address-entry");

    self->search_completion_popover = search_completion_popover_new();
    self->search_popover = search_popover_new();

    gtk_widget_set_parent(GTK_WIDGET(self->search_completion_popover), GTK_WIDGET(self));
    gtk_widget_set_parent(GTK_WIDGET(self->search_popover), GTK_WIDGET(self));
    gtk_widget_set_focusable(GTK_WIDGET(self->search_completion_popover), false);
    gtk_widget_set_focusable(GTK_WIDGET(self->search_popover), false);
    gtk_popover_set_autohide(self->search_completion_popover, false);
    gtk_popover_set_autohide(self->search_popover, false);

    GtkEventController* focus_controller = gtk_event_controller_focus_new();
    g_signal_connect(focus_controller, "enter", G_CALLBACK(on_focus_enter), self);
    g_signal_connect(focus_controller, "leave", G_CALLBACK(on_focus_leave), self);
    gtk_widget_add_controller(GTK_WIDGET(self), focus_controller);

    g_signal_connect(self, "changed", G_CALLBACK(on_changed), NULL);
    g_signal_connect(self, "activate", G_CALLBACK(on_activate), NULL);

    GtkExpression* root_expression = gtk_property_expression_new(GTK_TYPE_WIDGET, NULL, "root");
    GtkExpression* uri_expression = gtk_property_expression_new(ORION_TYPE_WINDOW, root_expression, "uri");
    gtk_expression_watch(uri_expression, self, on_root_uri_notify, self, NULL);
    gtk_expression_unref(uri_expression);
}

static void
orion_address_entry_class_init(OrionAddressEntryClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = orion_address_entry_dispose;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->root = orion_address_entry_root;
}
