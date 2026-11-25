#include "text_util.h"
#include <gtk/gtk.h>
#include <libintl.h>

#include "window_observer_util.h"

#include <vector>
#include <functional>
#include <cstring>
#include <algorithm>

#include <common/src/log.h>
#include <common/src/defer.h>
#include "window/main_window/main_window.h"

namespace orion {
    
//caller takes ownership of the data and is responsible for freeing

void
bind_callback_to_window_id(orion::WindowId id, std::function<void(const orion::WindowDto&)> cb, std::function<void(orion::ObserverId)> observer_creation_cb)
{
    global_core_api->get_windows({id}, [cb, id, observer_creation_cb](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);
        if(reply.windows.size())
        {
            cb(reply.windows.at(0));
        }
        
        observer_creation_cb(global_core_api->windows_change().add_observer([cb, id](const orion::WindowsChange& change){
                for(auto& window : change.updated)
                {
                    if(window.id == id)
                    {
                        cb(window);
                    }
                }
            }));
    });
}


} // namespace orion

static gchar*
get_n_tabs_string(int n)
{
    return g_strdup_printf(ngettext("%d Tab", "%d Tabs", n), n);
}

static void
label_from_window_cb(GObject* object, const orion::WindowDto& window)
{
    if(window.is_tab_group && !window.is_private)
    {
        gchar* n_tabs_string = (get_n_tabs_string(window.num_tabs));
        defer([n_tabs_string]{ g_free(n_tabs_string); });
        g_object_set(object, "label", n_tabs_string, NULL);
    }
    else
    {
        // Saved Tab Group or Private Tab Group
        g_object_set(object, "label", window.name.c_str(), NULL);
    }
}

// closure_context_object is HeaderBar or Sidebar at the moment, object is GtkLabel
char*
bind_object_with_label_to_window(GObject* closure_context_object, GObject* object, int64_t id)
{
    std::string label_text;
    if (GTK_IS_LABEL(object)) {
        label_text = gtk_label_get_text(GTK_LABEL(object));
    }

    orion::log_verbose(
        "bind_object_with_label_to_window: closure_context_object=%p type=%s object=%p type=%s id=%lld label_text=%s",
        closure_context_object, G_OBJECT_TYPE_NAME(closure_context_object), object, G_OBJECT_TYPE_NAME(object), id,
        label_text.c_str());

    g_signal_connect(object, "destroy", G_CALLBACK(+[](GObject* object, gpointer user_data){
        orion::log_verbose(
            "bind_object_with_label_to_window: destroy signal: object=%p type=%s",
            object, G_OBJECT_TYPE_NAME(object));
    }), NULL);

    // orion::ObserverId* observer_id_ptr = static_cast<orion::ObserverId*>(g_object_get_data(
    //     G_OBJECT(object), "window-observer-id"));

    // if (observer_id_ptr) {
    //     global_core_api->windows_change().remove_observer(*observer_id_ptr);
    //     delete(observer_id_ptr);
    // }
    g_object_set_data(G_OBJECT(object), "window-observer-id", NULL);

    orion::gtk::WeakRefWrapper object_weak(object);
    auto observer = [object_weak](const orion::WindowDto& window){
        auto object_strong = object_weak.lock();
        if(object_strong)
        {
            label_from_window_cb(G_OBJECT(object_strong.get()), window);
        }
    };

    auto observer_creation_cb = [object_weak](orion::ObserverId observer_id){
        auto object_strong = object_weak.lock();
        if(object_strong)
        {
            GObject* object = G_OBJECT(object_strong.get());
            orion::ObserverId* observer_id_ptr = new orion::ObserverId(observer_id);

            auto destroy_notify = [](gpointer data){
                orion::ObserverId* observer_id_ptr = static_cast<orion::ObserverId*>(data);
                orion::log_verbose(
                    "bind_object_with_label_to_window: destroy_notify: observer_id=%zu",
                    observer_id_ptr->value);
                
                global_core_api->windows_change().remove_observer(*observer_id_ptr);
                delete(observer_id_ptr);
            };
            
            g_object_set_data_full(object, "window-observer-id", observer_id_ptr, destroy_notify);
            
            orion::log_verbose(
                "bind_object_with_label_to_window: observer_creation_cb: object=%p type=%s observer_id=%zu",
                object, G_OBJECT_TYPE_NAME(object), observer_id.value);
        }
        else
        {
            orion::log_verbose(
                "bind_object_with_label_to_window: object destroyed, observer_id=%zu", observer_id.value);
            global_core_api->windows_change().remove_observer(observer_id);
        }
    };

    orion::bind_callback_to_window_id(orion::WindowId(id), observer, observer_creation_cb);

    return g_strdup("");
}

static void
image_from_window_cb(GtkImage* image, GtkLabel* label, GtkStack* stack, const orion::WindowDto& window)
{
    bool is_emoji = orion::text::text_is_emoji(window.icon_name.c_str(), pango_cairo_font_map_get_default());
    if(is_emoji)
    {
        gtk_label_set_text(label, window.icon_name.c_str());
        gtk_stack_set_visible_child(stack, GTK_WIDGET(label));
    }
    else
    {
        gtk_image_set_from_icon_name(image, window.icon_name.c_str());
        gtk_stack_set_visible_child(stack, GTK_WIDGET(image));
    }
}

gchar*
bind_window_icon(GObject* closure, GtkImage* image, GtkLabel* label, int64_t window_id, GtkStack* stack)
{
    g_object_set_data(G_OBJECT(stack), "window-observer-id", NULL);

    orion::gtk::WeakRefWrapper image_weak(image);
    orion::gtk::WeakRefWrapper label_weak(label);
    orion::gtk::WeakRefWrapper stack_weak(stack);
    auto observer = [image_weak, label_weak, stack_weak](const orion::WindowDto& window){
        auto image_strong = image_weak.lock();
        auto label_strong = label_weak.lock();
        auto stack_strong = stack_weak.lock();
        if(!(image_strong || label_strong || stack_strong)) return;
        {
            image_from_window_cb(GTK_IMAGE(image_strong.get()), GTK_LABEL(label_strong.get()), GTK_STACK(stack_strong.get()), window);
        }
    };

    auto observer_creation_cb = [image_weak, label_weak, stack_weak](orion::ObserverId observer_id){
        auto stack_strong = stack_weak.lock();
        if(stack_strong)
        {
            GObject* object = G_OBJECT(stack_strong.get());
            orion::ObserverId* observer_id_ptr = new orion::ObserverId(observer_id);

            auto destroy_notify = [](gpointer data){
                orion::ObserverId* observer_id_ptr = static_cast<orion::ObserverId*>(data);
                if (observer_id_ptr->value == 0) {
                    orion::log_warning("bind_window_icon: destroy_notify: invalid observer_id 0");
                }
                global_core_api->windows_change().remove_observer(*observer_id_ptr);
                delete(observer_id_ptr);
            };
            g_object_set_data_full(object, "window-observer-id", observer_id_ptr, destroy_notify);
        }
        else
        {
            if (observer_id.value == 0) {
                orion::log_warning("bind_window_icon: invalid observer_id 0");
            }
            global_core_api->windows_change().remove_observer(observer_id);
        }
    };

    orion::bind_callback_to_window_id(orion::WindowId(window_id), observer, observer_creation_cb);

    return NULL;
}
