#include <gdk/gdk.h>
#include <common/src/log.h>
#include <core/dto/search_engine_dto.h>

#include "../app.h"
#include "add_search_engine_dialog.h"
#include "util/dialog_util.h"
#include "window/main_window/main_window.h"

struct _AddSearchEngineDialog
{
	AdwDialog parent;

    orion::SearchEngineId id;
    GtkEntry* name_entry;
    GtkEntry* search_url_template_entry;
    GtkEntry* suggestions_url_template_entry;
    GtkEntry* keyword_entry;
};

G_DEFINE_TYPE(AddSearchEngineDialog, add_search_engine_dialog, ADW_TYPE_DIALOG)

AdwDialog*
add_search_engine_dialog_new(const orion::SearchEngineDto& dto)
{
    AddSearchEngineDialog* dialog = ADD_SEARCH_ENGINE_DIALOG(g_object_new(ADD_SEARCH_ENGINE_DIALOG_TYPE, NULL));

    dialog->id = dto.id;
    gtk_editable_set_text(GTK_EDITABLE(dialog->name_entry), dto.name.c_str());
    gtk_editable_set_text(GTK_EDITABLE(dialog->search_url_template_entry), dto.search_url_template.c_str());
    gtk_editable_set_text(GTK_EDITABLE(dialog->suggestions_url_template_entry), dto.suggestions_url_template.c_str());
    gtk_editable_set_text(GTK_EDITABLE(dialog->keyword_entry), dto.keyword.c_str());

	return ADW_DIALOG(dialog);
}

void
show_error_dialog(GtkWidget* parent, const std::string& heading, const std::string& body_markup)
{
    AdwAlertDialog* dialog = ADW_ALERT_DIALOG(adw_alert_dialog_new(NULL, NULL));

    adw_alert_dialog_set_heading(dialog, heading.c_str());
	adw_alert_dialog_format_body_markup(dialog, "%s", body_markup.c_str());

	adw_alert_dialog_add_responses(dialog, "ok", _("_OK"), NULL);
	adw_alert_dialog_set_response_appearance(dialog, "ok", ADW_RESPONSE_SUGGESTED);

	adw_alert_dialog_set_default_response(dialog, "ok");
	adw_alert_dialog_set_close_response(dialog, "ok");

    adw_dialog_present(ADW_DIALOG(dialog), parent);
}

static void
on_save_engine_button_clicked(GtkButton* button, gpointer user_data)
{
    AddSearchEngineDialog* dialog = ADD_SEARCH_ENGINE_DIALOG(user_data);
    GtkWindow* parent = orion::gtk::get_parent_window(GTK_WIDGET(dialog));
    if (!parent) {
        orion::log_warning("on_save_engine_button_clicked: failed to get parent window of dialog");
        return;
    }

    MainWindow* main_window = MAIN_WINDOW(parent);
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);

    std::string name = gtk_editable_get_text(GTK_EDITABLE(dialog->name_entry));
    std::string search_url_template = gtk_editable_get_text(GTK_EDITABLE(dialog->search_url_template_entry));
    std::string suggestions_url_template = gtk_editable_get_text(GTK_EDITABLE(dialog->suggestions_url_template_entry));
    std::string keyword = gtk_editable_get_text(GTK_EDITABLE(dialog->keyword_entry));

    if (name.empty()) {
        std::string heading = _("Search Engine Name Error");
        std::string body_markup = _("Search Engine name cannot be empty.");
        show_error_dialog(GTK_WIDGET(dialog), heading, body_markup);
        return;
    }

    if (search_url_template.empty()) {
        std::string heading = _("Search URL Template Error");
        std::string body_markup = _("Search URL Template cannot be empty.");
        show_error_dialog(GTK_WIDGET(dialog), heading, body_markup);
        return;
    }

    if (search_url_template.find("%s") == std::string::npos) {
        std::string heading = _("Invalid Search URL Template");
        std::string body_markup = _("Missing query placeholder.\n\nMake sure to include `%s` in the search URL template.");
        show_error_dialog(GTK_WIDGET(dialog), heading, body_markup);
        return;
    }

    if (!suggestions_url_template.empty() && suggestions_url_template.find("%s") == std::string::npos) {
        std::string heading = _("Invalid Suggestions URL Template");
        std::string body_markup = _("Missing query placeholder.\n\nMake sure to include `%s` in the suggestions URL template.");
        show_error_dialog(GTK_WIDGET(dialog), heading, body_markup);
        return;
    }

    orion::gtk::WeakRefWrapper dialog_weak(dialog);

    if (dialog->id.value) {
        // This is update of an existing search engine
        orion::UpdateSearchEngineQuery query{
            .id = dialog->id,
            .name = name,
            .search_url_template = search_url_template,
            .suggestions_url_template = suggestions_url_template,
            .keyword = keyword,
            .update_name = true,
            .update_search_url_template = true,
            .update_suggestions_url_template = true,
            .update_keyword = true,
        };
        global_core_api->update_search_engine(query, [dialog_weak](orion::Error err){
            VERIFY_LOG(err);

            auto dialog_strong = dialog_weak.get();
            if (!dialog_strong) return;
            GtkWidget* dialog = GTK_WIDGET(dialog_strong);

            adw_dialog_close(ADW_DIALOG(dialog));
        });
        return;
    }
    
    // This is creation of a new search engine
    orion::CreateSearchEngineQuery query{
        .profile_id = profile_id,
        .name = name,
        .search_url_template = search_url_template,
        .suggestions_url_template = suggestions_url_template,
        .keyword = keyword,
    };
    global_core_api->get_search_engines_by_query({profile_id},
        [query,dialog_weak](orion::GetSearchEnginesReply reply) {
            VERIFY_LOG(reply.err);

            auto dialog_strong = dialog_weak.get();
            if (!dialog_strong) return;
            GtkWidget* dialog = GTK_WIDGET(dialog_strong);

            for (const auto& search_engine : reply.search_engines) {
                if (search_engine.name == query.name) {
                    std::string heading = _("Search Engine Name Error");
                    std::string body_markup = _("Search Engine name must be unique.");
                    show_error_dialog(dialog, heading, body_markup);
                    return;
                }
            }

            global_core_api->create_search_engine(query, [dialog_weak](orion::CreateSearchEngineReply reply) {
                VERIFY_LOG(reply.err);

                auto dialog_strong = dialog_weak.get();
                if (!dialog_strong) return;
                GtkWidget* dialog = GTK_WIDGET(dialog_strong);

                adw_dialog_close(ADW_DIALOG(dialog));
            });
        });
}

static void
on_close_button_clicked(GtkButton* button, gpointer user_data)
{
	AdwDialog* dialog = ADW_DIALOG(user_data);
	adw_dialog_close(dialog);
}

static void
add_search_engine_dialog_finalize(GObject* gobject)
{
    AddSearchEngineDialog* self = ADD_SEARCH_ENGINE_DIALOG(gobject);
    std::destroy_at(&self->id);

    G_OBJECT_CLASS(add_search_engine_dialog_parent_class)->finalize(gobject);
}

static void
add_search_engine_dialog_init(AddSearchEngineDialog* self)
{
    std::construct_at(&self->id);

	gtk_widget_init_template(GTK_WIDGET(self));
}

static void 
add_search_engine_dialog_class_init(AddSearchEngineDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/add_search_engine_dialog.ui");

    G_OBJECT_CLASS(klass)->finalize = add_search_engine_dialog_finalize;

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_close_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_save_engine_button_clicked);

    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AddSearchEngineDialog, name_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AddSearchEngineDialog, search_url_template_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AddSearchEngineDialog, suggestions_url_template_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AddSearchEngineDialog, keyword_entry);
}
