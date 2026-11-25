#include "tab_switcher.h"

#include <common/src/log.h>
#include <core/core_api.h>
#include <common/src/defer.h>

#include "app.h"
#include "window/main_window/main_window.h"
#include "util/text_util.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_container.h"

#define MAX_COLUMNS_OF_TABS static_cast<uint>(8)

//region TabSwitcherModel declarations

#define APP_TYPE_TAB_SWITCHER_MODEL (tab_switcher_model_get_type())

G_DECLARE_FINAL_TYPE(TabSwitcherModel, tab_switcher_model, APP, TAB_SWITCHER_MODEL, GObject)

static void tab_switcher_model_selection_finish(TabSwitcherModel* model);
static void tab_switcher_model_selection_init(TabSwitcherModel* model);

#define APP_TYPE_TAB_SWITCH_ORDER_ENUM tab_switch_order_get_type()

GType
tab_switch_order_get_type()
{
  static GType type = 0;
  if (G_UNLIKELY (type == 0)) {
    static const GEnumValue values[] = {
      { static_cast<int>(orion::settings::TabSwitchOrder::not_set),  "TAB_SWITCH_ORDER_NOT_SET",  "not_set"  },
      { static_cast<int>(orion::settings::TabSwitchOrder::tab_order_in_window),  "TAB_SWITCH_ORDER_STANDARD",  "standard"  },
      { static_cast<int>(orion::settings::TabSwitchOrder::most_recently_used_order),  "TAB_SWITCH_ORDER_RECENTLY_USED",  "recently_used"  },
      { 0, NULL, NULL }
    };
    type = g_enum_register_static ("TabSwitchOrder", values);
  }
  return type;
}

//endregion TabSwitcherModel declarations

namespace {
    struct TabSwitcherCppWrapper {
    };
}

struct _TabSwitcher
{
    AdwBin parent_type;

    TabSwitcherCppWrapper cpp_wrapper;

    GtkGridView* tab_grid_view;
    GBinding* order_by_binding;
    
    bool left_ctrl_pressed;
    bool right_ctrl_pressed;
};

G_DEFINE_TYPE(TabSwitcher, tab_switcher, ADW_TYPE_BIN)

static void
on_pages_items_changed(GListModel* pages, guint pos, guint removed, guint added, gpointer user_data)
{
    uint n_items = g_list_model_get_n_items(G_LIST_MODEL(pages));
    if(n_items)
    {
        uint n_columns = std::min(n_items, MAX_COLUMNS_OF_TABS);
        gtk_grid_view_set_max_columns(GTK_GRID_VIEW(user_data), n_columns);
        gtk_grid_view_set_min_columns(GTK_GRID_VIEW(user_data), n_columns);
    }
}

static void
on_tab_grid_view_map(GtkGridView* view, gpointer user_data)
{
    tab_switcher_model_selection_init(APP_TAB_SWITCHER_MODEL(user_data));
}

static gboolean
tab_switch_order_setting_transform_to(GBinding* binding, const GValue* from_val, GValue* to_val, gpointer user_data)
{
    GVariant* var = g_value_get_variant(from_val);
    
    size_t len;
    const char* parameter_string = g_variant_get_string(var, &len);
    char* end;
    int val = strtol(parameter_string, &end, 10);
    if((size_t)(end-parameter_string)!=len)
    {
        orion::log_debug("programming error: tab switcher tab_switch_order_setting_transform_to() got passed bad action parameter string: \"%s\"", parameter_string);
        return false;
    }
    //orion::settings::TabSwitchOrder order = orion::settings::TabSwitchOrder(val);
    g_value_set_enum(to_val, val);
    return true;
}

void
tab_switcher_set_tab_view(TabSwitcher* self, AdwTabView* tab_view)
{
    GtkSelectionModel* prev_model = gtk_grid_view_get_model(self->tab_grid_view);
    if(prev_model)
    {
        g_signal_handlers_disconnect_by_data(prev_model, self->tab_grid_view);
        g_signal_handlers_disconnect_by_data(self->tab_grid_view, prev_model);
    }
    if(self->order_by_binding)
    {
        g_clear_pointer(&self->order_by_binding, g_binding_unbind);
    }

    GtkSelectionModel* pages = adw_tab_view_get_pages(tab_view);
    defer([pages]{ g_object_unref(pages); });
    gpointer model = g_object_new(APP_TYPE_TAB_SWITCHER_MODEL, "model", pages, NULL);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    GAction* setting = g_action_map_lookup_action(G_ACTION_MAP(win), "tab-switch-order");
    self->order_by_binding = g_object_bind_property_full(setting, "state", model, "order-by", G_BINDING_DEFAULT, tab_switch_order_setting_transform_to, NULL, NULL, NULL);

    gtk_grid_view_set_model(self->tab_grid_view, GTK_SELECTION_MODEL(model));


    uint n_items = g_list_model_get_n_items(G_LIST_MODEL(model));
    if(n_items)
    {
        uint n_columns = std::min(n_items, MAX_COLUMNS_OF_TABS);
        gtk_grid_view_set_max_columns(self->tab_grid_view, n_columns);
        gtk_grid_view_set_min_columns(self->tab_grid_view, n_columns);
    }
    g_signal_connect_object(model, "items-changed", G_CALLBACK(on_pages_items_changed), self->tab_grid_view, G_CONNECT_DEFAULT);
    g_signal_connect(self->tab_grid_view, "map", G_CALLBACK(on_tab_grid_view_map), model);
}


void
tab_switcher_increment_ctrl_count(TabSwitcher* self, bool is_left_control)
{
    if(is_left_control)
    {
        self->left_ctrl_pressed = true;
    }
    else
    {
        self->right_ctrl_pressed = true;
    }
}

void
tab_switcher_select_next_tab(TabSwitcher* self)
{
    GtkSelectionModel* model = gtk_grid_view_get_model(self->tab_grid_view);
    if(!model) return;
    
    uint n_items = g_list_model_get_n_items(G_LIST_MODEL(model));
    if(!n_items) return;

    GtkBitset* selection = gtk_selection_model_get_selection(model);
    defer([selection]{ gtk_bitset_unref(selection); });

    if(!gtk_bitset_get_size(selection))
    {
        orion::log_debug("tab_switcher_select_next_tab(): no tab previously selected");
        gtk_selection_model_select_item(model, 0, true);
    }
    else
    {
        uint current_selected = gtk_bitset_get_nth(selection, 0);
        if(current_selected == n_items-1)
        {
            gtk_selection_model_select_item(model, 0, true);
        }
        else
        {
            gtk_selection_model_select_item(model, current_selected+1, true);
        }
    }
}

void
tab_switcher_select_prev_tab(TabSwitcher* self)
{
    GtkSelectionModel* model = gtk_grid_view_get_model(self->tab_grid_view);
    if(!model) return;
    
    uint n_items = g_list_model_get_n_items(G_LIST_MODEL(model));
    if(!n_items) return;

    GtkBitset* selection = gtk_selection_model_get_selection(model);
    defer([selection]{ gtk_bitset_unref(selection); });

    if(!gtk_bitset_get_size(selection))
    {
        orion::log_debug("tab_switcher_select_next_tab(): no tab previously selected");
        gtk_selection_model_select_item(model, 0, true);
    }
    else
    {
        uint current_selected = gtk_bitset_get_nth(selection, 0);
        if(current_selected == 0)
        {
            gtk_selection_model_select_item(model, n_items-1, true);
        }
        else
        {
            gtk_selection_model_select_item(model, current_selected-1, true);
        }
    }
}

static void
tab_switcher_finish(TabSwitcher* self)
{
    if(gtk_widget_get_visible(GTK_WIDGET(self)))
    {
        tab_switcher_model_selection_finish(APP_TAB_SWITCHER_MODEL(gtk_grid_view_get_model(self->tab_grid_view)));
        gtk_widget_set_visible(GTK_WIDGET(self), false);
    }
}

void
tab_switcher_decrement_ctrl_count(TabSwitcher* self, bool is_left_control)
{
    if(is_left_control)
    {
        self->left_ctrl_pressed = false;
    }
    else
    {
        self->right_ctrl_pressed = false;
    }
    if(!(self->left_ctrl_pressed || self->right_ctrl_pressed))
    {
        tab_switcher_finish(self);
    }
}

void
tab_switcher_reset_ctrl_count(TabSwitcher* self)
{
    self->left_ctrl_pressed = false;
    self->right_ctrl_pressed = false;
    tab_switcher_finish(self);
    return;
}

static void
tab_switcher_dispose(GObject* self)
{
    TabSwitcher* tab_switcher = APP_TAB_SWITCHER(self);
    GtkSelectionModel* prev_model = gtk_grid_view_get_model(tab_switcher->tab_grid_view);
    if(tab_switcher->order_by_binding)
    {
        g_clear_pointer(&tab_switcher->order_by_binding, g_binding_unbind);
    }
    if(prev_model)
    {
        g_signal_handlers_disconnect_by_data(prev_model, tab_switcher->tab_grid_view);
        g_signal_handlers_disconnect_by_data(tab_switcher->tab_grid_view, prev_model);
        gtk_grid_view_set_model(tab_switcher->tab_grid_view, NULL);
    }

    G_OBJECT_CLASS(tab_switcher_parent_class)->dispose(self);
}

static void
tab_switcher_finalize(GObject* self)
{
    TabSwitcher* tab_switcher = APP_TAB_SWITCHER(self);
    std::destroy_at(&tab_switcher->cpp_wrapper);
    G_OBJECT_CLASS(tab_switcher_parent_class)->finalize(self);
}

static void
tab_switcher_init(TabSwitcher* self)
{
    std::construct_at(&self->cpp_wrapper);
    gtk_widget_init_template(GTK_WIDGET(self));
}

static char*
get_uri_authority(GtkListItem* item, const char* uri)
{
    return g_strdup(orion::Uri::parse_strict(uri).authority().c_str());
}

static void
on_tab_preview_picture_map(GtkPicture* picture, GtkListItem* item)
{
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    orion::TabId id = orion_webview_tab_container_get_id(container);

    orion::gtk::WeakRefWrapper picture_weak(picture);
                
    global_core_api->get_tabs({id},
        [picture_weak](orion::GetTabsReply reply){
            VERIFY_LOG(reply.err);

            if(reply.tabs.empty()) return;

            std::vector<uint8_t>& image_data = reply.tabs.at(0).snapshot_image; 

            auto picture_strong = picture_weak.lock();
            if(!picture_strong) return;

            GtkPicture* picture = GTK_PICTURE(picture_strong.get());

            if(!(image_data.size()))
            {
            }
            else
            {
                GBytes* image_bytes = g_bytes_new_static(image_data.data(), image_data.size());
                defer([image_bytes]{ g_bytes_unref(image_bytes); });
                GError* err = NULL;
                GdkTexture* texture = gdk_texture_new_from_bytes(image_bytes, &err);
                if(err)
                {
                    orion::log_debug("on_tab_event_controller_motion_enter while attempting to create texture from backend data: %s ", err->message);
                    g_error_free(err);
                    return;
                }
                defer([texture]{ g_object_unref(texture); });
                gtk_picture_set_paintable(picture, GDK_PAINTABLE(texture));
            }
        });
}

static void
tab_switcher_class_init(TabSwitcherClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = tab_switcher_dispose;
    object_class->finalize = tab_switcher_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/tab_switcher.ui");

    gtk_widget_class_bind_template_child(widget_class, TabSwitcher, tab_grid_view);
    gtk_widget_class_bind_template_callback(widget_class, get_uri_authority);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_preview_picture_map);
    gtk_widget_class_bind_template_callback(widget_class, tab_switcher_reset_ctrl_count);
}


//region TabSwitchModel

namespace {
    struct TabSwitcherModelCppWrapper {
        orion::settings::TabSwitchOrder tab_switch_order;
        //this is in reverse order since we can only push_back
        std::vector<uint> recently_used_tab_order;
    };
}

struct _TabSwitcherModel
{
    GObject object;
    GtkSelectionModel* model;
    uint selected_item;
    TabSwitcherModelCppWrapper cpp_data;
};

static void tab_switcher_model_g_list_model_interface_init(GListModelInterface* iface);
static void tab_switcher_model_gtk_selection_model_interface_init(GtkSelectionModelInterface* iface);
G_DEFINE_TYPE_WITH_CODE(
    TabSwitcherModel,
    tab_switcher_model,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL, tab_switcher_model_g_list_model_interface_init)
    G_IMPLEMENT_INTERFACE(GTK_TYPE_SELECTION_MODEL, tab_switcher_model_gtk_selection_model_interface_init)
)

namespace TabSwitcherModelProps {
    enum {
        PROP_0,
        PROP_MODEL,
        PROP_ORDER_BY,
        N_PROPERTIES
    };

    static GParamSpec* obj_properties[TabSwitcherModelProps::N_PROPERTIES] = {NULL,};
};




static uint
tab_switcher_model_wrapping_pos_to_real(TabSwitcherModel* self, uint wrapping_pos)
{
    if(!self->model)
    {
        return GTK_INVALID_LIST_POSITION;
    }
    if(self->cpp_data.tab_switch_order == orion::settings::TabSwitchOrder::most_recently_used_order)
    {
        uint recently_used_tab_vec_size = self->cpp_data.recently_used_tab_order.size();
        uint total_size = g_list_model_get_n_items(G_LIST_MODEL(self->model));
        if(wrapping_pos < recently_used_tab_vec_size)
        {
            return self->cpp_data.recently_used_tab_order.at(recently_used_tab_vec_size-1-wrapping_pos);
        }
        else if(wrapping_pos < total_size)
        {
            //this holds the position we are on after the recently_used_tab_order vector, 
            uint index_after_recently_used = wrapping_pos - recently_used_tab_vec_size;

            //this loop looks for N-th item not in the recently_used_tab_order vector
            uint item_not_in_recently_used_count = 0;
            for(uint i = 0; i<total_size; i++)
            {
                auto it = std::find(self->cpp_data.recently_used_tab_order.begin(), self->cpp_data.recently_used_tab_order.end(), i);
                if(it == self->cpp_data.recently_used_tab_order.end())
                {
                    //needs to happen pre increment because 0 indexing
                    if(item_not_in_recently_used_count == index_after_recently_used)
                    {
                        return i;
                    }
                    item_not_in_recently_used_count++;
                }
            }

            return GTK_INVALID_LIST_POSITION;
        }
        else
        {
            return GTK_INVALID_LIST_POSITION;
        }
    }
    else if(self->cpp_data.tab_switch_order == orion::settings::TabSwitchOrder::tab_order_in_window)
    {
        return wrapping_pos;
    }
    else
    {
        orion::log_verbose("tab_switcher_model_wrapping_pos_to_real(): tab switch order not set to a valid value, defaulting to standard");
        return wrapping_pos;
    }
}


static void
tab_switcher_model_init(TabSwitcherModel* self)
{

}

static void
tab_switcher_model_dispose(GObject* self)
{
    TabSwitcherModel* model = reinterpret_cast<TabSwitcherModel*>(self);
    if(model->model)
    {
        g_signal_handlers_disconnect_by_data(model, model->model);
        g_clear_object(&model->model);
    }
    G_OBJECT_CLASS(tab_switcher_model_parent_class)->dispose(self);
}

static void
tab_switcher_model_finalize(GObject* self)
{
    TabSwitcherModel* model = reinterpret_cast<TabSwitcherModel*>(self);
    if(model->model)
    {
        g_signal_handlers_disconnect_by_data(model->model, model);
        g_clear_object(&model->model);
    }
    G_OBJECT_CLASS(tab_switcher_model_parent_class)->dispose(self);
}

static void
tab_switcher_model_on_underlying_model_items_changed(GListModel *model, guint position, guint removed, guint added, gpointer user_data)
{
    TabSwitcherModel* self = APP_TAB_SWITCHER_MODEL(user_data);
    auto& recently_used_tab_order = self->cpp_data.recently_used_tab_order;

    //indexes in recently used tab order after position need to either be deleted or shifted
    auto it = recently_used_tab_order.begin();
    while (it != recently_used_tab_order.end()) {
        if (*it >= position && *it < (position + removed))
        {
            //deletes indices that are inside the removed range
            if(*it < (position + removed))
            {
                it = recently_used_tab_order.erase(it);
            }
            //shifts indices after modified range by added-removed
            else
            {
                *it += added - removed;
                ++it;
            }
        } 
        else
        {
            ++it;
        }
    }
    //TODO: improved items changed signal, currently just reports whole list as changed
    uint n_items = g_list_model_get_n_items(G_LIST_MODEL(model));
    g_list_model_items_changed(G_LIST_MODEL(self), 0, n_items+removed-added, n_items); 
}

static void
tab_switcher_model_on_underlying_model_selection_changed(GtkSelectionModel* model, guint position, guint n_items, gpointer user_data)
{
    TabSwitcherModel* self = APP_TAB_SWITCHER_MODEL(user_data);
    auto& recently_used_tab_order = self->cpp_data.recently_used_tab_order;

    GtkBitset* selection = gtk_selection_model_get_selection(model);
    defer([selection]{ gtk_bitset_unref(selection); });
    if(gtk_bitset_get_size(selection))
    {
        uint selected_position = gtk_bitset_get_nth(selection, 0);
        //we first calculate old position of the item, deleting a previous reference from recently_used_tab_order
        
        bool should_emit_items_changed = self->cpp_data.tab_switch_order == orion::settings::TabSwitchOrder::most_recently_used_order;
        uint removed_position;

        auto it = std::find(recently_used_tab_order.begin(), recently_used_tab_order.end(), selected_position);
        //if item was in the recently_used_tab_order vector, we just need to invert its position to get the wrapped pos,
        //since this vector is always exposed reverse at the front of the glistmodel
        if(it != recently_used_tab_order.end())
        {
            //if item was at the start (end because reversed) of the recently_used_tab_order vector, no further operation is needed
            if(it == recently_used_tab_order.end()-1)
            {
                return;
            }

            //this inverts the index of the iterator in the vector
            removed_position = std::distance(it, recently_used_tab_order.end())-1;
            //erase here to have our backing data consistent before emiting items changed
            recently_used_tab_order.erase(it);
        }
        else
        {
            //if the item is past the recently_used_tab_order vector, it can be preceeded by items of a higher index in the wrapped model, meaning
            //we have to find out how many indexes past this one are actually preceeding it in the wrapping model
            uint later_indexes_in_recently_used_vec_count =
                std::count_if(recently_used_tab_order.begin(), recently_used_tab_order.end(),
                    [selected_position](uint recently_used_index){ return recently_used_index>selected_position; });

            removed_position = selected_position + later_indexes_in_recently_used_vec_count;
        }
        recently_used_tab_order.push_back(selected_position);
        
        if(should_emit_items_changed) g_list_model_items_changed(G_LIST_MODEL(self), 0, removed_position+1, removed_position+1);
    }
}

static void
tab_switcher_model_set_underlying_model(TabSwitcherModel* self, GtkSelectionModel* model)
{
    if(model == self->model)
    {
        return;
    }

    if(self->model)
    {
        self->cpp_data.recently_used_tab_order.clear();
        g_signal_handlers_disconnect_by_data(self->model, model);
        g_clear_object(&self->model);
    }
    if(model)
    {
        GtkBitset* selection = gtk_selection_model_get_selection(model);
        defer([selection]{ gtk_bitset_unref(selection); });
        if(gtk_bitset_get_size(selection))
        {
            self->cpp_data.recently_used_tab_order.push_back(gtk_bitset_get_nth(selection, 0));
        }
        else
        {
            orion::log_verbose("tab_switcher_model_set_underlying_model(): given selection model has no selection, should not happen with adw tab view pages");
        }
        self->model = model;
        g_signal_connect(model, "items-changed", G_CALLBACK(tab_switcher_model_on_underlying_model_items_changed), self);
        g_signal_connect(model, "selection-changed", G_CALLBACK(tab_switcher_model_on_underlying_model_selection_changed), self);
    }

    g_object_notify_by_pspec(G_OBJECT(self), TabSwitcherModelProps::obj_properties[TabSwitcherModelProps::PROP_MODEL]);
}

static void
tab_switcher_model_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    TabSwitcherModel* self = APP_TAB_SWITCHER_MODEL(object);

    switch (property_id) {
        case TabSwitcherModelProps::PROP_MODEL:
            tab_switcher_model_set_underlying_model(self, GTK_SELECTION_MODEL(g_value_get_object(value)));
            break;

        case TabSwitcherModelProps::PROP_ORDER_BY:
            self->cpp_data.tab_switch_order = static_cast<orion::settings::TabSwitchOrder>(g_value_get_enum(value));
            g_list_model_items_changed(G_LIST_MODEL(self), 0, g_list_model_get_n_items(G_LIST_MODEL(self->model)), g_list_model_get_n_items(G_LIST_MODEL(self->model)));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
tab_switcher_model_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    TabSwitcherModel* self = APP_TAB_SWITCHER_MODEL(object);

    switch (property_id) {
        case TabSwitcherModelProps::PROP_MODEL:
            g_value_set_object(value, self->model);
            break;

        case TabSwitcherModelProps::PROP_ORDER_BY:
            g_value_set_enum(value, static_cast<int>(self->cpp_data.tab_switch_order));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
tab_switcher_model_class_init(TabSwitcherModelClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = tab_switcher_model_dispose;
    object_class->finalize = tab_switcher_model_finalize;

    object_class->set_property = tab_switcher_model_set_property;
    object_class->get_property = tab_switcher_model_get_property;
    TabSwitcherModelProps::obj_properties[TabSwitcherModelProps::PROP_MODEL] = 
        g_param_spec_object("model",
            "Model",
            "Underlying GListModel",
            G_TYPE_LIST_MODEL,
            GParamFlags(G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
    TabSwitcherModelProps::obj_properties[TabSwitcherModelProps::PROP_ORDER_BY] = 
        g_param_spec_enum("order-by",
            "Order by",
            "How the exposed items should be ordered",
            APP_TYPE_TAB_SWITCH_ORDER_ENUM,
            static_cast<int>(orion::settings::TabSwitchOrder::tab_order_in_window),
            GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_properties(object_class, static_cast<uint>(TabSwitcherModelProps::N_PROPERTIES), TabSwitcherModelProps::obj_properties);
}

static gpointer
tab_switcher_model_get_item(GListModel* model, uint pos)
{
    TabSwitcherModel* self = reinterpret_cast<TabSwitcherModel*>(model);
    if(!self->model) return NULL;
    
    uint real_pos = tab_switcher_model_wrapping_pos_to_real(self, pos);
    if(real_pos == GTK_INVALID_LIST_POSITION)
    {
        return NULL;
    }
    else
    {
        return g_list_model_get_item(G_LIST_MODEL(self->model), real_pos);
    }
}

static GType
tab_switcher_model_get_item_type(GListModel* model)
{
    TabSwitcherModel* self = APP_TAB_SWITCHER_MODEL(model);
    if(!self->model) return G_TYPE_NONE;

    return g_list_model_get_item_type(G_LIST_MODEL(self->model));
}


static uint
tab_switcher_model_get_n_items(GListModel* model)
{
    TabSwitcherModel* self = APP_TAB_SWITCHER_MODEL(model);
    if(!self->model) return 0;

    return g_list_model_get_n_items(G_LIST_MODEL(self->model));
}

static void
tab_switcher_model_g_list_model_interface_init(GListModelInterface* iface)
{
    iface->get_item = tab_switcher_model_get_item;
    iface->get_item_type = tab_switcher_model_get_item_type;
    iface->get_n_items = tab_switcher_model_get_n_items;
}

static GtkBitset*
tab_switcher_model_get_selection_in_range(GtkSelectionModel* model, guint position, guint n_items)
{
    TabSwitcherModel* specific_model = APP_TAB_SWITCHER_MODEL(model);
    
    if(specific_model->selected_item >= position && specific_model->selected_item < position+n_items)
    {
        return gtk_bitset_new_range(specific_model->selected_item, 1);
    }
    else
    {
        return gtk_bitset_new_empty();
    }
}

static gboolean
tab_switcher_model_is_selected(GtkSelectionModel* model, guint position)
{
    return position == APP_TAB_SWITCHER_MODEL(model)->selected_item;
}

static gboolean
tab_switcher_model_select_all(GtkSelectionModel* model)
{
    return FALSE;
}

static gboolean
tab_switcher_model_select_item(GtkSelectionModel* model, guint position, gboolean unselect_rest)
{
    TabSwitcherModel* specific_model = APP_TAB_SWITCHER_MODEL(model);
    guint n_items = g_list_model_get_n_items(G_LIST_MODEL(specific_model->model));
    if(position < n_items && position != specific_model->selected_item)
    {
        uint old_selected_item = specific_model->selected_item;
        specific_model->selected_item = position;
        if(old_selected_item < n_items) gtk_selection_model_selection_changed(model, old_selected_item, 1);
        gtk_selection_model_selection_changed(model, position, 1);
    }
    
    return TRUE;
}

static gboolean
tab_switcher_model_select_range(GtkSelectionModel* model, guint position, guint n_items, gboolean unselect_rest)
{
    return FALSE;
}

static gboolean
tab_switcher_model_set_selection(GtkSelectionModel* model, GtkBitset* selected, GtkBitset* mask)
{
    return FALSE;
}

static gboolean
tab_switcher_model_unselect_all(GtkSelectionModel* model)
{
    return FALSE;
}

static gboolean
tab_switcher_model_unselect_item(GtkSelectionModel* model, guint position)
{
    return FALSE;
}

static gboolean
tab_switcher_model_unselect_range(GtkSelectionModel* model, guint position, guint n_items)
{
    return FALSE;
}

static void
tab_switcher_model_gtk_selection_model_interface_init(GtkSelectionModelInterface* iface)
{
    iface->get_selection_in_range = tab_switcher_model_get_selection_in_range;
    iface->is_selected = tab_switcher_model_is_selected;
    iface->select_all = tab_switcher_model_select_all;
    iface->select_item = tab_switcher_model_select_item;
    iface->select_range = tab_switcher_model_select_range;
    iface->set_selection = tab_switcher_model_set_selection;
    iface->unselect_all = tab_switcher_model_unselect_all;
    iface->unselect_item = tab_switcher_model_unselect_item;
    iface->unselect_range = tab_switcher_model_unselect_range;
}

static void
tab_switcher_model_selection_init(TabSwitcherModel* model)
{
    if(model->cpp_data.tab_switch_order == orion::settings::TabSwitchOrder::most_recently_used_order)
    {
        // uint prev_selected_item = model->selected_item;
        // model->selected_item = 0;
        // gtk_selection_model_selection_changed(GTK_SELECTION_MODEL(model), prev_selected_item, 1);
        // gtk_selection_model_selection_changed(GTK_SELECTION_MODEL(model), 0, 1);
        tab_switcher_model_select_item(GTK_SELECTION_MODEL(model), 0, true);
    }
    else if(model->cpp_data.tab_switch_order == orion::settings::TabSwitchOrder::tab_order_in_window)
    {
        GtkBitset* selection = gtk_selection_model_get_selection(model->model);
        defer([selection]{ gtk_bitset_unref(selection); });
        if(!gtk_bitset_get_size(selection))
        {
            orion::log_debug("tab_switcher_model_selection_init(): underlying selection model has no selection");
            tab_switcher_model_select_item(GTK_SELECTION_MODEL(model), 0, true);
            return;
        }
        // uint prev_selected_item = model->selected_item;
        // model->selected_item = gtk_bitset_get_nth(selection, 0);
        // gtk_selection_model_selection_changed(GTK_SELECTION_MODEL(model), prev_selected_item, 1);
        // gtk_selection_model_selection_changed(GTK_SELECTION_MODEL(model), model->selected_item, 1);
        
        tab_switcher_model_select_item(GTK_SELECTION_MODEL(model), gtk_bitset_get_nth(selection, 0), true);
    }
}

static void
tab_switcher_model_selection_finish(TabSwitcherModel* model)
{
    if(!model->model)
    {
        orion::log_debug("tab_switcher_model_selection_finish(): model does not have an underlying model set");
    }

    gtk_selection_model_select_item(model->model, tab_switcher_model_wrapping_pos_to_real(model, model->selected_item), true);
}

//endregion TabSwitcher