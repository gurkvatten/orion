#include <gdk/gdk.h>

#include <common/src/filter_list.h>
#include <common/src/uuid.h>
#include <core/downloader.h>

#include <filesystem>
#include <memory>
#include <regex>

#include "../app.h"
#include "custom_filter_url_dialog.h"

namespace {
constexpr char submit_callback_data_key[] = "submit-callback";
constexpr char url_exists_callback_data_key[] = "url-exists-callback";
}

struct _CustomFilterUrlDialog
{
	AdwDialog parent;

    GtkEntry* url_entry;
    GtkButton* cancel_button;
    GtkButton* add_button;
    GtkEntry* title_entry;
    GtkTextView* description_text_view;

    std::shared_ptr<orion::Downloader> downloader;
    std::string download_path;
    bool is_editing;
    bool is_downloading;
};

G_DEFINE_TYPE(CustomFilterUrlDialog, custom_filter_url_dialog, ADW_TYPE_DIALOG)

AdwDialog*
custom_filter_url_dialog_new()
{
	return ADW_DIALOG(g_object_new(CUSTOM_FILTER_URL_DIALOG_TYPE, NULL));
}

void
custom_filter_url_dialog_set_values(
    CustomFilterUrlDialog* self, const char* url, const char* title, const char* description)
{
    gtk_editable_set_text(GTK_EDITABLE(self->url_entry), url);
    gtk_editable_set_text(GTK_EDITABLE(self->title_entry), title);
    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer(self->description_text_view),
        description,
        -1
    );
    gtk_editable_set_editable(GTK_EDITABLE(self->url_entry), FALSE);
    self->is_editing = true;
    gtk_button_set_label(self->add_button, _("Save"));
}

void
custom_filter_url_dialog_set_filter_list(
    CustomFilterUrlDialog* self, const orion::FilterListDto& filter_list)
{
    custom_filter_url_dialog_set_values(
        self,
        filter_list.url.c_str(),
        filter_list.display_name.c_str(),
        filter_list.description.c_str()
    );
}

void
custom_filter_url_dialog_set_submit_callback(
    CustomFilterUrlDialog* self, CustomFilterUrlDialogSubmitCallback callback)
{
    g_object_set_data_full(
        G_OBJECT(self),
        submit_callback_data_key,
        new CustomFilterUrlDialogSubmitCallback(std::move(callback)),
        [](gpointer data) {
            delete static_cast<CustomFilterUrlDialogSubmitCallback*>(data);
        }
    );
}

void
custom_filter_url_dialog_set_url_exists_callback(
    CustomFilterUrlDialog* self, CustomFilterUrlDialogUrlExistsCallback callback)
{
    g_object_set_data_full(
        G_OBJECT(self),
        url_exists_callback_data_key,
        new CustomFilterUrlDialogUrlExistsCallback(std::move(callback)),
        [](gpointer data) {
            delete static_cast<CustomFilterUrlDialogUrlExistsCallback*>(data);
        }
    );
}

static void
on_cancel_button_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_url_entry_changed(GtkEditable* editable, gpointer user_data)
{
    static const std::regex url_regex(R"(^https?://.)");
    CustomFilterUrlDialog* self = CUSTOM_FILTER_URL_DIALOG(user_data);
    const char* url = gtk_editable_get_text(editable);

    gtk_widget_set_sensitive(
        GTK_WIDGET(self->add_button),
        !self->is_downloading && std::regex_search(url, url_regex));
}

static void
set_downloading(CustomFilterUrlDialog* self, bool is_downloading)
{
    self->is_downloading = is_downloading;
    gtk_widget_set_sensitive(GTK_WIDGET(self->url_entry), !is_downloading);
    gtk_widget_set_sensitive(GTK_WIDGET(self->title_entry), !is_downloading);
    gtk_widget_set_sensitive(GTK_WIDGET(self->description_text_view), !is_downloading);
    gtk_button_set_label(
        self->add_button,
        is_downloading ? _("Downloading…") : (self->is_editing ? _("Save") : _("Add"))
    );

    const char* url = gtk_editable_get_text(GTK_EDITABLE(self->url_entry));
    static const std::regex url_regex(R"(^https?://.)");
    gtk_widget_set_sensitive(
        GTK_WIDGET(self->add_button),
        !is_downloading && std::regex_search(url, url_regex)
    );
}

static std::string
get_description(CustomFilterUrlDialog* self)
{
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->description_text_view);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char* text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    std::string description = text;
    g_free(text);
    return description;
}

static void
submit(CustomFilterUrlDialog* self, const std::string& url, const std::string& title,
    const std::string& description)
{
    const auto* submit_callback = static_cast<const CustomFilterUrlDialogSubmitCallback*>(
        g_object_get_data(G_OBJECT(self), submit_callback_data_key));
    if (!submit_callback) {
        orion::log_warning("%s: submit callback is not set", __func__);
        return;
    }

    (*submit_callback)(url, title, description);
    adw_dialog_close(ADW_DIALOG(self));
}

static void
show_download_error(CustomFilterUrlDialog* self)
{
    set_downloading(self, false);

    AdwDialog* dialog = adw_alert_dialog_new(
        _("Filter List Download Failed"),
        _("The filter list could not be downloaded. Check the URL and try again."));
    adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "ok", _("OK"));
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "ok");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "ok");
    adw_dialog_present(dialog, GTK_WIDGET(self));
}

static void
on_filter_list_download_finished(
    orion::gtk::WeakRefWrapper self_weak,
    const std::string& url,
    std::string title,
    std::string description,
    const std::string& download_path,
    orion::Error err)
{
    auto self_strong = self_weak.lock();
    if (!self_strong) {
        std::error_code remove_error;
        std::filesystem::remove(download_path, remove_error);
        return;
    }
    CustomFilterUrlDialog* self = CUSTOM_FILTER_URL_DIALOG(self_strong.get());

    if (err) {
        std::error_code remove_error;
        std::filesystem::remove(download_path, remove_error);
        self->download_path.clear();
        orion::log_warning(
            "%s: failed to download filter list '%s': %s",
            __func__, url.c_str(), err.to_string().c_str());
        show_download_error(self);
        return;
    }

    gchar* contents = nullptr;
    gsize contents_length = 0;
    GError* read_error = nullptr;
    gboolean read_success = g_file_get_contents(
        download_path.c_str(), &contents, &contents_length, &read_error);
    std::error_code remove_error;
    std::filesystem::remove(download_path, remove_error);
    self->download_path.clear();

    if (!read_success) {
        orion::log_warning(
            "%s: failed to read downloaded filter list '%s': %s",
            __func__, url.c_str(), read_error ? read_error->message : "unknown error");
        g_clear_error(&read_error);
        show_download_error(self);
        return;
    }

    std::unique_ptr<orion::FilterList, decltype(&orion::filter_list_free)> filter_list(
        orion::filter_list_new(), orion::filter_list_free);
    if (title.empty()) {
        orion::Error parse_error = orion::filter_list_parse_title(filter_list.get(), contents, &title);
        if (parse_error) {
            orion::log_warning(
                "%s: failed to parse filter list title: %s",
                __func__, parse_error.to_string().c_str());
        }
    }
    if (description.empty()) {
        orion::Error parse_error = orion::filter_list_parse_description(
            filter_list.get(), contents, &description);
        if (parse_error) {
            orion::log_warning(
                "%s: failed to parse filter list description: %s",
                __func__, parse_error.to_string().c_str());
        }
    }
    g_free(contents);

    if (!title.empty()) {
        gtk_editable_set_text(GTK_EDITABLE(self->title_entry), title.c_str());
    }
    if (!description.empty()) {
        gtk_text_buffer_set_text(
            gtk_text_view_get_buffer(self->description_text_view), description.c_str(), -1);
    }
    submit(self, url, title, description);
}

static void
on_add_button_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    CustomFilterUrlDialog* self = CUSTOM_FILTER_URL_DIALOG(user_data);
    std::string url = gtk_editable_get_text(GTK_EDITABLE(self->url_entry));
    std::string title = gtk_editable_get_text(GTK_EDITABLE(self->title_entry));
    std::string description = get_description(self);

    const auto* url_exists_callback = static_cast<const CustomFilterUrlDialogUrlExistsCallback*>(
        g_object_get_data(G_OBJECT(self), url_exists_callback_data_key));
    if (!self->is_editing && url_exists_callback && (*url_exists_callback)(url)) {
        AdwDialog* dialog = adw_alert_dialog_new(
            _("Error"), _("A custom filter with this URL already exists."));
        adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "ok", _("OK"));
        adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "ok");
        adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "ok");
        adw_dialog_present(dialog, GTK_WIDGET(self));
        return;
    }

    std::shared_ptr<orion::Downloader> downloader = global_core_api->create_downloader();
    if (!downloader) {
        orion::log_warning("%s: failed to create filter list downloader", __func__);
        show_download_error(self);
        return;
    }

    self->download_path = (
        std::filesystem::path(g_get_tmp_dir())
        / ("orion-filter-list-" + orion::generate_uuid_v4())
    ).string();
    orion::log_debug("%s: download path set to %s", __func__, self->download_path.c_str());
    self->downloader = downloader;
    set_downloading(self, true);

    orion::gtk::WeakRefWrapper self_weak(self);
    std::string download_path = self->download_path;
    downloader->set_target_path(download_path);
    downloader->set_uri(url);
    downloader->set_finished_cb(
        [self_weak, url, title, description, download_path](orion::Error err) mutable {
            on_filter_list_download_finished(
                self_weak, url, std::move(title), std::move(description), download_path, std::move(err));
        }
    );
    downloader->start_download();
}

static void
custom_filter_url_dialog_finalize(GObject* object)
{
    CustomFilterUrlDialog* self = CUSTOM_FILTER_URL_DIALOG(object);
    if (self->downloader) {
        self->downloader->set_finished_cb({});
        self->downloader->cancel();
        self->downloader.reset();
    }
    if (!self->download_path.empty()) {
        std::error_code remove_error;
        std::filesystem::remove(self->download_path, remove_error);
    }
    std::destroy_at(&self->downloader);
    std::destroy_at(&self->download_path);

    G_OBJECT_CLASS(custom_filter_url_dialog_parent_class)->finalize(object);
}

static void
custom_filter_url_dialog_init(CustomFilterUrlDialog* win)
{
	std::construct_at(&win->downloader);
	std::construct_at(&win->download_path);
	gtk_widget_init_template(GTK_WIDGET(win));

    g_signal_connect(win->url_entry, "changed", G_CALLBACK(on_url_entry_changed), win);
    g_signal_connect(win->cancel_button, "clicked", G_CALLBACK(on_cancel_button_clicked), win);
    g_signal_connect(win->add_button, "clicked", G_CALLBACK(on_add_button_clicked), win);
}

static void
custom_filter_url_dialog_class_init(CustomFilterUrlDialogClass* klass)
{
	G_OBJECT_CLASS(klass)->finalize = custom_filter_url_dialog_finalize;
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/custom_filter_url_dialog.ui");
    
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomFilterUrlDialog, url_entry);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomFilterUrlDialog, cancel_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomFilterUrlDialog, add_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomFilterUrlDialog, title_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), CustomFilterUrlDialog, description_text_view);
}
