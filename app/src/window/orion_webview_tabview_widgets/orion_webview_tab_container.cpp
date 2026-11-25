#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string>
#include <sstream>

#include <common/src/defer.h>

#include "app.h"

#include "data_item/back_forward_list.h"

#include "window/main_window/orion_window_iface.h"

#include "orion_webview_tab_container.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

#define BLUR_RADIUS 5.0

namespace {
    struct CppWrapper
    {
        orion::TabId tab_id;
        orion::TabId parent_id;
        std::vector<orion::gtk::GObjectPtr> property_bindings;
        orion::UnixTimeUtc64 last_active_time;
    };
}

struct _OrionWebviewTabContainer
{
	GtkWidget parent;

    GtkGraphicsOffload* graphics_offload;
    
    GWeakRef* previous_window_weak;

    GdkTexture* preview_texture;
    
    double seq_num;

    bool nested_tabs_expandable;
    bool group_expanded;
    int indent;

    CppWrapper cpp_data;
};

enum {
    PROP_0,
    PROP_ID,
    PROP_PARENT_ID,
    PROP_TAB_TITLE,
    PROP_ICON,
    PROP_IS_LOADING,
    PROP_SEQ_NUM,
    PROP_NESTED_TABS_EXPANDABLE,
    PROP_INDENT,
    PROP_URI,
    PROP_CAN_GO_BACK,
    PROP_CAN_GO_FORWARD,
    PROP_IS_READERABLE,
    PROP_READER_MODE_ENABLED,
    PROP_IS_PLAYING_AUDIO,
    PROP_IS_MUTED,
    PROP_TLS_STATUS,
    PROP_BACK_LIST,
    PROP_FORWARD_LIST,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS];

G_DEFINE_TYPE(OrionWebviewTabContainer, orion_webview_tab_container, GTK_TYPE_WIDGET)

OrionWebviewTabContent*
orion_webview_tab_container_get_content(OrionWebviewTabContainer* container)
{
    GtkWidget* child = gtk_graphics_offload_get_child(container->graphics_offload);
    if(child)
    {
        return ORION_WEBVIEW_TAB_CONTENT(child);
    }
    else
    {
        OrionWebviewTabContent* content = orion_app_get_shared_tab(ORION_APP(g_application_get_default()), container->cpp_data.tab_id);
        if (!content) {
            orion::log_verbose("orion_webview_tab_container_get_content: no content found for tab id %ld",
                container->cpp_data.tab_id.value);
        }
        //g_assert(content);
        return(content);
    }
}

OrionWebviewTabContainer*
orion_webview_tab_container_new(orion::TabId id, double seq_num)
{
    return static_cast<OrionWebviewTabContainer*>(g_object_new(ORION_WEBVIEW_TAB_CONTAINER_TYPE, "id", id.value, "seq-num", seq_num, NULL));
}


static void
orion_webview_tab_container_remove_child_content(OrionWebviewTabContainer* container, OrionWebviewTabContent* content)
{
    defer([go = container->graphics_offload]{ gtk_graphics_offload_set_child(go, NULL); });
    GtkNative *native = gtk_widget_get_native(GTK_WIDGET(container));
    if (!native) return;
    
    GskRenderer *renderer = gtk_native_get_renderer(native);
    if (!renderer) return;
    
    GtkSnapshot* snapshot = gtk_snapshot_new();
    gtk_widget_snapshot_child(GTK_WIDGET(container), GTK_WIDGET(container->graphics_offload), snapshot);
    GskRenderNode* node = gtk_snapshot_free_to_node(snapshot);

    if(node)
    {
        float width = (float)gtk_widget_get_width(GTK_WIDGET(container));
        float height = (float)gtk_widget_get_height(GTK_WIDGET(container));

        {
            graphene_matrix_t matrix;
            graphene_vec4_t offset;
            //calculates a matrix that is equal to c*identity matrix + (1-c)*grayscale matrix
            float c = 0.3f;
            //luminosity weights proportional to human perception
            float lr = 0.21f; float lg = 0.72f; float lb = 0.07f;
            float r = (1-c)*lr; float g = (1-c)*lg; float b = (1-c)*lb; 

            //the original color in the render node is treated as a vector (R, G, B, A)
            //is multiplied by a transposition of this matrix, producing a desaturated image
            //(for more information, look up gtk_snapshot_push_color_matrix)
            float matrix_data[] = {
                r+c,   r,   r, 0.0,
                g, g+c,   g, 0.0,
                b,   b, b+c, 0.0,
                0.0, 0.0, 0.0, 1.0
            };
            graphene_matrix_init_from_float (&matrix, matrix_data);
            graphene_vec4_init (&offset, 0, 0, 0, 0);
            
            GskRenderNode* tmp = gsk_color_matrix_node_new(node, &matrix, &offset);
            gsk_render_node_unref(node);
            node = tmp;
        }
        {
            float sx = ((float)width + 2 * BLUR_RADIUS) / width;
            float sy = ((float)height + 2 * BLUR_RADIUS) / height;
            graphene_point_t margin_offset = {.x = -BLUR_RADIUS, .y = -BLUR_RADIUS};

            //stretches the image out past the viewport to then cut off edges that get blurred with the usually black empty space background
            GskTransform* transform = gsk_transform_scale(gsk_transform_translate(NULL, &margin_offset), sx, sy);
            defer([transform]{ gsk_transform_unref(transform); });

            GskRenderNode* tmp = gsk_transform_node_new(node, transform);
            gsk_render_node_unref(node);
            node = tmp;
        }
        {
            GskRenderNode* tmp = gsk_blur_node_new(node, BLUR_RADIUS);
            gsk_render_node_unref(node);
            node = tmp;
        }


        g_clear_object(&container->preview_texture);

        graphene_rect_t viewport = {.origin {.x = 0, .y = 0},
            .size = {.width = width, .height = height}};
        container->preview_texture = gsk_renderer_render_texture(renderer, node, &viewport);

        gsk_render_node_unref(node);
    }
}

void
orion_webview_tab_container_set_child_content(OrionWebviewTabContainer* container, OrionWebviewTabContent* content)
{
    if(content)
    {
        bool already_is_child = false;
        if(GtkWidget* child = gtk_graphics_offload_get_child(container->graphics_offload))
        {
            if(static_cast<gpointer>(child) != content)
            {
                orion::log_debug("orion_webview_tab_container_set_child_content: container already has different child");
                orion::log_debug("container tab id: %ld", container->cpp_data.tab_id.value);
                bool is_content = ORION_IS_WEBVIEW_TAB_CONTENT(content);
                orion::log_debug("child is content: %d", is_content);
                if(is_content)
                {
                    orion::log_debug("content tab id: %ld", orion_webview_tab_content_get_id(ORION_WEBVIEW_TAB_CONTENT(child)).value);
                }
                gtk_widget_unparent(GTK_WIDGET(child));
            }
            else
            {
                already_is_child = true;
            }
        }
        if(!already_is_child)
        {
            auto& property_bindings = container->cpp_data.property_bindings;
            property_bindings.clear();
            g_object_ref(content);
            defer([content](){ g_object_unref(content); });
            GtkWidget* parent = gtk_widget_get_ancestor(GTK_WIDGET(content), ORION_WEBVIEW_TAB_CONTAINER_TYPE);
            if(parent)
            {
                //if this spot ever crashes, it is highly likely that content has an invalid parent
                //because a different container did not unparent correctly upon its destruction
                assert(ORION_IS_WEBVIEW_TAB_CONTAINER(parent));
                orion_webview_tab_container_remove_child_content(ORION_WEBVIEW_TAB_CONTAINER(parent), content);
            }
            gtk_graphics_offload_set_child(container->graphics_offload, GTK_WIDGET(content));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "id", container, "id", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "tab-title", container, "tab-title", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "icon", container, "icon", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "is-loading", container, "is-loading", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "uri", container, "uri", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "can-go-back", container, "can-go-back", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "can-go-forward", container, "can-go-forward", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "is-readerable", container, "is-readerable", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "reader-mode-enabled", container, "reader-mode-enabled", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "is-playing-audio", container, "is-playing-audio", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "is-muted", container, "is-muted", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "tls-status", container, "tls-status", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "back-list", container, "back-list", G_BINDING_SYNC_CREATE))));
            property_bindings.emplace_back(G_OBJECT(g_object_ref(g_object_bind_property(content, "forward-list", container, "forward-list", G_BINDING_SYNC_CREATE))));
        }
    }
    else
    {
        std::vector<GtkWidget*> children;
        for(GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(container->graphics_offload)); GTK_IS_WIDGET(child); child = gtk_widget_get_next_sibling(child))
        {
            children.push_back(child);
        }
        for(auto child : children)
        {
            gtk_widget_unparent(child);
        }
    }
}

static void
orion_webview_tab_container_init(OrionWebviewTabContainer* container)
{
    std::construct_at(&container->cpp_data);
    GWeakRef* ref = g_new0(GWeakRef, 1);
    g_weak_ref_init(ref, NULL);
    container->previous_window_weak = ref;


    GtkWidget* graphics_offload = gtk_graphics_offload_new(NULL);
    container->graphics_offload = GTK_GRAPHICS_OFFLOAD(graphics_offload);
    gtk_widget_set_parent(graphics_offload, GTK_WIDGET(container));

}

static void
orion_webview_tab_container_dispose(GObject* object)
{
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(object);
    g_clear_object(&container->preview_texture);
    if(container->previous_window_weak)
    {
        g_weak_ref_clear(container->previous_window_weak);
        g_clear_pointer(&container->previous_window_weak, g_free);
    }
    GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(container));
    while(GTK_IS_WIDGET(child))
    {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_widget_unparent(child);
        child = next;
    }
    G_OBJECT_CLASS(orion_webview_tab_container_parent_class)->dispose(object);
}

static void
orion_webview_tab_container_finalize(GObject* object)
{
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(object);
    std::destroy_at(&container->cpp_data);
    G_OBJECT_CLASS(orion_webview_tab_container_parent_class)->dispose(object);
}

static void
orion_webview_tab_container_snapshot(GtkWidget* widget, GtkSnapshot* snapshot)
{
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(widget);
    GtkWidget* content = gtk_graphics_offload_get_child(container->graphics_offload);
    if(content)
    {
        gtk_widget_snapshot_child(widget, GTK_WIDGET(container->graphics_offload), snapshot);
        g_clear_object(&container->preview_texture);
    }
    else 
    {
        if(container->preview_texture)
        {
            float width = (float)gtk_widget_get_width(widget);
            float height = (float)gtk_widget_get_height(widget);

            graphene_rect_t bounds = {.origin = {.x = 0, .y = 0}, .size = {.width = width, .height = height}};

            gtk_snapshot_append_texture(snapshot, container->preview_texture, &bounds);
        }
    }
}

static void
orion_webview_tab_container_grab_content(OrionWebviewTabContainer* container)
{
    OrionApp* app = ORION_APP(g_application_get_default());
    OrionWebviewTabContent* content = orion_app_get_shared_tab(app, container->cpp_data.tab_id);
    orion_webview_tab_container_set_child_content(container, content);
}

static void
orion_webview_tab_container_map(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(orion_webview_tab_container_parent_class)->map(widget);
    // OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(widget);
    // orion_webview_tab_container_grab_content(container);
}

static void
on_window_notify_is_active(GtkWindow* self, GParamSpec* pspec, OrionWebviewTabContainer* container)
{
    if(gtk_window_is_active(self))
    {
        orion_webview_tab_container_grab_content(container);
    }
}

static void
orion_webview_tab_container_root(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(orion_webview_tab_container_parent_class)->root(widget);
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(widget);
    GtkWindow* win = GTK_WINDOW(gtk_widget_get_root(widget));
    if(win)
    {
        gpointer prev = g_weak_ref_get(container->previous_window_weak);
        if(prev)
        {
            defer([prev]{ g_object_unref(prev); });
            GtkWindow* prev_win = GTK_WINDOW(prev);
            g_signal_handlers_disconnect_by_data(prev_win, widget);
        }
        g_signal_connect_object(win, "notify::is-active", G_CALLBACK(on_window_notify_is_active), container, G_CONNECT_DEFAULT);
        g_weak_ref_set(container->previous_window_weak, win);
    }
}

void
orion_webview_tab_container_set_content_seq_num(OrionWebviewTabContainer* container, double seq_num, UpdateBackend update_backend)
{

    if(container->seq_num != seq_num)
    {
        container->seq_num = seq_num;
        if(update_backend == UpdateBackend::yes)
        {
            orion::UpdateTabQuery query{};
            query.id = container->cpp_data.tab_id;
            query.update_seq_num = true;
            query.seq_num = seq_num;
            global_core_api->update_tab(query, [](orion::Error err){ VERIFY_LOG(err); });
        }
        g_object_notify_by_pspec(G_OBJECT(container), obj_properties[PROP_SEQ_NUM]);
    }
}

void
orion_webview_tab_container_set_parent_id(OrionWebviewTabContainer* container, orion::TabId parent_id)
{
    if (parent_id == container->cpp_data.parent_id) return;
    
    container->cpp_data.parent_id = parent_id;
    g_object_notify_by_pspec(G_OBJECT(container), obj_properties[PROP_PARENT_ID]);
}

void 
orion_webview_tab_container_set_nested_tabs_expandable(OrionWebviewTabContainer* container, bool expandable)
{
    if(!!(expandable) != !!container->nested_tabs_expandable)
    {
        container->nested_tabs_expandable = expandable;

        g_object_notify_by_pspec(G_OBJECT(container), obj_properties[PROP_NESTED_TABS_EXPANDABLE]);
    }
}

bool
orion_webview_tab_container_set_group_expanded(OrionWebviewTabContainer* container, bool group_expanded)
{
    if(group_expanded != container->group_expanded)
    {
        container->group_expanded = group_expanded;
        
        orion::UpdateTabQuery query{};
        query.id = container->cpp_data.tab_id;
        query.is_collapsed = !group_expanded;
        query.update_is_collapsed = true;
        
        global_core_api->update_tab(query, [](orion::Error err){
            VERIFY_LOG(err);
        });
        return true;
    }
    else
    {
        return false;
    }
}

void
orion_webview_tab_container_set_indent(OrionWebviewTabContainer* container, int indent)
{
    if(indent != container->indent)
    {
        container->indent = indent;

        g_object_notify_by_pspec(G_OBJECT(container), obj_properties[PROP_INDENT]);
    } 
}

void
orion_webview_tab_container_set_content_is_suspended(OrionWebviewTabContainer* container, bool is_suspended)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(content)
    {
        orion_webview_tab_content_set_is_suspended(content, is_suspended);
    }
}

void
orion_webview_tab_container_move_to_window(OrionWebviewTabContainer* container, orion::WindowId id, orion::TabId tab_after, orion::TabId parent_id, bool transfer_with_children)
{
    orion::MoveTabQuery query{};
    query.tab_id = container->cpp_data.tab_id;
    
    query.target_window_id = id;
    query.parent_tab_id = parent_id;
    query.is_nested_tabs_operation = transfer_with_children;
    query.after_tab_id = tab_after;
    // TODO: Set query.reorder_descendants = true if seq_num change originated from
    // sidebar, Vertical Tabs mode is active, and Show Nested Tabs setting is enabled.
    // Otherwise, set query.move_children_to_parent = true to fix parent tab hierarchy.
    global_core_api->move_tab(query, [](orion::Error err){
        VERIFY_LOG(err);
    });
}

void
orion_webview_tab_container_set_last_active_time(
    OrionWebviewTabContainer* container, orion::UnixTimeUtc64 last_active_time)
{
    orion::log_verbose(
        "orion_webview_tab_container_set_last_active_time: tab_id=%lld last_active_time=%lld",
        container->cpp_data.tab_id.value, last_active_time.value
    );
    container->cpp_data.last_active_time = last_active_time;
}

orion::UnixTimeUtc64
orion_webview_tab_container_get_last_active_time(OrionWebviewTabContainer* container)
{
    return container->cpp_data.last_active_time;
}

static void
orion_webview_tab_container_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    orion::log_verbose("orion_webview_tab_container_set_property: property_id=%u", property_id);
    OrionWebviewTabContainer* self = ORION_WEBVIEW_TAB_CONTAINER(object);

    switch (property_id)
    {
        //NOTE: these are only here to emit property changed signals based on the tab content for this tab
        case PROP_ID:
            self->cpp_data.tab_id = orion::TabId(g_value_get_int64(value));
            break;
        case PROP_PARENT_ID:
            orion_webview_tab_container_set_parent_id(self, orion::TabId(g_value_get_int64(value)));
            break;
        case PROP_NESTED_TABS_EXPANDABLE:
            orion_webview_tab_container_set_nested_tabs_expandable(self, g_value_get_boolean(value));
            break;
        case PROP_INDENT:
            orion_webview_tab_container_set_indent(self, g_value_get_int(value));
            break;
        case PROP_URI:
            break;
        case PROP_ICON:
            break;
        case PROP_IS_LOADING:
            break;
        case PROP_TAB_TITLE:
            break;
        case PROP_CAN_GO_BACK:
            break;
        case PROP_CAN_GO_FORWARD:
            break;
        case PROP_SEQ_NUM:
            self->seq_num = g_value_get_double(value);
            break;
        case PROP_IS_READERABLE:
            break;
        case PROP_READER_MODE_ENABLED:
            break;
        case PROP_IS_PLAYING_AUDIO:
            break;
        case PROP_IS_MUTED:
            break;
        case PROP_TLS_STATUS:
            break;
        case PROP_BACK_LIST:
            break;
        case PROP_FORWARD_LIST:
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

orion::TabId
orion_webview_tab_container_get_id(OrionWebviewTabContainer* container)
{
    return container->cpp_data.tab_id;
}

orion::TabId
orion_webview_tab_container_get_parent_id(OrionWebviewTabContainer* container)
{
    return container->cpp_data.parent_id;
}

std::string
orion_webview_tab_container_get_title(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(content)
    {
        return orion_webview_tab_content_get_title(content);
    }
    else
    {
        return "";
    }
}

GIcon*
orion_webview_tab_container_get_icon(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(content)
    {
        return orion_webview_tab_content_get_icon(content);
    }
    else
    {
        return NULL;
    }
}

bool
orion_webview_tab_container_get_is_loading(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(content)
    {
        return orion_webview_tab_content_get_is_loading(content);
    }
    else
    {
        return false;
    }
}

gboolean
orion_webview_tab_container_get_is_playing_audio(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    return content ? orion_webview_tab_content_get_is_playing_audio(content) : FALSE;
}

gboolean
orion_webview_tab_container_get_is_muted(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    return content ? orion_webview_tab_content_get_is_muted(content) : FALSE;
}

orion::TlsStatus
orion_webview_tab_container_get_tls_status(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    return content ? orion_webview_tab_content_get_tls_status(content) : orion::TlsStatus::none;
}

orion::BackForwardList*
orion_webview_tab_container_get_back_list(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    return content ? orion_webview_tab_content_get_back_list(content) : NULL;
}

orion::BackForwardList*
orion_webview_tab_container_get_forward_list(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    return content ? orion_webview_tab_content_get_forward_list(content) : NULL;
}

void
orion_webview_tab_container_set_is_muted(OrionWebviewTabContainer* container, gboolean muted)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(content) orion_webview_tab_content_set_is_muted(content, muted);
}

bool
orion_webview_tab_container_get_is_suspended(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(content)
    {
        return orion_webview_tab_content_get_is_suspended(content);
    }
    else
    {
        return false;
    }
}

double
orion_webview_tab_container_get_seq_num(OrionWebviewTabContainer* container)
{
    return container->seq_num;
}

bool
orion_webview_tab_container_get_nested_tabs_expandable(OrionWebviewTabContainer* container)
{
    return container->nested_tabs_expandable;
}

int
orion_webview_tab_container_get_indent(OrionWebviewTabContainer* container)
{
    return container->indent;
}

orion::Uri
orion_webview_tab_container_get_uri(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(content)
    {
        return orion_webview_tab_content_get_uri(content);
    }
    else
    {
        return orion::Uri::parse_strict("");
    }
}

bool
orion_webview_tab_container_get_can_go_back(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(content)
    {
        return orion_webview_tab_content_can_go_back(content);
    }
    else
    {
        return false;
    }
}

bool
orion_webview_tab_container_get_can_go_forward(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(content)
    {
        return orion_webview_tab_content_can_go_forward(content);
    }
    else
    {
        return false;
    }
}

bool
orion_webview_tab_container_get_group_expanded(OrionWebviewTabContainer* container)
{
    return container->group_expanded;
}

bool
orion_webview_tab_container_get_is_readerable(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(content)
    {
        return orion_webview_tab_content_get_is_readerable(content);
    }
    else
    {
        return false;
    }
}

bool
orion_webview_tab_container_get_reader_mode_enabled(OrionWebviewTabContainer* container)
{
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(content)
    {
        return orion_webview_tab_content_get_reader_mode_enabled(content);
    }
    else
    {
        return false;
    }
}


static void
orion_webview_tab_container_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    OrionWebviewTabContainer* self = ORION_WEBVIEW_TAB_CONTAINER(object);

    switch (prop_id)
    {
        case PROP_ID:
            g_value_set_int64(value, orion_webview_tab_container_get_id(self).value);
            break;
        case PROP_PARENT_ID:
            g_value_set_int64(value, orion_webview_tab_container_get_parent_id(self).value);
            break;
        case PROP_TAB_TITLE:
            g_value_set_string(value, orion_webview_tab_container_get_title(self).c_str());
            break;
        case PROP_ICON:
            g_value_take_object(value, orion_webview_tab_container_get_icon(self));
            break;
        case PROP_IS_LOADING:
            g_value_set_boolean(value, orion_webview_tab_container_get_is_loading(self));
            break;
        case PROP_SEQ_NUM:
            g_value_set_double(value, orion_webview_tab_container_get_seq_num(self));
            break;
        case PROP_NESTED_TABS_EXPANDABLE:
            g_value_set_boolean(value, orion_webview_tab_container_get_nested_tabs_expandable(self));
            break;
        case PROP_INDENT:
            g_value_set_int(value, orion_webview_tab_container_get_indent(self));
            break;
        case PROP_URI:
            g_value_set_string(value, orion_webview_tab_container_get_uri(self).uri().c_str());
            break;
        case PROP_CAN_GO_BACK:
            g_value_set_boolean(value, orion_webview_tab_container_get_can_go_back(self));
            break;
        case PROP_CAN_GO_FORWARD:
            g_value_set_boolean(value, orion_webview_tab_container_get_can_go_forward(self));
            break;
        case PROP_IS_READERABLE:
            g_value_set_boolean(value, orion_webview_tab_container_get_is_readerable(self));
            break;
        case PROP_READER_MODE_ENABLED:
            g_value_set_boolean(value, orion_webview_tab_container_get_reader_mode_enabled(self));
            break;
        case PROP_IS_PLAYING_AUDIO:
            g_value_set_boolean(value, orion_webview_tab_container_get_is_playing_audio(self));
            break;
        case PROP_IS_MUTED:
            g_value_set_boolean(value, orion_webview_tab_container_get_is_muted(self));
            break;
        case PROP_TLS_STATUS:
            g_value_set_int64(value, static_cast<gint64>(static_cast<int64_t>(orion_webview_tab_container_get_tls_status(self))));
            break;
        case PROP_BACK_LIST:
            g_value_take_boxed(value, orion_webview_tab_container_get_back_list(self));
            break;
        case PROP_FORWARD_LIST:
            g_value_take_boxed(value, orion_webview_tab_container_get_forward_list(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
orion_webview_tab_container_class_init(OrionWebviewTabContainerClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = orion_webview_tab_container_dispose;
    object_class->finalize = orion_webview_tab_container_finalize;
    object_class->set_property = orion_webview_tab_container_set_property;
    object_class->get_property = orion_webview_tab_container_get_property;

    obj_properties[PROP_ID] = g_param_spec_int64(
        "id", NULL, NULL, INT64_MIN, INT64_MAX, 0, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_PARENT_ID] = g_param_spec_int64(
        "parent-id", NULL, NULL, INT64_MIN, INT64_MAX, 0, GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_TAB_TITLE] = g_param_spec_string(
        "tab-title", NULL, NULL, "", GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_ICON] = g_param_spec_object(
        "icon", NULL, NULL, G_TYPE_ICON, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_IS_LOADING] = g_param_spec_boolean(
        "is-loading", NULL, NULL, false, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    //NOTE: construct only to enforce mindfulness about backend updates when setting this later
    obj_properties[PROP_SEQ_NUM] = g_param_spec_double(
        "seq-num", NULL, NULL, -DBL_MAX, DBL_MAX, 0, GParamFlags(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_NESTED_TABS_EXPANDABLE] = g_param_spec_boolean(
        "nested-tabs-expandable", NULL, NULL, false, GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_INDENT] = g_param_spec_int(
        "indent", NULL, NULL, 0, INT_MAX, 0, GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_URI] = g_param_spec_string(
        "uri", NULL, NULL, "", GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_CAN_GO_BACK] = g_param_spec_boolean(
        "can-go-back", NULL, NULL, false, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_CAN_GO_FORWARD] = g_param_spec_boolean(
        "can-go-forward", NULL, NULL, false, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_IS_READERABLE] = g_param_spec_boolean(
        "is-readerable", NULL, NULL, false, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_READER_MODE_ENABLED] = g_param_spec_boolean(
        "reader-mode-enabled", NULL, NULL, false, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_IS_PLAYING_AUDIO] = g_param_spec_boolean(
        "is-playing-audio", NULL, NULL, false, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_IS_MUTED] = g_param_spec_boolean(
        "is-muted", NULL, NULL, false, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_TLS_STATUS] = g_param_spec_int64("tls-status", NULL, NULL, INT64_MIN, INT64_MAX, gint64(orion::TlsStatus::none),
        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_BACK_LIST] = g_param_spec_boxed("back-list", NULL, NULL, APP_TYPE_BACK_FORWARD_LIST,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_FORWARD_LIST] = g_param_spec_boxed("forward-list", NULL, NULL, APP_TYPE_BACK_FORWARD_LIST,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_properties(object_class, N_PROPS, obj_properties);

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot = orion_webview_tab_container_snapshot;
    widget_class->map = orion_webview_tab_container_map;
    widget_class->root = orion_webview_tab_container_root;

    gtk_widget_class_set_layout_manager_type(widget_class, GTK_TYPE_BIN_LAYOUT);
}