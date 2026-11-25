#include <gtk/gtk.h>

#include <vector>
#include <string>
#include <iostream>
#include <functional>

#include <common/src/defer.h>

#include "app.h"

#include "popular_extensions_dialog.h"

#include "data_item/popular_extension_item.h"

#include "util/dialog_util.h"
#include "util/scroll_util.h"

#include "bookmarks_dialog.h"
#include "add_web_extension_dialog.h"

#include "window/main_window/main_window.h"
#include "window/url_image/url_image.h"

static void
get_extension_list(WebExtensions::WebExtensionsInternalStore::FetchExtensionListReply reply) {
    global_core_api->internal_store().get_extension_list(reply);
}

static void
fetch_extension(WebExtensions::ExtensionInfo extension, WebExtensions::WebExtensionsInternalStore::FetchCompletionHandler completion_handler) {
    global_core_api->internal_store().fetch_extension(extension, completion_handler);
}

struct _PopularExtensionsDialog
{
	AdwDialog parent;

	GtkListBox* sidebar_list_box;

    GtkAdjustment* content_vadjustment;

    GtkLabel* password_managers_label;

	GtkListBoxRow* debug_sidebar_row;
	GtkWidget* debug_section;
	GtkGridView* debug_grid_view;
	GtkGridView* password_managers_grid_view;
    GtkLabel* ads_and_tracking_label;
	GtkGridView* ads_and_tracking_grid_view;
    GtkLabel* vpns_label;
	GtkGridView* vpns_grid_view;
    GtkLabel* browse_better_label;
	GtkGridView* browse_better_grid_view;

    orion::ObserverId extensions_change_observer_id;
};

G_DEFINE_TYPE(PopularExtensionsDialog, popular_extensions_dialog, ADW_TYPE_DIALOG)

static GListStore*
list_store_init(const std::vector<WebExtensions::ExtensionInfo>& extension_info_list)
{
    GListStore* store = g_list_store_new(G_TYPE_OBJECT);
    for(const auto& extension : extension_info_list)
    {
        UNIQUE_PTR(PopularExtensionItem, g_object_unref) item(popular_extension_item_new(extension));
        g_list_store_append(store, item.get());
    }
    return store;
}

static GtkSelectionModel*
extension_info_list_model(const std::vector<WebExtensions::ExtensionInfo>& extension_info_list)
{
    GListStore* store = list_store_init(extension_info_list);
    GtkNoSelection* selection = gtk_no_selection_new(G_LIST_MODEL(store));
    return GTK_SELECTION_MODEL(selection);
}

static detached_task
popular_extensions_dialog_init_from_internal_store(PopularExtensionsDialog* self)
{
    orion::gtk::WeakRefWrapper weak_ref(self);
    WebExtensions::ExtensionList extension_list = co_await async(get_extension_list);
    auto ref = weak_ref.lock();
    if(!ref) {
        co_return;
    }
    self = POPULAR_EXTENSIONS_DIALOG(ref.get());
    gtk_grid_view_set_model(self->debug_grid_view, extension_info_list_model(extension_list.debug.value_or(std::vector<WebExtensions::ExtensionInfo>())));
    gtk_grid_view_set_model(self->password_managers_grid_view, extension_info_list_model(extension_list.passwordManagers.value_or(std::vector<WebExtensions::ExtensionInfo>())));
    gtk_grid_view_set_model(self->ads_and_tracking_grid_view, extension_info_list_model(extension_list.adBlockers.value_or(std::vector<WebExtensions::ExtensionInfo>())));
    gtk_grid_view_set_model(self->vpns_grid_view, extension_info_list_model(extension_list.vpns.value_or(std::vector<WebExtensions::ExtensionInfo>())));
    gtk_grid_view_set_model(self->browse_better_grid_view, extension_info_list_model(extension_list.browserBetter.value_or(std::vector<WebExtensions::ExtensionInfo>())));
}

AdwDialog*
popular_extensions_dialog_new()
{
	return ADW_DIALOG(g_object_new(POPULAR_EXTENSIONS_DIALOG_TYPE, NULL));
}

static void
on_manage_extensions_button_clicked(GtkButton* button, gpointer user_data)
{
	AdwDialog* dialog = bookmarks_dialog_new();
	adw_dialog_present(dialog, GTK_WIDGET(user_data));
	bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(dialog), 4);
}

static GListStore*
store_for_grid_view(GtkGridView* grid)
{
    GtkNoSelection* no_selection_model = GTK_NO_SELECTION(gtk_grid_view_get_model(grid));
    GListModel* list_model = G_LIST_MODEL(gtk_no_selection_get_model(no_selection_model));
    GListStore* list_store = G_LIST_STORE(list_model);
    return list_store;
}

static void
foreach_item_in_store(GListStore* store, std::function<bool(GObject*)> cb)
{    
    guint n_items = g_list_model_get_n_items(G_LIST_MODEL(store));
    for (guint item_position = 0; item_position < n_items; item_position++) {
        orion::gtk::GObjectPtr ref((GObject*)g_list_model_get_item(G_LIST_MODEL(store), item_position));
        bool modified = cb(ref.get());
        if(modified) {
            g_list_store_insert(store, item_position, ref.get());
            g_list_store_remove(store, item_position + 1);
        }
    }
}

static void
update_extension(GListStore* store, WebExtensions::ExtensionInfo extension, ExtensionInstallState new_state)
{
    orion::ExtensionSku extension_sku = global_core_api->internal_store().get_extension_sku(extension);
    foreach_item_in_store(store, [extension_sku,new_state](GObject* ref){
        PopularExtensionItem* item = POPULAR_EXTENSION_ITEM(ref);
        bool modified = false;
        orion::ExtensionSku item_sku = global_core_api->internal_store().get_extension_sku(popular_extension_item_get_extension(item));
        if(extension_sku == item_sku) {
            modified = true;
            popular_extension_item_set_install_state(item, new_state);
        }
        return modified;
    });
}

static void
update_extension(PopularExtensionsDialog* self, WebExtensions::ExtensionInfo extension, ExtensionInstallState new_state)
{
    update_extension(store_for_grid_view(self->debug_grid_view), extension, new_state);
    update_extension(store_for_grid_view(self->password_managers_grid_view), extension, new_state);
    update_extension(store_for_grid_view(self->ads_and_tracking_grid_view), extension, new_state);
    update_extension(store_for_grid_view(self->vpns_grid_view), extension, new_state);
    update_extension(store_for_grid_view(self->browse_better_grid_view), extension, new_state);
}

static void
update_store_after_install(GListStore* store, const std::vector<WebExtensions::ExtensionInfo>& extension_list, const std::set<orion::ExtensionSku>& installed_skus)
{
    for(const auto& extension : extension_list) {
        orion::ExtensionSku sku = global_core_api->internal_store().get_extension_sku(extension);
        ExtensionInstallState new_state = ExtensionInstallState::None;
        if(extension.installURL) {
            new_state = ExtensionInstallState::Installable;
        }
        if(installed_skus.contains(sku)) {
            new_state = ExtensionInstallState::Installed;
        }
        update_extension(store, extension, new_state);
    }
}

static detached_task
on_extensions_change(PopularExtensionsDialog* dialog,
    const std::vector<orion::ExtensionSku>& installed_set)
{
    std::set<orion::ExtensionSku> installed_skus;
    for(auto& sku : installed_set) {
        installed_skus.insert(sku);
    }

    orion::gtk::WeakRefWrapper weak_ref(dialog);
    WebExtensions::ExtensionList extension_list = co_await async(get_extension_list);
    auto ref = weak_ref.lock();
    if(!ref) {
        co_return;
    }
    PopularExtensionsDialog* self = POPULAR_EXTENSIONS_DIALOG(ref.get());

    update_store_after_install(store_for_grid_view(self->debug_grid_view), extension_list.debug.value_or(std::vector<WebExtensions::ExtensionInfo>()), installed_skus);
    update_store_after_install(store_for_grid_view(self->password_managers_grid_view), extension_list.passwordManagers.value_or(std::vector<WebExtensions::ExtensionInfo>()), installed_skus);
    update_store_after_install(store_for_grid_view(self->ads_and_tracking_grid_view), extension_list.adBlockers.value_or(std::vector<WebExtensions::ExtensionInfo>()), installed_skus);
    update_store_after_install(store_for_grid_view(self->vpns_grid_view), extension_list.vpns.value_or(std::vector<WebExtensions::ExtensionInfo>()), installed_skus);
    update_store_after_install(store_for_grid_view(self->browse_better_grid_view), extension_list.browserBetter.value_or(std::vector<WebExtensions::ExtensionInfo>()), installed_skus);
}

static detached_task
update_installable_state(PopularExtensionsDialog* dialog);

static void
on_extensions_change(PopularExtensionsDialog* dialog, const orion::ExtensionsChange& extensions_change)
{
    update_installable_state(dialog);
}

static void
on_all_installed_extension_skus(PopularExtensionsDialog* dialog, orion::GetInstalledExtensionsReply reply)
{
    std::vector<orion::ExtensionSku> installed_set;
    for(const auto& extension : reply.extensions) {
        installed_set.push_back(extension.sku);
    }
    on_extensions_change(dialog, installed_set);
}

static detached_task
on_all_installed_extension_ids(PopularExtensionsDialog* dialog, orion::GetAllInstalledExtensionIdsReply all_installed_reply)
{
    orion::gtk::WeakRefWrapper weak_ref(dialog);
    orion::GetInstalledExtensionsQuery query;
    query.ids = all_installed_reply.ids;
    orion::GetInstalledExtensionsReply reply = AWAIT(get_installed_extensions, query);
    auto ref = weak_ref.lock();
    if(!ref) {
        co_return;
    }
    on_all_installed_extension_skus(POPULAR_EXTENSIONS_DIALOG(ref.get()), reply);
}

static void
on_web_extension_installed(const WebExtensions::ExtensionInfo& extension, orion::InstallExtensionReply reply)
{
    VERIFY_LOG(reply.err);
    orion::ExtensionSku sku = global_core_api->internal_store().get_extension_sku(extension);
    orion::log_debug("Installed '%s'", sku.value.c_str());
}

static detached_task
on_add_web_extension_dialog_closed(AddWebExtensionDialog *dialog, gpointer user_data)
{
    int response = add_web_extension_dialog_response(dialog);
    WebExtensions::ExtensionInfo extension = add_web_extension_dialog_extension(dialog);
    orion::ExtensionSku sku = global_core_api->internal_store().get_extension_sku(extension);
    PopularExtensionsDialog* self = POPULAR_EXTENSIONS_DIALOG(user_data);
    if(response != GTK_RESPONSE_OK) {
        orion::log_debug("Install cancelled '%s'", sku.value.c_str());
        update_extension(self, extension, ExtensionInstallState::Installable);
        co_return;
    }
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(dialog)));
    orion::InstallExtensionQuery query{};
    query.profile_id = main_window_get_current_profile_id(win);
    query.sku = sku;
    query.type = orion::ExtensionType::firefox;
    orion::log_debug("Installing '%s' ...", sku.value.c_str());
    std::shared_ptr<WebExtensionInstallSource> source = add_web_extension_dialog_source(dialog);
    orion::InstallExtensionReply install_reply =
        co_await async(invoke(source, &WebExtensionInstallSource::install), main_window_get_current_profile_id(win));
    on_web_extension_installed(extension, install_reply);
}

static void
on_extension_fetched(PopularExtensionsDialog* self, std::shared_ptr<WebExtensions::WebExtensionInstallSource> source, const WebExtensions::ExtensionInfo& extension)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    orion::ExtensionSku sku = global_core_api->internal_store().get_extension_sku(extension);
    if(!source) {
        orion::log_debug("Fetch cancelled '%s'", sku.value.c_str());
        update_extension(self, extension, ExtensionInstallState::Installable);
        return;
    }
    orion::log_debug("Fetched '%s'", sku.value.c_str());
    AdwDialog* dialog = add_web_extension_dialog_new(extension, source);
    adw_dialog_present(dialog, GTK_WIDGET(win));
    g_signal_connect(dialog, "closed", G_CALLBACK(on_add_web_extension_dialog_closed), self);
}

static detached_task
update_installable_state(PopularExtensionsDialog* dialog)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(dialog)));
    orion::gtk::WeakRefWrapper weak_ref(dialog);
    orion::GetInstalledExtensionIdsQuery query;
    query.profile_id = main_window_get_current_profile_id(win);
    orion::GetAllInstalledExtensionIdsReply reply = AWAIT(get_installed_extension_ids, query);
    auto ref = weak_ref.lock();
    if(!ref) {
        co_return;
    }
    on_all_installed_extension_ids(POPULAR_EXTENSIONS_DIALOG(ref.get()), reply);
}

static detached_task
on_action_clicked(GtkButton* button, gpointer user_data)
{
    GtkListItem* list_item = GTK_LIST_ITEM(user_data);
    PopularExtensionItem* item = POPULAR_EXTENSION_ITEM(gtk_list_item_get_item(list_item));
    switch(popular_extension_item_get_install_state(item)) {
        case ExtensionInstallState::None:
        case ExtensionInstallState::Installed:
            co_return;
        case ExtensionInstallState::Fetching:
            // TODO: cancel download/install
            co_return;
        case ExtensionInstallState::Installable:
            break;
    }
    PopularExtensionsDialog* self = (PopularExtensionsDialog*)button;
    while(self && !POPULAR_EXTENSIONS_IS_DIALOG(self)) {
        self = (PopularExtensionsDialog*)gtk_widget_get_parent(GTK_WIDGET(self));
    }
    if(!self) {
        co_return;
    }
    WebExtensions::ExtensionInfo extension = popular_extension_item_get_extension(item);
    orion::ExtensionSku sku = global_core_api->internal_store().get_extension_sku(extension);
    update_extension(self, extension, ExtensionInstallState::Fetching);
    orion::log_debug("Fetching '%s' ...", sku.value.c_str());
    orion::gtk::WeakRefWrapper weak_ref(self);
    std::shared_ptr<WebExtensions::WebExtensionInstallSource> source = co_await async(fetch_extension, extension);
    auto ref = weak_ref.lock();
    if(!ref) {
        co_return;
    }
    self = POPULAR_EXTENSIONS_DIALOG(ref.get());
    on_extension_fetched(self, std::move(source), extension);
}

static void
popular_extensions_dialog_attached(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(popular_extensions_dialog_parent_class)->root(widget);

    PopularExtensionsDialog* dialog = POPULAR_EXTENSIONS_DIALOG(widget);
    update_installable_state(dialog);
}

static void
popular_extensions_dialog_init(PopularExtensionsDialog* win)
{
	g_type_ensure(APP_TYPE_URL_IMAGE_WIDGET);

	gtk_widget_init_template(GTK_WIDGET(win));

    if(!global_core_api->debug_mode()) {
        gtk_widget_set_visible(GTK_WIDGET(win->debug_sidebar_row), false);
        gtk_widget_set_visible(win->debug_section, false);
    }

    std::construct_at(&win->extensions_change_observer_id);

    orion::gtk::WeakRefWrapper weak_ref(win);
    win->extensions_change_observer_id = global_core_api->extensions_change().add_observer(
        [weak_ref](const orion::ExtensionsChange& extensions_change){
            auto ref = weak_ref.lock();
            if(!ref) {
                return;
            }
            PopularExtensionsDialog* dialog = POPULAR_EXTENSIONS_DIALOG(ref.get());
            on_extensions_change(dialog, extensions_change);
        });

    popular_extensions_dialog_init_from_internal_store(win);
	//g_signal_connect(win->sidebar_list_box, "row-selected", G_CALLBACK(orion::gtk::bookmarks_dialog::change_tab), win->contentStack);
}

static char*
get_extension_name(GtkListItem* listitem)
{
    PopularExtensionItem* item = POPULAR_EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    if(!item) {
        return nullptr;
    }
    return g_strdup(popular_extension_item_get_extension(item).displayName.value_or("").c_str());
}

static char*
get_extension_description(GtkListItem* listitem)
{
    PopularExtensionItem* item = POPULAR_EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    if(!item) {
        return nullptr;
    }
    return g_strdup(popular_extension_item_get_extension(item).info.value_or("").c_str());
}

static char*
get_extension_icon_url(GtkListItem* listitem)
{
    PopularExtensionItem* item = POPULAR_EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    if(!item) {
        return nullptr;
    }
    return g_strdup(popular_extension_item_get_extension(item).iconURL.value_or("").c_str());
}

static char*
get_extension_action_string(GtkListItem* listitem)
{
    PopularExtensionItem* item = POPULAR_EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    if(!item) {
        return nullptr;
    }
    switch(popular_extension_item_get_install_state(item)) {
        case ExtensionInstallState::None:
        case ExtensionInstallState::Installable:
            return g_strdup(_("Get"));
        case ExtensionInstallState::Fetching:
            return g_strdup(_("Cancel"));
        case ExtensionInstallState::Installed:
            return g_strdup(_("Installed"));
    }
    return nullptr;
}

static gboolean
get_extension_action_sensitive(GtkListItem* listitem)
{
    PopularExtensionItem* item = POPULAR_EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    if(!item) {
        return false;
    }
    switch(popular_extension_item_get_install_state(item)) {
        case ExtensionInstallState::None:
        case ExtensionInstallState::Installed:
            return false;
        case ExtensionInstallState::Installable:
        case ExtensionInstallState::Fetching:
            return true;
            return true;
    }
    return false;
}

static char*
get_extension_type_string(GtkListItem* listitem)
{
    PopularExtensionItem* item = POPULAR_EXTENSION_ITEM(gtk_list_item_get_item(listitem));
    if(!item) {
        return nullptr;
    }
    std::string extension_type = popular_extension_item_get_extension(item).webExtType.value_or("not_set");
    if(extension_type == "not_set") {
        extension_type = _("unknown");
    }
    if(extension_type == "chrome") {
        extension_type = "Chrome";
    }
    if(extension_type == "firefox") {
        extension_type = "Firefox";
    }

    std::string version = popular_extension_item_get_extension(item).version.value_or("");
    if(version.length() > 0) {
        extension_type += " - " + version;
    }
    return g_strdup(extension_type.c_str());
}

static void
popular_extensions_dialog_finalize(GObject* gobject)
{
    PopularExtensionsDialog* win = POPULAR_EXTENSIONS_DIALOG(gobject);
    global_core_api->extensions_change().remove_observer(win->extensions_change_observer_id);
    std::destroy_at(&win->extensions_change_observer_id);
    G_OBJECT_CLASS(popular_extensions_dialog_parent_class)->finalize(gobject);
}

static gboolean
on_store_activate_link(GtkLinkButton* button, gpointer user_data)
{
    auto parent_window = orion::gtk::get_parent_window(GTK_WIDGET(button));
    if (parent_window) {
        //get uri from button
        const char* uri = gtk_link_button_get_uri(button);
        GVariant* uri_variant = g_variant_new_string(uri);

        g_action_group_activate_action(
            G_ACTION_GROUP(parent_window),
            "open-uri-in-new-tab",
            uri_variant);

        if (user_data && ADW_IS_DIALOG(user_data)) {
            adw_dialog_close(ADW_DIALOG(user_data));
        } else {
            orion::log_warning("on_store_activate_link: user_data not a valid AdwDialog!");
        }
    }
    return TRUE;
}

static void on_vadjustment_changed(GtkAdjustment* vadjustment, PopularExtensionsDialog* self);

static std::vector<GtkWidget*>
get_visible_category_scroll_targets(PopularExtensionsDialog* self)
{
    std::vector<GtkWidget*> targets;
    if(gtk_widget_get_visible(self->debug_section)) {
        targets.push_back(self->debug_section);
    }
    targets.push_back(GTK_WIDGET(self->password_managers_label));
    targets.push_back(GTK_WIDGET(self->ads_and_tracking_label));
    targets.push_back(GTK_WIDGET(self->vpns_label));
    targets.push_back(GTK_WIDGET(self->browse_better_label));
    return targets;
}

static void
on_sidebar_row_activated(GtkListBox* box, GtkListBoxRow* row, PopularExtensionsDialog* self)
{
    int i = gtk_list_box_row_get_index(row);

    //g_signal_handlers_block_by_func(self->content_vadjustment, (gpointer)on_vadjustment_changed, self);
    switch(i)
    {
        case 0:
            orion::gtk::widget_scroll_to(self->debug_section);
            break;
        case 1:
            orion::gtk::widget_scroll_to(GTK_WIDGET(self->password_managers_label));
            break;
        case 2:
            orion::gtk::widget_scroll_to(GTK_WIDGET(self->ads_and_tracking_label));
            break;
        case 3:
            orion::gtk::widget_scroll_to(GTK_WIDGET(self->vpns_label));
            break;
        case 4:
            orion::gtk::widget_scroll_to(GTK_WIDGET(self->browse_better_label));
            break;
    }
    //g_signal_handlers_unblock_by_func(self->content_vadjustment, (gpointer)on_vadjustment_changed, self);
}

//expects input to be ordered by y position

static void
after_frame_clock_paint(GdkFrameClock* clock, orion::gtk::WeakRefWrapper* self_weak)
{
    defer([clock, self_weak]{ g_signal_handlers_disconnect_by_func(clock, (gpointer)after_frame_clock_paint, self_weak); });
    auto self_strong  = self_weak->lock();
    if(!self_strong) return;
    PopularExtensionsDialog* self = POPULAR_EXTENSIONS_DIALOG(self_strong.get());

    std::vector<GtkWidget*> targets = get_visible_category_scroll_targets(self);
    int row_index = (int)orion::gtk::get_scrolled_to_widget_index(targets, 0.25, 0.25);

    // The hidden Debug row still occupies index 0 in the list box.
    if(!gtk_widget_get_visible(GTK_WIDGET(self->debug_sidebar_row))) {
        row_index++;
    }

    GtkListBoxRow* row = gtk_list_box_get_row_at_index(self->sidebar_list_box, row_index);
    if(row) gtk_list_box_select_row(self->sidebar_list_box, row);
}

static void
on_vadjustment_changed(GtkAdjustment* vadjustment, PopularExtensionsDialog* self)
{
    auto destroy_notify = [](gpointer data, GClosure* closure){
        delete static_cast<orion::gtk::WeakRefWrapper*>(data);
    };

    auto* self_weak = new orion::gtk::WeakRefWrapper(self);

    GdkFrameClock* frame_clock = gtk_widget_get_frame_clock(GTK_WIDGET(self));
    g_signal_connect_data(frame_clock, "after-paint", G_CALLBACK(after_frame_clock_paint), self_weak, destroy_notify, G_CONNECT_DEFAULT);
}

static void
on_vadjustment_value_changed(GtkAdjustment* vadjustment, PopularExtensionsDialog* self)
{
    on_vadjustment_changed(vadjustment, self);
}

static void
popular_extensions_dialog_class_init(PopularExtensionsDialogClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = popular_extensions_dialog_finalize;
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->root = popular_extensions_dialog_attached;
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/popular_extensions_dialog.ui");

	gtk_widget_class_bind_template_callback(widget_class, on_manage_extensions_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_action_clicked);
    gtk_widget_class_bind_template_callback(widget_class, get_extension_name);
    gtk_widget_class_bind_template_callback(widget_class, get_extension_description);
    gtk_widget_class_bind_template_callback(widget_class, get_extension_type_string);
    gtk_widget_class_bind_template_callback(widget_class, get_extension_icon_url);
    gtk_widget_class_bind_template_callback(widget_class, get_extension_action_string);
    gtk_widget_class_bind_template_callback(widget_class, get_extension_action_sensitive);
    gtk_widget_class_bind_template_callback(widget_class, on_store_activate_link);
    gtk_widget_class_bind_template_callback(widget_class, on_sidebar_row_activated);
    gtk_widget_class_bind_template_callback(widget_class, on_vadjustment_changed);
    gtk_widget_class_bind_template_callback(widget_class, on_vadjustment_value_changed);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, sidebar_list_box);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, content_vadjustment);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, password_managers_label);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, debug_sidebar_row);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, debug_section);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, debug_grid_view);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, password_managers_grid_view);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, ads_and_tracking_label);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, ads_and_tracking_grid_view);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, vpns_label);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, vpns_grid_view);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, browse_better_label);
    gtk_widget_class_bind_template_child(widget_class, PopularExtensionsDialog, browse_better_grid_view);
}
