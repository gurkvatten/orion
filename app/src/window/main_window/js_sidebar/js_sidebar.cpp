#define ENABLE_VERBOSE_LOGS 0

#include <string>
#include <atomic>

#include "js_sidebar.h"

#include "window/main_window/main_window.h"

namespace { // always wrap CppWrapper struct into anonymous namespace to avoid nasty ODR violations

struct CppWrapper {
};

}

struct _JsSidebar {
    GtkBox parent;

    GtkLabel* title_label;

    GtkTextView* text_view;

    int32_t epoch;

    CppWrapper cpp_wrapper;
};

G_DEFINE_TYPE(JsSidebar, js_sidebar, GTK_TYPE_BOX)


static void
js_sidebar_finalize(GObject* gobject)
{
    JsSidebar* self = APP_JS_SIDEBAR(gobject);
    G_OBJECT_CLASS(js_sidebar_parent_class)->finalize(gobject);

    std::destroy_at(&self->cpp_wrapper);
}

static void
js_sidebar_init(JsSidebar* self)
{
    std::construct_at(&self->cpp_wrapper);
    gtk_widget_init_template(GTK_WIDGET(self));
}

int32_t
js_sidebar_start_operation(JsSidebar* self, const char* title)
{
    gtk_label_set_label(self->title_label, title);


    static std::atomic<int32_t> global_programmable_button_epoch{1}; //has to be different from default value inside the js sidebar struct

    self->epoch = global_programmable_button_epoch.fetch_add(1, std::memory_order_relaxed);
    
    return self->epoch;
}

void
js_sidebar_clear(JsSidebar* self, int32_t epoch)
{
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->text_view);
    
    const char* empty = "";
    gtk_text_buffer_set_text(buffer, empty, 0);
}

void
js_sidebar_set_content(JsSidebar* self, int32_t epoch, std::string_view content)
{
    if(g_utf8_validate(content.data(), content.length(), NULL))
    {
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->text_view);

        gtk_text_buffer_set_text(buffer, content.data(), content.length());
    }
}

void
js_sidebar_add_content(JsSidebar* self, int32_t epoch, std::string_view content)
{
    if(g_utf8_validate(content.data(), content.length(), NULL))
    {
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->text_view);

        GtkTextIter end_iter = {};

        gtk_text_buffer_get_end_iter(buffer, &end_iter);

        gtk_text_buffer_insert(buffer, &end_iter, content.data(), content.length());
    }
}

char*
js_sidebar_get_content(JsSidebar* self)
{
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->text_view);
    GtkTextIter start = {}, end = {};
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
}

bool js_sidebar_match_epoch(JsSidebar* self, int32_t epoch)
{
    return self->epoch == epoch;
}


static void
on_close_clicked(GtkButton* button, JsSidebar* self)
{
    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(self));
    if(!MAIN_IS_WINDOW(root)) return;
    MainWindow* win = MAIN_WINDOW(root);

    AdwOverlaySplitView* view = main_window_get_js_sidebar_split_view(win);
    adw_overlay_split_view_set_show_sidebar(view, false);
}



static void
js_sidebar_class_init(JsSidebarClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = js_sidebar_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/main_window/js_sidebar/js_sidebar.ui");
    gtk_widget_class_bind_template_child(widget_class, JsSidebar, title_label);
    gtk_widget_class_bind_template_child(widget_class, JsSidebar, text_view);
    gtk_widget_class_bind_template_callback(widget_class, on_close_clicked);
}
