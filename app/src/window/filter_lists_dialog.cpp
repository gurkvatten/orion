#include <gdk/gdk.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "app.h"
#include "filter_lists_dialog.h"
#include "custom_filter_url_dialog.h"
#include "util/click_util.h"
#include "util/dialog_util.h"
#include "window/main_window/main_window.h"
#include "content_block/embedded_filter_lists.h"

struct StagedFilterList {
    orion::FilterListDto filter_list;
    bool update_details{};
    bool update_is_enabled{};
};

struct _FilterListsDialog
{
	AdwDialog parent;

    GtkListBox* list_box;
    GtkTextBuffer* custom_filter_buffer;
    GtkCheckButton* custom_filters_check_button;

    GtkButton* add_button;
    GtkButton* remove_button;

    GtkButton* cancel_button;
    GtkButton* done_button;

    std::vector<orion::FilterListDto>* filter_lists;
    std::vector<orion::FilterListDto>* original_filter_lists;
    std::vector<StagedFilterList>* staged_filter_lists;
    std::vector<orion::FilterListId>* staged_removed_filter_list_ids;
    bool is_loading_data{};
};

G_DEFINE_TYPE(FilterListsDialog, filter_lists_dialog, ADW_TYPE_DIALOG)

static void stage_user_filter_list(FilterListsDialog* self, const orion::FilterListDto& filter_list);
static void on_filter_list_toggled(GtkCheckButton* check_button, gpointer user_data);

namespace {
constexpr char filter_list_data_key[] = "filter-list";
}

AdwDialog*
filter_lists_dialog_new()
{
	return ADW_DIALOG(g_object_new(FILTER_LISTS_DIALOG_TYPE, NULL));
}

static int64_t
get_next_user_filter_list_position(FilterListsDialog* self)
{
    bool has_user_filter_list = false;
    int64_t max_position = std::numeric_limits<int64_t>::min();
    for (const orion::FilterListDto& filter_list : *self->filter_lists) {
        if (filter_list.filter_type != orion::FilterListType::user) continue;
        has_user_filter_list = true;
        max_position = std::max(max_position, filter_list.position);
    }

    if (!has_user_filter_list) return 0;
    if (max_position != std::numeric_limits<int64_t>::max()) {
        return max_position + 1;
    }

    // Older user lists were created at INT64_MAX, which leaves no position
    // available after them. Preserve their displayed order while making room.
    std::vector<orion::FilterListDto> normalized_filter_lists;
    int64_t position = 0;
    for (const orion::FilterListDto& filter_list : *self->filter_lists) {
        if (filter_list.filter_type != orion::FilterListType::user) continue;
        normalized_filter_lists.push_back(filter_list);
        normalized_filter_lists.back().position = position++;
    }
    for (const orion::FilterListDto& filter_list : normalized_filter_lists) {
        stage_user_filter_list(self, filter_list);
    }
    return position;
}

static void
on_add_button_clicked(GtkButton* button, gpointer user_data)
{
    FilterListsDialog* self = FILTER_LISTS_DIALOG(user_data);
    MainWindow* main_window = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!main_window) {
        orion::log_warning("%s: failed to get root main window", __func__);
        return;
    }

    AdwDialog* dialog = custom_filter_url_dialog_new();
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);
    orion::gtk::WeakRefWrapper self_weak(self);
    custom_filter_url_dialog_set_url_exists_callback(
        CUSTOM_FILTER_URL_DIALOG(dialog),
        [self_weak](const std::string& url) {
            auto self_strong = self_weak.lock();
            if (!self_strong) return false;

            FilterListsDialog* self = FILTER_LISTS_DIALOG(self_strong.get());
            return std::any_of(
                self->filter_lists->begin(),
                self->filter_lists->end(),
                [&url](const orion::FilterListDto& filter_list) {
                    return filter_list.filter_type == orion::FilterListType::user
                        && filter_list.url == url;
                });
        });
    custom_filter_url_dialog_set_submit_callback(
        CUSTOM_FILTER_URL_DIALOG(dialog),
        [self_weak, profile_id](
            const std::string& url, const std::string& title, const std::string& description) {
            auto self_strong = self_weak.lock();
            if (!self_strong) return;

            orion::FilterListDto filter_list{
                .profile_id = profile_id,
                .name = url,
                .version = "",
                .position = get_next_user_filter_list_position(
                    FILTER_LISTS_DIALOG(self_strong.get())),
                .url = url,
                .description = description,
                .display_name = title.empty() ? url : title,
                .is_enabled = true,
                .updated_at = orion::UnixTimeUtc64(0),
                .active_rules_count = 0,
                .filter_type = orion::FilterListType::user,
                .custom_filter_data = "",
                .content_hash = "",
            };
            stage_user_filter_list(FILTER_LISTS_DIALOG(self_strong.get()), filter_list);
        }
    );
    adw_dialog_present(dialog, GTK_WIDGET(self));
}

static void
on_cancel_button_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    adw_dialog_close(ADW_DIALOG(user_data));
}

struct EditFilterListActionData {
    orion::gtk::WeakRefWrapper parent_dialog;
    orion::FilterListDto filter_list;
};

static void
on_copy_filter_url_action_activate(GSimpleAction*, GVariant*, gpointer user_data)
{
    const auto* url = static_cast<const std::string*>(user_data);
    GdkDisplay* display = gdk_display_get_default();
    if (!display) {
        orion::log_warning("%s: display is unavailable", __func__);
        return;
    }

    gdk_clipboard_set_text(gdk_display_get_clipboard(display), url->c_str());
}

static void
on_edit_filter_list_action_activate(GSimpleAction*, GVariant*, gpointer user_data)
{
    auto* data = static_cast<EditFilterListActionData*>(user_data);
    auto parent_strong = data->parent_dialog.lock();
    if (!parent_strong) return;

    AdwDialog* dialog = custom_filter_url_dialog_new();
    custom_filter_url_dialog_set_filter_list(
        CUSTOM_FILTER_URL_DIALOG(dialog),
        data->filter_list
    );
    orion::gtk::WeakRefWrapper parent_weak(parent_strong.get());
    orion::FilterListDto filter_list = data->filter_list;
    custom_filter_url_dialog_set_submit_callback(
        CUSTOM_FILTER_URL_DIALOG(dialog),
        [parent_weak, filter_list](
            const std::string& url, const std::string& title, const std::string& description) mutable {
            auto parent_strong = parent_weak.lock();
            if (!parent_strong) return;

            filter_list.url = url;
            filter_list.display_name = title.empty() ? url : title;
            filter_list.description = description;
            stage_user_filter_list(FILTER_LISTS_DIALOG(parent_strong.get()), filter_list);
        }
    );
    adw_dialog_present(dialog, GTK_WIDGET(parent_strong.get()));
}

static void
on_filter_list_secondary_released(
    GtkGestureClick* click, gint, gdouble x, gdouble y, gpointer user_data)
{
    GtkPopover* popover = GTK_POPOVER(user_data);
    GdkRectangle rect{
        .x = static_cast<int>(x),
        .y = static_cast<int>(y),
        .width = 1,
        .height = 1,
    };
    gtk_popover_set_pointing_to(popover, &rect);
    gtk_popover_set_has_arrow(popover, FALSE);
    gtk_popover_popup(popover);
    gtk_gesture_set_state(GTK_GESTURE(click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
add_filter_list_context_menu(
    FilterListsDialog* self, GtkWidget* row, const orion::FilterListDto& dto)
{
    if (dto.filter_type != orion::FilterListType::builtin
        && dto.filter_type != orion::FilterListType::user) {
        return;
    }

    GSimpleActionGroup* action_group = g_simple_action_group_new();

    if (dto.filter_type == orion::FilterListType::user) {
        GSimpleAction* edit_action = g_simple_action_new("edit", NULL);
        g_signal_connect_data(
            edit_action,
            "activate",
            G_CALLBACK(on_edit_filter_list_action_activate),
            new EditFilterListActionData{
                .parent_dialog = orion::gtk::WeakRefWrapper(self),
                .filter_list = dto,
            },
            [](gpointer data, GClosure*) {
                delete static_cast<EditFilterListActionData*>(data);
            },
            G_CONNECT_DEFAULT
        );
        g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(edit_action));
        g_object_unref(edit_action);
    }

    GSimpleAction* copy_action = g_simple_action_new("copy-url", NULL);
    g_signal_connect_data(
        copy_action,
        "activate",
        G_CALLBACK(on_copy_filter_url_action_activate),
        new std::string(dto.url),
        [](gpointer data, GClosure*) {
            delete static_cast<std::string*>(data);
        },
        G_CONNECT_DEFAULT
    );
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(copy_action));
    g_object_unref(copy_action);

    gtk_widget_insert_action_group(row, "filter-list", G_ACTION_GROUP(action_group));
    g_object_unref(action_group);

    GMenu* menu = g_menu_new();
    if (dto.filter_type == orion::FilterListType::user) {
        g_menu_append(menu, _("Edit…"), "filter-list.edit");
    }
    g_menu_append(menu, _("Copy filter URL"), "filter-list.copy-url");

    GtkWidget* popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
    g_object_unref(menu);
    gtk_widget_set_parent(popover, row);
    orion::gtk::register_right_click_with_user_data(
        GTK_WIDGET(self), row, on_filter_list_secondary_released, popover);
}

static GtkWidget*
create_filter_list_widget_from_dto(FilterListsDialog* self, const orion::FilterListDto& dto)
{
    GtkWidget* row = gtk_list_box_row_new();
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* check_button = gtk_check_button_new_with_label(dto.display_name.c_str()); // do not translate
    gtk_check_button_set_active(GTK_CHECK_BUTTON(check_button), dto.is_enabled);
    gtk_box_append(GTK_BOX(box), check_button);
    GtkWidget* label = gtk_label_new(dto.description.c_str()); // should already be translated (or it was set by the user)
    gtk_widget_set_margin_start(label, 28);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_box_append(GTK_BOX(box), label);
    add_filter_list_context_menu(self, box, dto);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    g_object_set_data_full(
        G_OBJECT(row),
        filter_list_data_key,
        new orion::FilterListDto(dto),
        [](gpointer data) {
            delete static_cast<orion::FilterListDto*>(data);
        }
    );
    g_signal_connect(check_button, "toggled", G_CALLBACK(on_filter_list_toggled), self);
    return row;
}

static bool
is_same_filter_list(const orion::FilterListDto& lhs, const orion::FilterListDto& rhs)
{
    if (lhs.id != orion::FilterListId(0) && rhs.id != orion::FilterListId(0)) {
        return lhs.id == rhs.id;
    }
    return lhs.profile_id == rhs.profile_id
        && lhs.name == rhs.name;
}

static void
render_filter_list_rows(FilterListsDialog* self)
{
    gtk_widget_set_sensitive(GTK_WIDGET(self->remove_button), FALSE);
    while (GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(self->list_box))) {
        gtk_list_box_remove(self->list_box, child);
    }

    for (const orion::FilterListDto& filter_list : *self->filter_lists) {
        switch (filter_list.filter_type) {
            case orion::FilterListType::builtin:
            case orion::FilterListType::not_set:
            case orion::FilterListType::user:
                gtk_list_box_append(
                    self->list_box,
                    create_filter_list_widget_from_dto(self, filter_list)
                );
                break;
            case orion::FilterListType::custom:
            case orion::FilterListType::block_web_fonts:
                break;
        }
    }
}

static const orion::FilterListDto*
get_filter_list(GtkListBoxRow* row)
{
    if (!row) return nullptr;
    return static_cast<const orion::FilterListDto*>(
        g_object_get_data(G_OBJECT(row), filter_list_data_key));
}

static void
update_done_button_sensitivity(FilterListsDialog* self)
{
    gtk_widget_set_sensitive(
        GTK_WIDGET(self->done_button),
        !self->staged_filter_lists->empty()
            || !self->staged_removed_filter_list_ids->empty());
}

static void
stage_filter_list_enabled_change(
    FilterListsDialog* self, const orion::FilterListDto& filter_list, bool is_enabled)
{
    auto displayed = std::find_if(
        self->filter_lists->begin(),
        self->filter_lists->end(),
        [&filter_list](const orion::FilterListDto& candidate) {
            return is_same_filter_list(candidate, filter_list);
        });
    if (displayed == self->filter_lists->end()) return;
    displayed->is_enabled = is_enabled;

    auto original = std::find_if(
        self->original_filter_lists->begin(),
        self->original_filter_lists->end(),
        [&filter_list](const orion::FilterListDto& candidate) {
            return is_same_filter_list(candidate, filter_list);
        });
    const bool update_is_enabled = original != self->original_filter_lists->end()
        && original->is_enabled != is_enabled;

    auto staged = std::find_if(
        self->staged_filter_lists->begin(),
        self->staged_filter_lists->end(),
        [&filter_list](const StagedFilterList& candidate) {
            return is_same_filter_list(candidate.filter_list, filter_list);
        });
    if (staged != self->staged_filter_lists->end()) {
        staged->filter_list.is_enabled = is_enabled;
        staged->update_is_enabled = update_is_enabled;
        if (!staged->update_details && !staged->update_is_enabled) {
            self->staged_filter_lists->erase(staged);
        }
    } else if (update_is_enabled) {
        self->staged_filter_lists->push_back({
            .filter_list = *displayed,
            .update_details = false,
            .update_is_enabled = true,
        });
    }

    update_done_button_sensitivity(self);
}

static void
on_filter_list_toggled(GtkCheckButton* check_button, gpointer user_data)
{
    FilterListsDialog* self = FILTER_LISTS_DIALOG(user_data);
    GtkWidget* row = gtk_widget_get_ancestor(
        GTK_WIDGET(check_button), GTK_TYPE_LIST_BOX_ROW);
    if (!row) return;
    const orion::FilterListDto* filter_list = get_filter_list(GTK_LIST_BOX_ROW(row));
    if (!filter_list) return;

    stage_filter_list_enabled_change(
        self, *filter_list, gtk_check_button_get_active(check_button));
}

static void
on_custom_filters_toggled(GtkCheckButton* check_button, gpointer user_data)
{
    FilterListsDialog* self = FILTER_LISTS_DIALOG(user_data);
    if (self->is_loading_data) return;

    auto custom_filter_list = std::find_if(
        self->filter_lists->begin(),
        self->filter_lists->end(),
        [](const orion::FilterListDto& filter_list) {
            return filter_list.filter_type == orion::FilterListType::custom;
        });
    if (custom_filter_list == self->filter_lists->end()) return;

    stage_filter_list_enabled_change(
        self, *custom_filter_list, gtk_check_button_get_active(check_button));
}

static void
on_custom_filter_buffer_changed(GtkTextBuffer* buffer, gpointer user_data)
{
    FilterListsDialog* self = FILTER_LISTS_DIALOG(user_data);
    if (self->is_loading_data) return;

    auto custom_filter_list = std::find_if(
        self->filter_lists->begin(),
        self->filter_lists->end(),
        [](const orion::FilterListDto& filter_list) {
            return filter_list.filter_type == orion::FilterListType::custom;
        });
    if (custom_filter_list == self->filter_lists->end()) return;

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar* text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    custom_filter_list->custom_filter_data = text;
    g_free(text);

    auto original = std::find_if(
        self->original_filter_lists->begin(),
        self->original_filter_lists->end(),
        [&custom_filter_list](const orion::FilterListDto& filter_list) {
            return is_same_filter_list(filter_list, *custom_filter_list);
        });
    const bool update_details = original != self->original_filter_lists->end()
        && original->custom_filter_data != custom_filter_list->custom_filter_data;

    auto staged = std::find_if(
        self->staged_filter_lists->begin(),
        self->staged_filter_lists->end(),
        [&custom_filter_list](const StagedFilterList& candidate) {
            return is_same_filter_list(candidate.filter_list, *custom_filter_list);
        });
    if (staged != self->staged_filter_lists->end()) {
        staged->filter_list.custom_filter_data = custom_filter_list->custom_filter_data;
        staged->update_details = update_details;
        if (!staged->update_details && !staged->update_is_enabled) {
            self->staged_filter_lists->erase(staged);
        }
    } else if (update_details) {
        self->staged_filter_lists->push_back({
            .filter_list = *custom_filter_list,
            .update_details = true,
            .update_is_enabled = false,
        });
    }

    update_done_button_sensitivity(self);
}

static void
on_filter_list_row_selected(GtkListBox*, GtkListBoxRow* row, gpointer user_data)
{
    FilterListsDialog* self = FILTER_LISTS_DIALOG(user_data);
    const orion::FilterListDto* filter_list = get_filter_list(row);
    gtk_widget_set_sensitive(
        GTK_WIDGET(self->remove_button),
        filter_list && filter_list->filter_type == orion::FilterListType::user);
}

static void
on_remove_button_clicked(GtkButton*, gpointer user_data)
{
    FilterListsDialog* self = FILTER_LISTS_DIALOG(user_data);
    const orion::FilterListDto* selected = get_filter_list(
        gtk_list_box_get_selected_row(self->list_box));
    if (!selected || selected->filter_type != orion::FilterListType::user) return;

    const orion::FilterListDto filter_list = *selected;
    std::erase_if(
        *self->staged_filter_lists,
        [&filter_list](const StagedFilterList& candidate) {
            return is_same_filter_list(candidate.filter_list, filter_list);
        });

    if (filter_list.id != orion::FilterListId(0)
        && std::find(
            self->staged_removed_filter_list_ids->begin(),
            self->staged_removed_filter_list_ids->end(),
            filter_list.id) == self->staged_removed_filter_list_ids->end()) {
        self->staged_removed_filter_list_ids->push_back(filter_list.id);
    }

    std::erase_if(
        *self->filter_lists,
        [&filter_list](const orion::FilterListDto& candidate) {
            return is_same_filter_list(candidate, filter_list);
        });
    render_filter_list_rows(self);
    update_done_button_sensitivity(self);
}

static void
stage_user_filter_list(FilterListsDialog* self, const orion::FilterListDto& filter_list)
{
    orion::FilterListDto updated_filter_list = filter_list;
    auto displayed = std::find_if(
        self->filter_lists->begin(),
        self->filter_lists->end(),
        [&filter_list](const orion::FilterListDto& candidate) {
            return is_same_filter_list(candidate, filter_list);
        }
    );
    if (displayed != self->filter_lists->end()) {
        updated_filter_list.is_enabled = displayed->is_enabled;
    }

    auto staged = std::find_if(
        self->staged_filter_lists->begin(),
        self->staged_filter_lists->end(),
        [&filter_list](const StagedFilterList& candidate) {
            return is_same_filter_list(candidate.filter_list, filter_list);
        }
    );
    if (staged == self->staged_filter_lists->end()) {
        self->staged_filter_lists->push_back({
            .filter_list = updated_filter_list,
            .update_details = true,
            .update_is_enabled = false,
        });
    } else {
        staged->filter_list = updated_filter_list;
        staged->update_details = true;
    }

    if (displayed == self->filter_lists->end()) {
        self->filter_lists->push_back(updated_filter_list);
    } else {
        *displayed = updated_filter_list;
    }

    render_filter_list_rows(self);
    update_done_button_sensitivity(self);
}

static void
load_data(FilterListsDialog* self)
{
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!win) {
        orion::log_warning("%s: failed to get root main window", __func__);
        return;
    }
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    WebExtensionWebViewFactory& webview_factory = main_window_get_current_webview_factory(win);

    orion::GetFilterListsQuery query{
        .profile_id = profile_id,
    };

    orion::gtk::WeakRefWrapper self_weak(self);

    global_core_api->get_filter_lists(query, [self_weak](orion::GetFilterListsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.err) return;

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        FilterListsDialog* self = FILTER_LISTS_DIALOG(self_strong.get());

        const auto& metadata_list = orion::contentblock::get_metadata_list();
        std::map<std::string, orion::contentblock::FilterListMetadata> metadata_map;
        for (const auto& metadata : metadata_list) {
            metadata_map[metadata.name] = metadata;
        }

        self->is_loading_data = true;
        bool is_custom_filter_present = false;
        for (auto& dto : reply.filter_lists) {
            bool is_handled = false;

            switch (dto.filter_type) {
                case orion::FilterListType::builtin:
                case orion::FilterListType::not_set: {
                    auto it = metadata_map.find(dto.name);
                    if (it != metadata_map.end()) {
                        auto metadata = it->second;
                        dto.description = metadata.description;
                        dto.display_name = metadata.display_name;
                    }
                    is_handled = true;
                    break;
                }
                case orion::FilterListType::block_web_fonts:
                case orion::FilterListType::user: {
                    is_handled = true;
                    break;
                }
                case orion::FilterListType::custom: {
                    is_handled = true;
                    if (is_custom_filter_present) {
                        orion::log_warning("load_data: multiple custom filter lists present");
                    } else {
                        gtk_text_buffer_set_text(self->custom_filter_buffer, dto.custom_filter_data.c_str(), -1);
                        gtk_check_button_set_active(self->custom_filters_check_button, dto.is_enabled);
                        is_custom_filter_present = true;
                    }
                    break;
                }
            }
            if (!is_handled) {
                orion::log_warning("load_data: unhandled filter list type: %d",
                    static_cast<int>(dto.filter_type));
            }
        }
        if (!is_custom_filter_present) {
            orion::log_warning("load_data: no custom filter list present");
        }

        *self->filter_lists = reply.filter_lists;
        *self->original_filter_lists = std::move(reply.filter_lists);
        self->staged_filter_lists->clear();
        self->staged_removed_filter_list_ids->clear();
        self->is_loading_data = false;
        render_filter_list_rows(self);
        update_done_button_sensitivity(self);
    });
}

static orion::CreateFilterListQuery
make_create_or_update_query(const StagedFilterList& staged)
{
    const orion::FilterListDto& filter_list = staged.filter_list;
    return {
        .profile_id = filter_list.profile_id,
        .name = filter_list.name,
        .version = filter_list.version,
        .position = filter_list.position,
        .url = filter_list.url,
        .display_name = filter_list.display_name,
        .description = filter_list.description,
        .is_enabled = filter_list.is_enabled,
        .updated_at = filter_list.updated_at,
        .active_rules_count = filter_list.active_rules_count,
        .filter_type = filter_list.filter_type,
        .custom_filter_data = filter_list.custom_filter_data,
        .content_hash = filter_list.content_hash,
        .update_is_enabled = staged.update_is_enabled,
        .update_updated_at = false,
        .update_active_rules_count = false,
        .update_version = false,
        // content_hash describes the data successfully compiled into WebKit. It
        // is updated by filter_store_save_cb after the staged text is compiled.
        .update_content_hash = false,
    };
}

struct SaveStagedFilterListsState {
    orion::gtk::WeakRefWrapper dialog;
    size_t remaining{};
    bool has_error{};
};

static void
on_staged_filter_list_operation_finished(
    const std::shared_ptr<SaveStagedFilterListsState>& state, bool has_error)
{
    state->has_error = state->has_error || has_error;
    if (--state->remaining != 0) return;

    auto dialog_strong = state->dialog.lock();
    if (!dialog_strong) return;
    FilterListsDialog* self = FILTER_LISTS_DIALOG(dialog_strong.get());
    if (state->has_error) {
        gtk_widget_set_sensitive(GTK_WIDGET(self->done_button), TRUE);
        return;
    }

    MainWindow* main_window = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (main_window) {
        // Reconcile the final saved state with this profile/private-mode store.
        // Enabled embedded lists missing from WebKit are compiled and saved;
        // disabled lists are removed by the same setup pass. Enabled remote lists
        // are downloaded and compiled without keeping the dialog open.
        orion::contentblock::setup_filter_lists(
            &main_window_get_current_webview_factory(main_window), true);
    } else {
        orion::log_warning(
            "on_staged_filter_list_operation_finished: failed to get root main window");
    }

    adw_dialog_close(ADW_DIALOG(self));
}

static void
on_done_button_clicked(GtkButton* button, gpointer user_data)
{
    FilterListsDialog* self = FILTER_LISTS_DIALOG(user_data);
    if (self->staged_filter_lists->empty()
        && self->staged_removed_filter_list_ids->empty()) {
        adw_dialog_close(ADW_DIALOG(self));
        return;
    }

    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    auto state = std::make_shared<SaveStagedFilterListsState>(SaveStagedFilterListsState{
        .dialog = orion::gtk::WeakRefWrapper(self),
        .remaining = self->staged_filter_lists->size()
            + (self->staged_removed_filter_list_ids->empty() ? 0 : 1),
    });

    for (const StagedFilterList& staged : *self->staged_filter_lists) {
        global_core_api->create_or_update_filter_list(
            make_create_or_update_query(staged),
            [state,staged](orion::CreateFilterListReply reply) {
                VERIFY_LOG(reply.err);

                if (staged.update_is_enabled
                    && !staged.filter_list.is_enabled
                    && staged.filter_list.filter_type == orion::FilterListType::builtin) {
                    auto dialog_strong = state->dialog.lock();
                    if (dialog_strong) {
                        MainWindow* main_window = orion::gtk::get_root_main_window(
                            GTK_WIDGET(dialog_strong.get()));
                        if (main_window) {
                            orion::contentblock::remove_filter_list_from_webkit_store(
                                &main_window_get_current_webview_factory(main_window),
                                staged.filter_list.name,
                                nullptr);
                        }
                    }
                }

                on_staged_filter_list_operation_finished(
                    state, static_cast<bool>(reply.err));
            }
        );
    }

    if (!self->staged_removed_filter_list_ids->empty()) {
        global_core_api->delete_filter_lists(
            *self->staged_removed_filter_list_ids,
            [state](orion::Error err) {
                VERIFY_LOG(err);
                on_staged_filter_list_operation_finished(state, static_cast<bool>(err));
            }
        );
    }
}

static void
filter_lists_dialog_root(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(filter_lists_dialog_parent_class)->root(widget); // chain up first

    load_data(FILTER_LISTS_DIALOG(widget));
}

static void
filter_lists_dialog_init(FilterListsDialog* self)
{
    self->filter_lists = new std::vector<orion::FilterListDto>();
	self->original_filter_lists = new std::vector<orion::FilterListDto>();
	self->staged_filter_lists = new std::vector<StagedFilterList>();
	self->staged_removed_filter_list_ids = new std::vector<orion::FilterListId>();
	gtk_widget_init_template(GTK_WIDGET(self));

    g_signal_connect(self->add_button, "clicked", G_CALLBACK(on_add_button_clicked), self);
    g_signal_connect(self->remove_button, "clicked", G_CALLBACK(on_remove_button_clicked), self);
    g_signal_connect(self->list_box, "row-selected", G_CALLBACK(on_filter_list_row_selected), self);
    g_signal_connect(
        self->custom_filters_check_button,
        "toggled",
        G_CALLBACK(on_custom_filters_toggled),
        self);
    g_signal_connect(
        self->custom_filter_buffer,
        "changed",
        G_CALLBACK(on_custom_filter_buffer_changed),
        self);
    g_signal_connect(self->cancel_button, "clicked", G_CALLBACK(on_cancel_button_clicked), self);
    g_signal_connect(self->done_button, "clicked", G_CALLBACK(on_done_button_clicked), self);
}

static void
filter_lists_dialog_dispose(GObject* object)
{
    FilterListsDialog* self = FILTER_LISTS_DIALOG(object);
    if (self->list_box) {
        // Must be disconnected explicitly here to avoid referencing already destroyed widgets
        // in on_filter_list_row_selected handler (when Cancel button is clicked)
        g_signal_handlers_disconnect_by_data(self->list_box, self);
    }

    G_OBJECT_CLASS(filter_lists_dialog_parent_class)->dispose(object);
}

static void
filter_lists_dialog_finalize(GObject* object)
{
    FilterListsDialog* self = FILTER_LISTS_DIALOG(object);
    delete self->filter_lists;
    delete self->original_filter_lists;
    delete self->staged_filter_lists;
    delete self->staged_removed_filter_list_ids;
    G_OBJECT_CLASS(filter_lists_dialog_parent_class)->finalize(object);
}

static void
filter_lists_dialog_class_init(FilterListsDialogClass* klass)
{
	G_OBJECT_CLASS(klass)->dispose = filter_lists_dialog_dispose;
	G_OBJECT_CLASS(klass)->finalize = filter_lists_dialog_finalize;
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/filter_lists_dialog.ui");

    GTK_WIDGET_CLASS(klass)->root = filter_lists_dialog_root;
    
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), FilterListsDialog, list_box);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), FilterListsDialog, custom_filter_buffer);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), FilterListsDialog, custom_filters_check_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), FilterListsDialog, add_button);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), FilterListsDialog, remove_button);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), FilterListsDialog, cancel_button);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), FilterListsDialog, done_button);
}
