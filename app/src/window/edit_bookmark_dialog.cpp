#include <gdk/gdk.h>

#include "../app.h"
#include "edit_bookmark_dialog.h"

//TODO: implement this

struct _EditBookmarkDialog
{
	AdwDialog parent;
    std::optional<orion::BookmarkId> id;
    GtkEntry* title_entry;
    GtkEntry* address_entry;
    GtkEntry* invocation_keyword_entry;
};

G_DEFINE_TYPE(EditBookmarkDialog, edit_bookmark_dialog, ADW_TYPE_DIALOG)

AdwDialog*
edit_bookmark_dialog_new()
{
	return ADW_DIALOG(g_object_new(EDIT_BOOKMARK_DIALOG_TYPE, NULL));
}

AdwDialog*
edit_bookmark_dialog_new_for_id(orion::BookmarkId id)
{
    EditBookmarkDialog* dialog = static_cast<EditBookmarkDialog*>(g_object_new(EDIT_BOOKMARK_DIALOG_TYPE, NULL));
    dialog->id = id;

    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(dialog);

    orion::GetBookmarksQuery query{
        .ids = {id},
    };
    global_core_api->get_bookmarks(query, [wrapped_ref](orion::GetBookmarksReply reply){
        VERIFY_LOG(reply.err);

        EditBookmarkDialog* dialog = EDIT_BOOKMARK_DIALOG(wrapped_ref->get());
        if(dialog)
        {
            orion::BookmarkDto bookmark = reply.bookmarks.at(0);
            gtk_editable_set_text(GTK_EDITABLE(dialog->title_entry), bookmark.name.c_str());
            gtk_editable_set_text(GTK_EDITABLE(dialog->address_entry), bookmark.address.c_str());
            gtk_editable_set_text(GTK_EDITABLE(dialog->invocation_keyword_entry), bookmark.invocation_keyword.c_str());
            g_object_unref(dialog);
        }
    });
    return ADW_DIALOG(dialog);
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_remove_clicked(GtkButton* button, gpointer user_data)
{
    EditBookmarkDialog* dialog = EDIT_BOOKMARK_DIALOG(user_data);
    if(dialog->id)
    {
        auto id = dialog->id.value();
        global_core_api->delete_bookmarks({id}, [](orion::Error err){VERIFY_LOG(err);});
    }
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_save_clicked(GtkButton* button, gpointer user_data)
{
    EditBookmarkDialog* dialog = EDIT_BOOKMARK_DIALOG(user_data);
    if(dialog->id)
    {
        orion::UpdateBookmarkQuery query;

        query.id = dialog->id.value();

        query.update_name = true;
        query.update_address = true;
        query.update_invocation_keyword = true;

        query.name = gtk_editable_get_text(GTK_EDITABLE(dialog->title_entry));
        query.address = gtk_editable_get_text(GTK_EDITABLE(dialog->address_entry));
        query.invocation_keyword = gtk_editable_get_text(GTK_EDITABLE(dialog->invocation_keyword_entry));

        global_core_api->update_bookmark(query, [](orion::Error err){VERIFY_LOG(err);});
    }
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
edit_bookmark_dialog_finalize(GObject* object)
{
    EditBookmarkDialog* self = EDIT_BOOKMARK_DIALOG(object);
    std::destroy_at(&self->id);
    G_OBJECT_CLASS(edit_bookmark_dialog_parent_class)->finalize(object);
}

static void
edit_bookmark_dialog_init(EditBookmarkDialog* win)
{
    std::construct_at(&win->id);
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
edit_bookmark_dialog_class_init(EditBookmarkDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    G_OBJECT_CLASS(klass)->finalize = edit_bookmark_dialog_finalize;
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/edit_bookmark_dialog.ui");

    gtk_widget_class_bind_template_child(widget_class, EditBookmarkDialog, title_entry);
    gtk_widget_class_bind_template_child(widget_class, EditBookmarkDialog, address_entry);
    gtk_widget_class_bind_template_child(widget_class, EditBookmarkDialog, invocation_keyword_entry);

    gtk_widget_class_bind_template_callback(widget_class, on_remove_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_save_clicked);
}
