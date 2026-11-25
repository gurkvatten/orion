#include <gtk/gtk.h>

#include "app.h"
#include "extension_popover.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

#include <webextensions/webview_factory_host.h>

#include <window/main_window/main_window.h>

struct _ExtensionPopover
{
	GtkPopover parent;
    AdwBin* content_bin;
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* content_webview;
#endif
    orion::ExtensionId id;
};

G_DEFINE_TYPE(ExtensionPopover, extension_popover, GTK_TYPE_POPOVER)

GtkPopover*
extension_popover_new(orion::ExtensionId id)
{
    ExtensionPopover* self = EXTENSION_POPOVER(g_object_new(EXTENSION_POPOVER_TYPE, NULL));
    self->id = id;
	return GTK_POPOVER(self);
}

static void
extension_popover_attached(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(extension_popover_parent_class)->root(widget);

    ExtensionPopover* self = EXTENSION_POPOVER(widget);
    orion::gtk::WeakRefWrapper weak_ref(self);
    WebExtensionWebViewFactory& factory = main_window_get_current_webview_factory(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(widget))));
    factory.web_view_for_extension_popup(self->id, [weak_ref](WebViewRef web_view){
        auto ref = weak_ref.lock();
        if(!ref || !web_view) {
            return;
        }
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        ExtensionPopover* self = EXTENSION_POPOVER(ref.get());
        
        WebKitWebView* content_webview = WEBKIT_WEB_VIEW(web_view.release());
        self->content_webview = content_webview;
        adw_bin_set_child(self->content_bin, GTK_WIDGET(self->content_webview));
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    });
}

static void
extension_popover_init(ExtensionPopover* self)
{
    std::construct_at(&self->id);
	gtk_widget_init_template(GTK_WIDGET(self));
}

static void
extension_popover_finalize(GObject* gobject)
{
    ExtensionPopover* self = EXTENSION_POPOVER(gobject);
    std::destroy_at(&self->id);
    G_OBJECT_CLASS(extension_popover_parent_class)->finalize(gobject);
}

static void
extension_popover_class_init(ExtensionPopoverClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = extension_popover_finalize;
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/extension_popover.ui");
    widget_class->root = extension_popover_attached;
    gtk_widget_class_bind_template_child(widget_class, ExtensionPopover, content_bin);
}
