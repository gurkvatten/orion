#define ENABLE_VERBOSE_LOGS 0

#include "bookmarks_bar.h"
#include "window/bookmarks_dialog.h"
#include "window/columnview/bookmarks_bar_listview.h"
#include "window/main_window/main_window.h"
#include "util/dialog_util.h"

#include <common/src/log.h>
#include <core/settings.h>
#include <common/src/defer.h>

//TODO: implement selecting correct folder when opening bookmarks dialog through folder edit context menu item
//TODO: implement correct dnd highlighting

namespace //force internal linkage to avoid potential ODR violations
{
    struct BookmarksBarCppWrapper
    {
        orion::ObserverId style_setting_observer_id;
        orion::ObserverId bookmarks_change_observer_id;
        orion::ObserverId folders_change_observer_id;
        std::vector<orion::BookmarkDto> bookmarks;
        std::vector<orion::BookmarkFolderDto> folders;
        orion::BookmarkFolderId bookmars_bar_folder_id;
    };
}

struct _BookmarksBar
{
    AdwBin parent_type;

    GtkButton* bookmarks_bar_bookmarks_button;
    GtkListView* bookmarks_button_list_view;

    GtkPopoverMenu* folder_popover_menu;
    GtkPopoverMenu* bookmarks_popover_menu;
    GtkPopoverMenu* list_view_popover_menu;
    GtkScrolledWindow* scrolled_window;
    gboolean is_backend_data_fetched; //used just for the first time the bookmarks bar is prepared

    // enum classes are trivial types and have standard layout, no need
    // to use std::construct_at and std::destroy_at
    orion::settings::BookmarksBarStyle style;

    BookmarksBarCppWrapper cpp_wrapper;
};

G_DEFINE_TYPE(BookmarksBar, bookmarks_bar, ADW_TYPE_BIN)

void
bookmarks_bar_set_bookmarks_change_observer_id(BookmarksBar* self, orion::ObserverId id)
{
    self->cpp_wrapper.bookmarks_change_observer_id = id;
}

void
bookmarks_bar_set_folders_change_observer_id(BookmarksBar* self, orion::ObserverId id)
{
    self->cpp_wrapper.folders_change_observer_id = id;
}

orion::ObserverId
bookmarks_bar_get_bookmarks_change_observer_id(BookmarksBar* self)
{
    return self->cpp_wrapper.bookmarks_change_observer_id;
}

orion::ObserverId
bookmarks_bar_get_folders_change_observer_id(BookmarksBar* self)
{
    return self->cpp_wrapper.folders_change_observer_id;
}

void
bookmarks_bar_set_bookmarks(BookmarksBar* self, std::vector<orion::BookmarkDto> bookmarks)
{
    self->cpp_wrapper.bookmarks = std::move(bookmarks);
}

void
bookmarks_bar_set_folders(BookmarksBar* self, std::vector<orion::BookmarkFolderDto> folders)
{
    self->cpp_wrapper.folders = std::move(folders);
}

std::vector<orion::BookmarkDto>*
bookmarks_bar_get_bookmarks(BookmarksBar* self)
{
    return &self->cpp_wrapper.bookmarks;
}

std::vector<orion::BookmarkFolderDto>*
bookmarks_bar_get_folders(BookmarksBar* self)
{
    return &self->cpp_wrapper.folders;
}

void
bookmarks_bar_set_bookmars_bar_folder_id(BookmarksBar* self, orion::BookmarkFolderId id)
{
    self->cpp_wrapper.bookmars_bar_folder_id = id;
}

orion::BookmarkFolderId
bookmarks_bar_get_bookmars_bar_folder_id(BookmarksBar* self)
{
    return self->cpp_wrapper.bookmars_bar_folder_id;
}

void
bookmarks_bar_set_mini_toolbar_enabled(BookmarksBar* self, bool enabled)
{
    auto add_or_remove_func = enabled
        ? gtk_widget_add_css_class
        : gtk_widget_remove_css_class;

    add_or_remove_func(GTK_WIDGET(self->bookmarks_bar_bookmarks_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->bookmarks_button_list_view), "mini-toolbar-listview");
}

GtkPopoverMenu*
bookmarks_bar_get_folder_popover_menu(BookmarksBar* self)
{
    return APP_BOOKMARKS_BAR(self)->folder_popover_menu;
}

GtkPopoverMenu*
bookmarks_bar_get_bookmark_popover_menu(BookmarksBar* self)
{
    return APP_BOOKMARKS_BAR(self)->bookmarks_popover_menu;
}

static void
on_bookmarks_bar_bookmarks_button_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = bookmarks_dialog_new();
    bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(dialog), 1);
    adw_dialog_present(dialog ,GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(button))));
}

void
bookmarks_bar_set_style(BookmarksBar* self, orion::settings::BookmarksBarStyle style)
{
    self->style = style;
}

orion::settings::BookmarksBarStyle
bookmarks_bar_get_style(BookmarksBar* self)
{
    return self->style;
}

static void
bookmarks_bar_dispose(GObject* gobject)
{
    orion::log_verbose("bookmarks_bar_dispose");
    BookmarksBar* self = APP_BOOKMARKS_BAR(gobject);
    bookmarks_bar_list_view_dispose(self->bookmarks_button_list_view, self);

    //note: the call below sets NULL to self->bookmarks_button_list_view and possibly other fields
    //that are bound as template children in class_init
    gtk_widget_dispose_template(GTK_WIDGET(gobject), APP_BOOKMARKS_BAR_TYPE);

    g_clear_pointer(&self->folder_popover_menu, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->bookmarks_popover_menu, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->list_view_popover_menu, orion::gtk::widget_unparent_wrapper);

    G_OBJECT_CLASS(bookmarks_bar_parent_class)->dispose(gobject);
}

static void
bookmarks_bar_finalize(GObject* gobject)
{
    orion::log_verbose("bookmarks_bar_finalize");
    BookmarksBar* self = APP_BOOKMARKS_BAR(gobject);
    bookmarks_bar_list_view_finalize(self->bookmarks_button_list_view, self);
    std::destroy_at(&self->cpp_wrapper);
    G_OBJECT_CLASS(bookmarks_bar_parent_class)->finalize(gobject);
}

static void
on_list_view_secondary_released(GtkGestureClick* gesture, int n_press, double x, double y, gpointer user_data)
{
    GtkPopover* popover = GTK_POPOVER(APP_BOOKMARKS_BAR(user_data)->list_view_popover_menu);
    GdkRectangle rec = {.x = (int)x, .y = (int)y, .width = 1, .height = 1};
    gtk_popover_set_pointing_to(popover, &rec);
    gtk_popover_popup(popover);
    GtkWidget* scrolled_window = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    main_window_set_last_right_clicked_widget(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))), gtk_widget_get_first_child(scrolled_window));
}

void
bookmarks_bar_refresh_style(BookmarksBar* bookmarks_bar)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(bookmarks_bar)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    auto weak_bar_ref = orion::gtk::WeakRefWrapper(bookmarks_bar);

    global_core_api->get_setting(profile_id, orion::settings::appearance::bookmarks_bar_style,
        [weak_bar_ref](orion::GetSettingReply<orion::settings::BookmarksBarStyle> reply){
            VERIFY_LOG(reply.err);
            auto bar_ref = weak_bar_ref.lock();
            if (!bar_ref) return;
            BookmarksBar* bookmarks_bar = APP_BOOKMARKS_BAR(bar_ref.get());
            bookmarks_bar_set_style(bookmarks_bar, reply.value);
            bookmarks_bar_list_view_refresh(bookmarks_bar->bookmarks_button_list_view);
        });
}

void
bookmarks_bar_map(BookmarksBar* self)
{
    orion::log_verbose("bookmarks_bar_map");
    auto weak_bar_ref = orion::gtk::WeakRefWrapper(self);
    self->cpp_wrapper.style_setting_observer_id = global_core_api->add_single_setting_change_observer(orion::settings::appearance::bookmarks_bar_style,
        [weak_bar_ref](orion::SingleSettingChange<orion::settings::BookmarksBarStyle> change){
            orion::log_verbose("bookmarks bar style setting change observed");
            auto bar_ref = weak_bar_ref.lock();
            if (!bar_ref) return;
            BookmarksBar* bookmarks_bar = APP_BOOKMARKS_BAR(bar_ref.get());
            bookmarks_bar_refresh_style(bookmarks_bar);
        });

    bookmarks_bar_refresh_style(self);
    bookmarks_bar_list_view_map(self->bookmarks_button_list_view, self);

    if(!self->is_backend_data_fetched)
    {
        //this cannot be called from init() or constructed() because profile_id is not known yet there
        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
        orion::ProfileId profile_id = main_window_get_current_profile_id(win);
        bookmarks_bar_list_view_fetch_backend_data(self->bookmarks_button_list_view, self, profile_id);
        self->is_backend_data_fetched = TRUE;
    }
}

void
bookmarks_bar_unmap(BookmarksBar* self)
{
    orion::log_verbose("bookmarks_bar_unmap");
    global_core_api->single_setting_change().remove_observer(self->cpp_wrapper.style_setting_observer_id);
    self->cpp_wrapper.style_setting_observer_id = orion::ObserverId{};
    bookmarks_bar_list_view_unmap(self->bookmarks_button_list_view, self);
}

static void
bookmarks_bar_constructed(GObject* object)
{
    // always chain up first
    G_OBJECT_CLASS(bookmarks_bar_parent_class)->constructed(object);
    BookmarksBar* self = APP_BOOKMARKS_BAR(object);
    bookmarks_bar_list_view_constructed(self->bookmarks_button_list_view, self);
}

static void
bookmarks_bar_init(BookmarksBar* self)
{
    std::construct_at(&self->cpp_wrapper);
    gtk_widget_init_template(GTK_WIDGET(self));

    {
        GtkBuilder* builder = gtk_builder_new_from_resource(GRESOURCE_PREFIX "/src/window/main_window/sidebar/context_menus.ui");
        defer([builder]{ g_object_unref(builder); }); //auto unref at end of current scope

        orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->folder_popover_menu, "bookmarks_bar_folder_context_menu");
        orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->bookmarks_popover_menu, "bookmarks_bar_bookmark_context_menu");
        orion::gtk::setup_context_menu(builder, GTK_WIDGET(self->scrolled_window), &self->list_view_popover_menu, "bookmarks_bar_list_view_context_menu");
    }

    bookmarks_bar_list_view_init(self->bookmarks_button_list_view, self);

    g_signal_connect(self, "map", G_CALLBACK(bookmarks_bar_map), NULL);
    g_signal_connect(self, "unmap", G_CALLBACK(bookmarks_bar_unmap), NULL);
    
    orion::gtk::register_right_click_with_user_data(GTK_WIDGET(self->scrolled_window), GTK_WIDGET(self->scrolled_window), on_list_view_secondary_released, self);
}

static void
bookmarks_bar_class_init(BookmarksBarClass* klass)
{
    G_OBJECT_CLASS(klass)->dispose = bookmarks_bar_dispose;
    G_OBJECT_CLASS(klass)->finalize = bookmarks_bar_finalize;
    G_OBJECT_CLASS(klass)->constructed = bookmarks_bar_constructed;
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/main_window/header/bookmarks_bar.ui");

    gtk_widget_class_bind_template_callback(widget_class, on_bookmarks_bar_bookmarks_button_clicked);
    gtk_widget_class_bind_template_child(widget_class, BookmarksBar, bookmarks_button_list_view);
    gtk_widget_class_bind_template_child(widget_class, BookmarksBar, scrolled_window);
    gtk_widget_class_bind_template_child(widget_class, BookmarksBar, bookmarks_bar_bookmarks_button);
}
