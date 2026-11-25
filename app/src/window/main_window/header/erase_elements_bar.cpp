#include "app.h"

#include <common/src/defer.h>
#include <common/src/string_util.h>

#include <core/settings.h>

#include "erase_elements_bar.h"

#include "util/debounce_util.h"
#include "util/css_util.h"

#include "window/main_window/main_window.h"

namespace {
    struct CppWrapper {
        std::string current_host;
        std::vector<std::string> history;
    };
}

struct _EraseElementsBar {
    GtkBox parent;

    GtkMenuButton* text_view_menu_button;
    GtkTextView* element_blockers_text_view;
    GtkTextBuffer* element_blockers_text_buffer;
    GtkActionBar* saved_bar;
    GtkCheckButton* preview_erased_check_button;
    GtkActionBar* unsaved_bar;
    GtkButton* undo_button;

    OrionWebviewTabContent* element_picker_content; //will be set if we are currently running

    gboolean text_is_saved;
    gboolean popover_dismissed_by_esc;

    CppWrapper wrapper;
};

G_DEFINE_TYPE(EraseElementsBar, erase_elements_bar, GTK_TYPE_BOX);


static inline void
update_undo_sensitivity(EraseElementsBar* self)
{
    gtk_widget_set_sensitive(GTK_WIDGET(self->undo_button), !self->text_is_saved || (self->wrapper.history.size() > 1));
}

static void
set_text_is_saved(EraseElementsBar* self, gboolean value)
{
    gtk_widget_set_visible(GTK_WIDGET(self->saved_bar), value);
    gtk_widget_set_visible(GTK_WIDGET(self->unsaved_bar), !value);

    self->text_is_saved = value;

    update_undo_sensitivity(self);
}

static void
element_blockers_changed(GtkTextBuffer* buffer, EraseElementsBar* self)
{
    set_text_is_saved(self, false);
    //self->wrapper.input_debouncer.reset_timer();
}

//NOTE: the location iter here should be const but...
//"Note that if your handler runs before the default handler it must not invalidate the location iter (or has to revalidate it).
//The default signal handler revalidates it to point to the end of the inserted text."
//this is taken from the gtk docs, and it means that the iterator realistically is not treated as const
static void
element_blockers_insert_text(GtkTextBuffer* buffer, GtkTextIter* location, gchar* new_text, gint len, EraseElementsBar* self)
{
    if (!new_text) return;


    bool contains_forbidden = FALSE;
    for (gint i = 0; i < len; i++) {
        if (new_text[i] == '{' || new_text[i] == '}') {
            contains_forbidden = TRUE;
            break;
        }
    }

    if (contains_forbidden) {
        g_signal_stop_emission_by_name(buffer, "insert-text");

        std::string str = new_text;

        auto new_end = std::remove_if(str.begin(), str.end(), [](char c){
            return c == '{' || c == '}';
        });
        str.erase(new_end, str.end());

        g_signal_handlers_block_by_func(buffer, (gpointer)element_blockers_insert_text, self);
        gtk_text_buffer_insert(buffer, location, str.c_str(), str.length());
        g_signal_handlers_unblock_by_func(buffer, (gpointer)element_blockers_insert_text, self);
    }
}

static void
save_element_blockers(EraseElementsBar* self)
{
    if(!self->element_blockers_text_buffer) return;
    if(!self->wrapper.current_host.length())
    {
        orion::log_error("EraseElementsBar save_element_blockers(): attempted to save but no host is set");
        return;
    }
    
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(self->element_blockers_text_buffer, &start, &end);
    char* text = gtk_text_buffer_get_text(self->element_blockers_text_buffer, &start, &end, FALSE);
    defer([text]{ g_free(text); });
    std::string_view text_str_view = text;

    auto setting = WEBSITE_SETTING(page_tweaks_element_blocker_list, self->wrapper.current_host);
    orion::SetSettingQuery query{
        .profile_id = main_window_get_current_profile_id(win),
        .key = setting.key(),
        .realm = setting.realm(),
        .key_name = setting.key_name(),
        .type = setting.type(),
        .value = std::string(text_str_view),
        .default_value = setting.default_value()
    };
    global_core_api->set_setting_raw(query, [](orion::SetSettingReply<std::string> reply){
        VERIFY_LOG(reply.err);
    });

    set_text_is_saved(self, true);
    self->wrapper.history.push_back(text);
    update_undo_sensitivity(self);

    unsigned long count = orion::count_css_selectors(text);
    char* n_erase_elements_label = g_strdup_printf(
        ngettext("%lu element erased", "%lu  elements erased", count),
        count
    );
    defer([n_erase_elements_label]{ g_free(n_erase_elements_label); });
    gtk_menu_button_set_label(self->text_view_menu_button, n_erase_elements_label);

    if(gtk_check_button_get_active(self->preview_erased_check_button))
    {
        orion_webview_tab_content_update_preview_erased_selectors(self->element_picker_content, text_str_view);
    }
}

static void
undo(EraseElementsBar* self)
{
    if(!self->wrapper.current_host.length())
    {
        orion::log_error("EraseElementsBar undo(): attempted to undo but no host is set");
        return;
    }
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    if(self->text_is_saved)
    {
        if(self->wrapper.history.size() <= 1)
        {
            return;
        }
        self->wrapper.history.pop_back();
        update_undo_sensitivity(self);
    }
    else
    {
        set_text_is_saved(self, true);
    }
    
    if(self->wrapper.history.size())
    {
        std::string& old_value = self->wrapper.history.back();
        auto setting = WEBSITE_SETTING(page_tweaks_element_blocker_list, self->wrapper.current_host);
        orion::SetSettingQuery query{
            .profile_id = main_window_get_current_profile_id(win),
            .key = setting.key(),
            .realm = setting.realm(),
            .key_name = setting.key_name(),
            .type = setting.type(),
            .value = old_value,
            .default_value = setting.default_value()
        };
        global_core_api->set_setting_raw(query, [](orion::SetSettingReply<std::string> reply){
            VERIFY_LOG(reply.err);
        });

        g_signal_handlers_block_by_func(self->element_blockers_text_buffer, gpointer(element_blockers_changed), self);
        gtk_text_buffer_set_text(self->element_blockers_text_buffer, old_value.c_str(), old_value.length());
        g_signal_handlers_unblock_by_func(self->element_blockers_text_buffer, gpointer(element_blockers_changed), self);

        unsigned long count = orion::count_css_selectors(old_value);
        char* n_erase_elements_label = g_strdup_printf(
            ngettext("%lu element erased", "%lu  elements erased", count),
            count
        );
        defer([n_erase_elements_label]{ g_free(n_erase_elements_label); });
        gtk_menu_button_set_label(self->text_view_menu_button, n_erase_elements_label);

        if(gtk_check_button_get_active(self->preview_erased_check_button))
        {
            orion_webview_tab_content_update_preview_erased_selectors(self->element_picker_content, old_value);
        }
    }
}

void
erase_elements_bar_reset_selectors(EraseElementsBar* self)
{
    if(gtk_text_buffer_get_char_count(self->element_blockers_text_buffer))
    {
        char empty_text = '\0';
        gtk_text_buffer_set_text(self->element_blockers_text_buffer, &empty_text, 0);
        save_element_blockers(self);
    }
}

static void
cancel(EraseElementsBar* self)
{
    if(!self->text_is_saved)
    {
        if(self->wrapper.history.size())
        {
            gtk_text_buffer_set_text(self->element_blockers_text_buffer, self->wrapper.history.back().c_str(), self->wrapper.history.back().length());
        }
        else
        {
            char empty_text = '\0';
            gtk_text_buffer_set_text(self->element_blockers_text_buffer, &empty_text, 0);
        }
        set_text_is_saved(self, true);
    }
}

static void
apply(EraseElementsBar* self)
{
    save_element_blockers(self);
}

static void
done(EraseElementsBar* self)
{
    if(self->element_picker_content)
    {
        orion_webview_tab_content_end_element_picker(self->element_picker_content);
    }
}

static void
on_text_view_popover_closed(GtkPopover* popover, EraseElementsBar* self)
{
    if(self->popover_dismissed_by_esc)
    {
        cancel(self);
    }
    else
    {
        if(!self->text_is_saved)
        {
            save_element_blockers(self);
        }
    }

    self->popover_dismissed_by_esc = false;
}

static gboolean
on_text_view_popover_key_pressed(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, EraseElementsBar* self)
{
    if(keyval == GDK_KEY_Escape)
    {
        self->popover_dismissed_by_esc = true;
    }

    return GDK_EVENT_PROPAGATE;
}

static void
on_preview_erased_toggled(GtkCheckButton* button, EraseElementsBar* self)
{
    if(self->element_picker_content)
    {
        if(gtk_check_button_get_active(button))
        {
            GtkTextIter start, end;
            gtk_text_buffer_get_bounds(self->element_blockers_text_buffer, &start, &end);

            char* selectors = gtk_text_buffer_get_text(self->element_blockers_text_buffer, &start, &end, true);
            defer([selectors]{ g_free(selectors); });

            orion_webview_tab_content_set_preview_erased(
                self->element_picker_content,
                selectors,
                self->wrapper.current_host,
                true
            );
        }
        else
        {
            orion_webview_tab_content_set_preview_erased(
                self->element_picker_content,
                "",
                self->wrapper.current_host,
                false
            );
        }
    }
}

static void
erase_elements_bar_finish(EraseElementsBar* self)
{
    if(self->element_picker_content)
    {
        gtk_check_button_set_active(self->preview_erased_check_button, FALSE);
        g_signal_handlers_disconnect_by_data(self->element_picker_content, self);
        self->element_picker_content = NULL;
    }
    else
    {
        orion::log_error("erase_elements_bar_finish() called after element_picker_content already unset");
    }

    gtk_widget_set_visible(GTK_WIDGET(self), FALSE);
    self->wrapper.history.clear();
    self->wrapper.current_host.clear();
}

static void
on_element_picker_fail(OrionWebviewTabContent* content, EraseElementsBar* self)
{
    erase_elements_bar_finish(self);
}

static void
on_element_picker_pick(OrionWebviewTabContent* content, gboolean erase, const gchar* selector, const gchar* host, EraseElementsBar* self)
{
    if(!selector || !host || !strlen(selector)) return;

    if(self->wrapper.current_host != host)
    {
        orion::log_error("on_element_picker_pick(): called with wrong host");
        return;
    }

    if(erase)
    {
        GtkTextIter iter;

        gtk_text_buffer_get_end_iter(self->element_blockers_text_buffer, &iter);

        g_signal_handlers_block_by_func(self->element_blockers_text_buffer, gpointer(element_blockers_changed), self);
        
        if(gtk_text_buffer_get_char_count(self->element_blockers_text_buffer))
        {
            gtk_text_buffer_insert(self->element_blockers_text_buffer, &iter, ",\n", 2);
        }

        gtk_text_buffer_insert(self->element_blockers_text_buffer, &iter, selector, -1);

        g_signal_handlers_unblock_by_func(self->element_blockers_text_buffer, gpointer(element_blockers_changed), self);

        save_element_blockers(self);
    }
    else
    {
        GtkTextIter iter, match_start, match_end;
        gboolean found;

        gtk_text_buffer_get_start_iter(self->element_blockers_text_buffer, &match_end);

        g_signal_handlers_block_by_func(self->element_blockers_text_buffer, gpointer(element_blockers_changed), self);
        gtk_text_buffer_begin_user_action(self->element_blockers_text_buffer);
        while(true)
        {
            found = gtk_text_iter_forward_search(
                &match_end,
                selector,
                GTK_TEXT_SEARCH_TEXT_ONLY,
                &match_start,
                &match_end,
                NULL
            );

            if(!found) break;
            
            //walks match_start to last character before whitespace immediately preceeding us (or our own first character)
            while(gtk_text_iter_backward_char(&match_start))
            {
                if(!g_unichar_isspace(gtk_text_iter_get_char(&match_start)))
                {
                    break;
                }
            }

            //walks match_end to first character after whitespace immediately after us (or our own first character)
            while(g_unichar_isspace(gtk_text_iter_get_char(&match_end)))
            {
                gtk_text_iter_forward_char(&match_end);
            }

            //if any of these is false, its corresponding match_x iter needs to be on (or in GtkTextIter left of) a comma
            bool at_start = gtk_text_iter_is_start(&match_start); //walks us to the character preceeding whitespace before if at_start is false
            bool at_end = gtk_text_iter_is_end(&match_end); //walks us to the character past whitespace after if at_end is false

            //sanity check for whether we matched against a full selector, parenthesis is mostly sanitised by element_picker
            if ((at_start || gtk_text_iter_get_char(&match_start) == ',') &&
                (at_end || gtk_text_iter_get_char(&match_end) == ','))
            {
                //keeps leading/trailing commas with whitespace past them (ideally should not be there), treated as user intended
                if(at_start && at_end)
                {
                    //noop, just delete everything
                }
                else if(at_start)
                {
                    //expects match_end to now be sitting on a comma
                    //walk match_end to 1 before the start of the next css selector or start of buffer
                    while(gtk_text_iter_forward_char(&match_end))
                    {
                        if(!g_unichar_isspace(gtk_text_iter_get_char(&match_end)))
                        {
                            break;
                        }
                    }
                }
                else if(at_end)
                {
                    //expects match_start to now be sitting on a comma
                    //walk match_start to 1 past the end of the previous css selector or end of buffer
                    while(gtk_text_iter_backward_char(&match_start))
                    {
                        if(!g_unichar_isspace(gtk_text_iter_get_char(&match_start)))
                        {
                            gtk_text_iter_forward_char(&match_start);
                            break;
                        }
                    }
                }
                else
                {
                    //since we likely won't be leaving trailing whitespace, 
                    //it's mostly safe to just delete our portion and a comma surrounding us,
                    //since the comma pointed to by match_end is excluded in the deletion, this can be a noop
                }
                gtk_text_buffer_delete(self->element_blockers_text_buffer, &match_start, &match_end);
            }
        }
        gtk_text_buffer_end_user_action(self->element_blockers_text_buffer);
        g_signal_handlers_unblock_by_func(self->element_blockers_text_buffer, gpointer(element_blockers_changed), self);
        
        save_element_blockers(self);
    }
}

void
erase_elements_bar_start(EraseElementsBar* self)
{
    if(self->element_picker_content)
    {
        return; //duplicate operation
    }

    gtk_check_button_set_active(self->preview_erased_check_button, FALSE);

    GtkWidget* self_widget = GTK_WIDGET(self);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(self_widget));
    if(!win) return;

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    if(!view) return;

    self->element_picker_content = orion_webview_tab_view_get_current_tab_content(view);
    if(!self->element_picker_content) return;

    gtk_widget_set_visible(self_widget, TRUE);

    orion::Uri uri = orion_webview_tab_view_get_uri(view);
    self->wrapper.current_host = uri.authority();

    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper self_weak(self);

    orion::GetSettingForWebsiteOrDefaultQuery query {
        .profile_id = profile_id,
        .website = uri.authority(),
        .key_name = orion::settings::website::specific::page_tweaks_element_blocker_list.key_name()
    };
    global_core_api->get_setting_for_website_or_default(query, [self_weak, profile_id, host = uri.authority()](orion::GetSettingForWebsiteOrDefaultReply reply){
        VERIFY_LOG(reply.err);

        auto self_strong = self_weak.lock();
        if(!self_strong) return;
        EraseElementsBar* self = APP_ERASE_ELEMENTS_BAR(self_strong.get());
        GtkTextBuffer* buffer = self->element_blockers_text_buffer;
        
        g_signal_handlers_block_by_func(buffer, gpointer(element_blockers_changed), self);

        gtk_text_buffer_set_text(buffer, reply.value.c_str(), reply.value.length());

        g_signal_handlers_unblock_by_func(buffer, gpointer(element_blockers_changed), self);
        
        self->wrapper.history.push_back(reply.value);

        self->text_is_saved = true;

        unsigned long count = std::count(reply.value.cbegin(), reply.value.cend(), ',') + (reply.value.size()?1:0);
        char* n_erase_elements_label = g_strdup_printf(
            ngettext("%lu element erased", "%lu  elements erased", count),
            count
        );
        defer([n_erase_elements_label]{ g_free(n_erase_elements_label); });
        gtk_menu_button_set_label(self->text_view_menu_button, n_erase_elements_label);
    });

    g_signal_connect(self->element_picker_content, "element-picker-pick", G_CALLBACK(on_element_picker_pick), self);
    g_signal_connect(self->element_picker_content, "element-picker-fail", G_CALLBACK(on_element_picker_fail), self);
    orion_webview_tab_content_start_element_picker(self->element_picker_content);
}

bool
erase_elements_bar_get_is_running(EraseElementsBar* self)
{
    return self->element_picker_content;
}

static void
erase_elements_bar_init(EraseElementsBar* self)
{
    std::construct_at(&self->wrapper);

    gtk_widget_init_template(GTK_WIDGET(self));
}

static void
erase_elements_bar_map(GtkWidget* widget)
{
    EraseElementsBar* self = APP_ERASE_ELEMENTS_BAR(widget);

    GTK_WIDGET_CLASS(erase_elements_bar_parent_class)->map(widget);
}

static void
erase_elements_bar_unmap(GtkWidget* widget)
{
    EraseElementsBar* self = APP_ERASE_ELEMENTS_BAR(widget);
    gtk_check_button_set_active(self->preview_erased_check_button, FALSE);
    if(self->element_picker_content)
    {
        orion_webview_tab_content_set_preview_erased(self->element_picker_content, "", self->wrapper.current_host, false);
        g_signal_handlers_disconnect_by_data(self->element_picker_content, self);
        self->element_picker_content = NULL;
    }

    GTK_WIDGET_CLASS(erase_elements_bar_parent_class)->unmap(widget);
}

static void
erase_elements_bar_dispose(GObject* object)
{
    EraseElementsBar* self = APP_ERASE_ELEMENTS_BAR(object);

    gtk_widget_dispose_template(GTK_WIDGET(self), APP_TYPE_ERASE_ELEMENTS_BAR);

    if(self->element_picker_content)
    {
        orion_webview_tab_content_set_preview_erased(self->element_picker_content, "", self->wrapper.current_host, false);
        g_signal_handlers_disconnect_by_data(self->element_picker_content, self);
        self->element_picker_content = NULL;
    }

    G_OBJECT_CLASS(erase_elements_bar_parent_class)->dispose(object);
}

static void
erase_elements_bar_finalize(GObject* object)
{
    EraseElementsBar* self = APP_ERASE_ELEMENTS_BAR(object);

    std::destroy_at(&self->wrapper);

    G_OBJECT_CLASS(erase_elements_bar_parent_class)->finalize(object);
}

static void
erase_elements_bar_class_init(EraseElementsBarClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = erase_elements_bar_dispose;
    object_class->finalize = erase_elements_bar_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->map = erase_elements_bar_map;
    widget_class->unmap = erase_elements_bar_unmap;

    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/main_window/header/erase_elements_bar.ui");
    gtk_widget_class_bind_template_child(widget_class, EraseElementsBar, text_view_menu_button);
    gtk_widget_class_bind_template_child(widget_class, EraseElementsBar, element_blockers_text_view);
    gtk_widget_class_bind_template_child(widget_class, EraseElementsBar, element_blockers_text_buffer);
    gtk_widget_class_bind_template_child(widget_class, EraseElementsBar, saved_bar);
    gtk_widget_class_bind_template_child(widget_class, EraseElementsBar, preview_erased_check_button);
    gtk_widget_class_bind_template_child(widget_class, EraseElementsBar, unsaved_bar);
    gtk_widget_class_bind_template_child(widget_class, EraseElementsBar, undo_button);
    gtk_widget_class_bind_template_callback(widget_class, element_blockers_changed);
    gtk_widget_class_bind_template_callback(widget_class, element_blockers_insert_text);
    gtk_widget_class_bind_template_callback(widget_class, on_text_view_popover_closed);
    gtk_widget_class_bind_template_callback(widget_class, on_text_view_popover_key_pressed);
    gtk_widget_class_bind_template_callback(widget_class, on_preview_erased_toggled);
    gtk_widget_class_bind_template_callback(widget_class, cancel);
    gtk_widget_class_bind_template_callback(widget_class, apply);
    gtk_widget_class_bind_template_callback(widget_class, undo);
    gtk_widget_class_bind_template_callback(widget_class, erase_elements_bar_reset_selectors);
    gtk_widget_class_bind_template_callback(widget_class, done);
}