#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "window/orion_webview_tabview_widgets/orion_webview_tab_container.h"
#include "orion_tab_selection_model.h"
#include <common/src/defer.h>

//FIXME: do not change tab on collapsing a group
//FIXME: make this aware of tab reordering through adw tab view, so that this handles tab grouping accordingly

struct _OrionTabSelectionModel
{
	GObject object;
    GListModel* model;
    uint old_len;
    bool show_nested_tabs;
};


static void orion_tab_selection_model_g_list_model_interface_init(GListModelInterface* iface);
static void orion_tab_selection_model_gtk_selection_model_interface_init(GtkSelectionModelInterface* iface);
G_DEFINE_TYPE_WITH_CODE(
    OrionTabSelectionModel,
    orion_tab_selection_model,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL, orion_tab_selection_model_g_list_model_interface_init)
    G_IMPLEMENT_INTERFACE(GTK_TYPE_SELECTION_MODEL, orion_tab_selection_model_gtk_selection_model_interface_init)
)

enum {
    PROP_0,
    PROP_MODEL,
    PROP_SHOW_NESTED,
    N_PROPERTIES
};

static GParamSpec* obj_properties[N_PROPERTIES] = {NULL,};

static guint
orion_tab_selection_model_get_actual_pos(OrionTabSelectionModel* model, guint pos)
{
    //translates position to real pos in underlying model
    
    //calculation is only needed when nested tabs are to be shown
    //goes through wrapped model, finds all collapsed groups that precede pos,
    //adds their count onto pos so that return value is now the intended index in the wrapped model
    if(model->show_nested_tabs)
    {
        uint len = g_list_model_get_n_items(model->model);
        if(pos>=len) return GTK_INVALID_LIST_POSITION;
        std::vector<orion::TabId> parents;

        //NOTE: pos gets incremented for every collapsed item found before our intended item
        for(uint i = 0; i<=pos; i++)
        {
            AdwTabPage* parent_page = ADW_TAB_PAGE(g_list_model_get_item(model->model, i));
            defer([parent_page]{ g_object_unref(parent_page); });
            OrionWebviewTabContainer* parent_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(parent_page));

            if(!orion_webview_tab_container_get_group_expanded(parent_container))
            {
                parents.push_back(orion_webview_tab_container_get_id(parent_container));
                uint x = i+1;
                for(; x<=pos; x++)
                {
                    AdwTabPage* page = ADW_TAB_PAGE(g_list_model_get_item(model->model, x));
                    defer([page]{ g_object_unref(page); });
                    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
                    orion::TabId parent_id = orion_webview_tab_container_get_parent_id(container);
                    auto it = std::find(parents.crbegin(), parents.crend(), parent_id);
                    if(it!=parents.crend())
                    {
                        parents.erase(it.base(), parents.cend());
                        parents.push_back(orion_webview_tab_container_get_id(container));
                        pos++;
                        if(pos>=len) return GTK_INVALID_LIST_POSITION;
                    }
                    else
                    {
                        break;
                    }
                }
                i=x-1;
                parents.clear();

            }
        }
    }

    return pos;
}

static bool
orion_tab_selection_model_get_item_expandable(OrionTabSelectionModel* model, uint pos)
{
    uint real_pos = orion_tab_selection_model_get_actual_pos(model, pos);
    guint n_items = g_list_model_get_n_items(model->model);

    if(!(model->show_nested_tabs) || real_pos >= n_items)
    {
        orion::log_verbose(
            "orion_tab_selection_model_get_item_expandable: show_nested_tabs=%d real_pos=%u pos=%u n_items=%u",
            model->show_nested_tabs, real_pos, pos, n_items
        );
        return false;
    }

    AdwTabPage* page = ADW_TAB_PAGE(g_list_model_get_item(model->model, real_pos));
    if(!page) return false;
    defer([page]{ g_object_unref(page); });
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));

    
    AdwTabPage* page_after = ADW_TAB_PAGE(g_list_model_get_item(model->model, real_pos+1));
    if(!page_after) return false;
    defer([page_after]{ g_object_unref(page_after); });
    OrionWebviewTabContainer* container_after = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page_after));
    
    return orion_webview_tab_container_get_parent_id(container_after) == orion_webview_tab_container_get_id(container);
}


static void
orion_tab_selection_model_emit_items_changed(OrionTabSelectionModel* model, guint pos, guint removed, guint added)
{
    for(uint i = 0; i<added; i++)
    {
        AdwTabPage* page = ADW_TAB_PAGE(g_list_model_get_item(G_LIST_MODEL(model), pos+i));
        if(!page) return;
        defer([page]{g_object_unref(page);});
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
        orion_webview_tab_container_set_nested_tabs_expandable(container, orion_tab_selection_model_get_item_expandable(model, pos+i));

        
        orion_webview_tab_container_set_indent(container, orion_tab_selection_model_get_item_indent(model, pos+i));
    }
    g_list_model_items_changed(G_LIST_MODEL(model), pos, removed, added);
}

static guint
orion_tab_selection_model_get_wrapped_pos_from_actual(OrionTabSelectionModel* model, guint pos)
{
    //translates position from real pos in underlying model to wrapped pos
    uint ret_val = pos;
    
    //calculation is only needed when nested tabs are to be shown
    if(model->show_nested_tabs)
    {
        uint len = g_list_model_get_n_items(model->model);
        if(pos>=len) return UINT_MAX;
        for(uint i = 0; i<pos; i++)
        {
            AdwTabPage* page = ADW_TAB_PAGE(g_list_model_get_item(model->model, i));
            defer([page]{ g_object_unref(page); });
            OrionWebviewTabContainer* parent_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
            if(!orion_webview_tab_container_get_group_expanded(parent_container))
            {

                std::vector<orion::TabId> parents = {orion_webview_tab_container_get_id(parent_container)};
                uint x = i+1;
                for(; x<pos; x++)
                {
                    AdwTabPage* page = ADW_TAB_PAGE(g_list_model_get_item(model->model, x));
                    defer([page]{ g_object_unref(page);});
                    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));

                    orion::TabId parent_id = orion_webview_tab_container_get_parent_id(container);
                    auto it = std::find(parents.crbegin(), parents.crend(), parent_id);
                    if(it!=parents.crend())
                    {
                        parents.erase(it.base(), parents.cend());
                        parents.push_back(orion_webview_tab_container_get_id(container));
                        ret_val--;
                    }
                    else
                    {
                        break;
                    }
                }
                i = x-1;
                parents.clear();
            }
        }
    }
    return ret_val;
}

static void
orion_tab_selection_model_on_item_notify_parent_id(OrionWebviewTabContent* content, GParamSpec* pspec, gpointer user_data)
{
    OrionTabSelectionModel* wrapping_model = ORION_TAB_SELECTION_MODEL(user_data);
    uint len = g_list_model_get_n_items(G_LIST_MODEL(user_data));
    orion_tab_selection_model_emit_items_changed(ORION_TAB_SELECTION_MODEL(user_data), 0, (wrapping_model->old_len), len);
    wrapping_model->old_len = len;
}

static void
orion_tab_selection_model_on_items_changed(GListModel* model, guint position, guint removed, guint added, gpointer user_data)
{
    OrionTabSelectionModel* wrapping_model = ORION_TAB_SELECTION_MODEL(user_data);
    uint len = g_list_model_get_n_items(G_LIST_MODEL(user_data));
    uint wrapped_pos = orion_tab_selection_model_get_wrapped_pos_from_actual(wrapping_model, position);
    if(wrapped_pos!=GTK_INVALID_LIST_POSITION)
    {
        if(wrapped_pos > 0)
        {
            orion_tab_selection_model_emit_items_changed(ORION_TAB_SELECTION_MODEL(user_data), wrapped_pos-1, ((wrapping_model->old_len)-wrapped_pos)+1, (len-wrapped_pos+1));
        }
        else
        {
            orion_tab_selection_model_emit_items_changed(ORION_TAB_SELECTION_MODEL(user_data), wrapped_pos, (wrapping_model->old_len)-wrapped_pos, len-wrapped_pos);
        }
    }
    else
    {
        orion_tab_selection_model_emit_items_changed(ORION_TAB_SELECTION_MODEL(user_data), 0, (wrapping_model->old_len), len);
        wrapped_pos = 0;
    }
    uint real_len = g_list_model_get_n_items(wrapping_model->model);
    for(uint i = wrapped_pos; i<real_len; i++)
    {
        AdwTabPage* page = ADW_TAB_PAGE(g_list_model_get_item(wrapping_model->model, i));
        defer([page]{g_object_unref(page);});
        GtkWidget* content = adw_tab_page_get_child(page);

        g_signal_handlers_disconnect_by_data(content, user_data);
        g_signal_connect_object(content, "notify::parent-id", G_CALLBACK(orion_tab_selection_model_on_item_notify_parent_id), user_data, G_CONNECT_DEFAULT);
    }
    wrapping_model->old_len = len;
}

static void
orion_tab_selection_model_on_selection_changed(GtkSelectionModel* model, guint position, guint n_items, gpointer user_data)
{
    gtk_selection_model_selection_changed(GTK_SELECTION_MODEL(user_data), 0, g_list_model_get_n_items(G_LIST_MODEL(user_data)));
}

//takes a reference to incoming data
void
orion_tab_selection_model_set_model(OrionTabSelectionModel* selection_model, GListModel* model)
{
    g_return_if_fail(ORION_IS_TAB_SELECTION_MODEL(selection_model));

    if(selection_model->model)
    {
        g_signal_handlers_disconnect_by_data(selection_model->model, selection_model);
    }
    selection_model->model = g_object_ref(model);
    selection_model->old_len = g_list_model_get_n_items(G_LIST_MODEL(selection_model));
    for(uint i = 0; i<selection_model->old_len; i++)
    {
        AdwTabPage* page = ADW_TAB_PAGE(g_list_model_get_item(G_LIST_MODEL(model), i));
        defer([page]{g_object_unref(page);});
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
        orion_webview_tab_container_set_nested_tabs_expandable(container, orion_tab_selection_model_get_item_expandable(selection_model, i));
        g_signal_connect_object(container, "notify::parent-id", G_CALLBACK(orion_tab_selection_model_on_item_notify_parent_id), selection_model, G_CONNECT_DEFAULT);
    }
    g_signal_connect(selection_model->model, "items-changed", G_CALLBACK(orion_tab_selection_model_on_items_changed), selection_model);
    g_signal_connect(selection_model->model, "selection-changed", G_CALLBACK(orion_tab_selection_model_on_selection_changed), selection_model);
}

void
orion_tab_selection_model_set_show_nested(OrionTabSelectionModel* selection_model, gboolean show_nested)
{
    g_return_if_fail(ORION_IS_TAB_SELECTION_MODEL(selection_model));

    if(!!(selection_model->show_nested_tabs) != !!(show_nested))
    {
        selection_model->show_nested_tabs = show_nested;
        g_object_notify_by_pspec(G_OBJECT(selection_model), obj_properties[PROP_SHOW_NESTED]); 
        orion_tab_selection_model_emit_items_changed(ORION_TAB_SELECTION_MODEL(selection_model), 0, selection_model->old_len, g_list_model_get_n_items(selection_model->model));    
        selection_model->old_len = g_list_model_get_n_items(selection_model->model); 
    }
}

void
orion_tab_selection_model_set_item_expanded(OrionTabSelectionModel* model, uint pos, bool item_expanded)
{
    uint real_pos = orion_tab_selection_model_get_actual_pos(model, pos);
    AdwTabPage* parent_page = ADW_TAB_PAGE(g_list_model_get_item(model->model, real_pos));
    defer([parent_page]{ g_object_unref(parent_page); });

    OrionWebviewTabContainer* parent_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(parent_page));
    if(orion_webview_tab_container_set_group_expanded(parent_container, item_expanded))
    {
        std::vector<std::pair<orion::TabId, bool>> parents = {{orion_webview_tab_container_get_id(parent_container),
            true}};
        uint count = 0;
        uint len = g_list_model_get_n_items(model->model);
        for(uint x = real_pos+1; x<len; x++)
        {
            AdwTabPage* page = ADW_TAB_PAGE(g_list_model_get_item(model->model, x));
            defer([page]{ g_object_unref(page);});
            OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));

            orion::TabId parent_id = orion_webview_tab_container_get_parent_id(container);
            auto it = std::find_if(parents.crbegin(), parents.crend(), [parent_id](std::pair<orion::TabId, bool> data)
            {
                return data.first == parent_id;
            });
            if(it!=parents.crend())
            {
                bool child_is_visible = it->second;
                if(it->second) count++;
                parents.erase(it.base(), parents.cend());
                parents.push_back({orion_webview_tab_container_get_id(container), child_is_visible && orion_webview_tab_container_get_group_expanded(container)});
            }
            else
            {
                break;
            }
        }


        if(item_expanded)
        {
            model->old_len+=count;
            orion_tab_selection_model_emit_items_changed(ORION_TAB_SELECTION_MODEL(model), pos, 1, count+1);
        }
        else
        {
            model->old_len-=count;
            orion_tab_selection_model_emit_items_changed(ORION_TAB_SELECTION_MODEL(model), pos, count+1, 1);
        }
        orion::log_verbose("orion_tab_selection_model_set_item_expanded: n_items=%i",
            g_list_model_get_n_items(G_LIST_MODEL(model))
        );
    }
}



uint
orion_tab_selection_model_get_item_indent(OrionTabSelectionModel* model, uint pos)
{
    if(!(model->show_nested_tabs))
    {
        return 0;
    }
    uint real_pos = orion_tab_selection_model_get_actual_pos(model, pos);
    if(real_pos >= g_list_model_get_n_items(model->model))
    {
        return 0;
    }
    else if(real_pos == 0)
    {
        return 0;
    }
    else
    {
        AdwTabPage* page = ADW_TAB_PAGE(g_list_model_get_item(model->model, real_pos));
        defer([page]{ g_object_unref(page); });
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));

        //TODO: find all parents
        orion::TabId parent_id = orion_webview_tab_container_get_parent_id(container);
        
        uint i = real_pos-1;
        uint ret_val = 0;
        while(parent_id!=orion::TabId(0) && i != 0)
        {
            AdwTabPage* page = ADW_TAB_PAGE(g_list_model_get_item(model->model, i));
            defer([page]{ g_object_unref(page); });
            OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
            if(orion_webview_tab_container_get_id(container)==parent_id)
            {
                ret_val++;
                parent_id = orion_webview_tab_container_get_parent_id(container);
            }
            i--;
        }

        AdwTabPage* first_page = ADW_TAB_PAGE(g_list_model_get_item(model->model, 0));
        defer([first_page]{ g_object_unref(first_page); });
        OrionWebviewTabContainer* first_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(first_page));

        if(orion_webview_tab_container_get_id(first_container)==parent_id)
        {
            ret_val++;
        }

        return ret_val;
    }
}

static void
orion_tab_selection_model_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    OrionTabSelectionModel* model = ORION_TAB_SELECTION_MODEL(object);

    switch (property_id) {
        case PROP_MODEL:
            orion_tab_selection_model_set_model(model, G_LIST_MODEL(g_value_get_object(value)));
            break;
        case PROP_SHOW_NESTED:
            orion_tab_selection_model_set_show_nested(model, g_value_get_boolean(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
orion_tab_selection_model_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    OrionTabSelectionModel* model = ORION_TAB_SELECTION_MODEL(object);

    switch (property_id) {
        case PROP_MODEL:
            g_value_set_object(value, model->model);
            break;
        case PROP_SHOW_NESTED:
            g_value_set_boolean(value, model->show_nested_tabs);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

OrionTabSelectionModel*
orion_tab_selection_model_new(GListModel* model)
{
    OrionTabSelectionModel* selection_model = (OrionTabSelectionModel*)g_object_new(ORION_TYPE_TAB_SELECTION_MODEL, "model", model, NULL);
    return selection_model;
}

void
orion_tab_selection_model_dispose(GObject* object)
{
    OrionTabSelectionModel* selection_model = ORION_TAB_SELECTION_MODEL(object);
    g_signal_handlers_disconnect_by_data(selection_model->model, selection_model);
    g_clear_object(&(selection_model->model));
    G_OBJECT_CLASS(orion_tab_selection_model_parent_class)->dispose(G_OBJECT(selection_model));
}

void
orion_tab_selection_model_init(OrionTabSelectionModel* item)
{
}

// static void
// orion_tab_selection_model_constructed(GObject* object)
// {
//     g_signal_connect(ORION_TAB_SELECTION_MODEL(object)->model, "items-changed", G_CALLBACK(orion_tab_selection_model_on_items_changed), object);
//     g_signal_connect(ORION_TAB_SELECTION_MODEL(object)->model, "selection_changed", G_CALLBACK(orion_tab_selection_model_on_selection_changed), object);
//     G_OBJECT_CLASS(orion_tab_selection_model_parent_class)->constructed(object);
// }

static void
orion_tab_selection_model_class_init(OrionTabSelectionModelClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = orion_tab_selection_model_dispose;

    object_class->set_property = orion_tab_selection_model_set_property;
    object_class->get_property = orion_tab_selection_model_get_property;
    obj_properties[PROP_MODEL] = 
        g_param_spec_object("model",
            "Model",
            "Underlying GListModel",
            G_TYPE_LIST_MODEL,
            GParamFlags(G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_SHOW_NESTED] = 
        g_param_spec_boolean("show-nested-tabs",
            "Show nested tabs",
            "Whether to show nested tabs and thus apply all grouping rules",
            1,
            GParamFlags(G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);

    //object_class->constructed = orion_tab_selection_model_constructed;
}

static gpointer
orion_tab_selection_model_get_item(GListModel* model, guint pos)
{
    OrionTabSelectionModel* selection_model = ORION_TAB_SELECTION_MODEL(model);
    return g_list_model_get_item(selection_model->model, orion_tab_selection_model_get_actual_pos(selection_model, pos));
}

static GType
orion_tab_selection_model_get_item_type(GListModel* model)
{
    return g_list_model_get_item_type(ORION_TAB_SELECTION_MODEL(model)->model);
}

static guint
orion_tab_selection_model_get_n_items(GListModel* model)
{
    //goes through the wrapped list model, searches for any collapsed groups and subtracts their count
    if(ORION_TAB_SELECTION_MODEL(model)->show_nested_tabs)
    {
        uint len = g_list_model_get_n_items(ORION_TAB_SELECTION_MODEL(model)->model);
        uint return_val = len;

        std::vector<orion::TabId> parents;

        for(uint i = 0; i<len; i++)
        {
            AdwTabPage* parent_page = ADW_TAB_PAGE(g_list_model_get_item(ORION_TAB_SELECTION_MODEL(model)->model, i));
            defer([parent_page]{ g_object_unref(parent_page); });
            OrionWebviewTabContainer* parent_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(parent_page));

            if(!orion_webview_tab_container_get_group_expanded(parent_container))
            {
                parents.push_back(orion_webview_tab_container_get_id(parent_container));
                uint x = i+1;
                for(; x<len; x++)
                {
                    AdwTabPage* page = ADW_TAB_PAGE(g_list_model_get_item(ORION_TAB_SELECTION_MODEL(model)->model, x));
                    defer([page]{ g_object_unref(page); });
                    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
                    orion::TabId parent_id = orion_webview_tab_container_get_parent_id(container);
                    auto it = std::find(parents.crbegin(), parents.crend(), parent_id);
                    if(it!=parents.crend())
                    {
                        parents.erase(it.base(), parents.cend());
                        parents.push_back(orion_webview_tab_container_get_id(container));
                        return_val--;
                    }
                    else
                    {
                        break;
                    }
                }
                i=x-1;
                parents.clear();

            }
        }
        return return_val;
    }
    else
    {
        return g_list_model_get_n_items(ORION_TAB_SELECTION_MODEL(model)->model);
    }
}

static void
orion_tab_selection_model_g_list_model_interface_init(GListModelInterface* iface)
{
    iface->get_item = orion_tab_selection_model_get_item;
    iface->get_item_type = orion_tab_selection_model_get_item_type;
    iface->get_n_items = orion_tab_selection_model_get_n_items;
}

static GtkBitset*
orion_tab_selection_model_get_selection_in_range(GtkSelectionModel* model, guint position, guint n_items)
{
    OrionTabSelectionModel* specific_model = ORION_TAB_SELECTION_MODEL(model);
    GtkSelectionModel* selection_model = GTK_SELECTION_MODEL(specific_model->model);
    GtkBitset* selection = gtk_selection_model_get_selection(selection_model);
    defer([selection]{ gtk_bitset_unref(selection); });

    if(!gtk_bitset_get_size(selection)) return gtk_bitset_new_empty();

    uint real_selection_pos = gtk_bitset_get_nth(selection, 0);
    uint wrapped_selection_pos = orion_tab_selection_model_get_wrapped_pos_from_actual(specific_model, real_selection_pos);
    if(wrapped_selection_pos>=position && wrapped_selection_pos<position+n_items)
    {
        return gtk_bitset_new_range(wrapped_selection_pos, 1);
    }
    else
    {
        return gtk_bitset_new_empty();
    }
}

static gboolean
orion_tab_selection_model_is_selected(GtkSelectionModel* model, guint position)
{
    OrionTabSelectionModel* specific_model = ORION_TAB_SELECTION_MODEL(model);
    GtkSelectionModel* selection_model = GTK_SELECTION_MODEL(specific_model->model);
    GtkBitset* selection = gtk_selection_model_get_selection(selection_model);
    defer([selection]{ gtk_bitset_unref(selection); });

    if(!gtk_bitset_get_size(selection)) return false;

    uint real_selection_pos = gtk_bitset_get_nth(selection, 0);
    uint wrapped_selection_pos = orion_tab_selection_model_get_wrapped_pos_from_actual(specific_model, real_selection_pos);
    return position==wrapped_selection_pos;
}

static gboolean
orion_tab_selection_model_select_all(GtkSelectionModel* model)
{
    return FALSE;
}

static gboolean
orion_tab_selection_model_select_item(GtkSelectionModel* model, guint position, gboolean unselect_rest)
{
    OrionTabSelectionModel* specific_model = ORION_TAB_SELECTION_MODEL(model);
    return gtk_selection_model_select_item(GTK_SELECTION_MODEL(specific_model->model), orion_tab_selection_model_get_actual_pos(specific_model, position), unselect_rest);
}

static gboolean
orion_tab_selection_model_select_range(GtkSelectionModel* model, guint position, guint n_items, gboolean unselect_rest)
{
    return FALSE;
}

static gboolean
orion_tab_selection_model_set_selection(GtkSelectionModel* model, GtkBitset* selected, GtkBitset* mask)
{
    return FALSE;
}

static gboolean
orion_tab_selection_model_unselect_all(GtkSelectionModel* model)
{
    return FALSE;
}

static gboolean
orion_tab_selection_model_unselect_item(GtkSelectionModel* model, guint position)
{
    return FALSE;
}

static gboolean
orion_tab_selection_model_unselect_range(GtkSelectionModel* model, guint position, guint n_items)
{
    return FALSE;
}

static void
orion_tab_selection_model_gtk_selection_model_interface_init(GtkSelectionModelInterface* iface)
{
    iface->get_selection_in_range = orion_tab_selection_model_get_selection_in_range;
    iface->is_selected = orion_tab_selection_model_is_selected;
    iface->select_all = orion_tab_selection_model_select_all;
    iface->select_item = orion_tab_selection_model_select_item;
    iface->select_range = orion_tab_selection_model_select_range;
    iface->set_selection = orion_tab_selection_model_set_selection;
    iface->unselect_all = orion_tab_selection_model_unselect_all;
    iface->unselect_item = orion_tab_selection_model_unselect_item;
    iface->unselect_range = orion_tab_selection_model_unselect_range;
}
