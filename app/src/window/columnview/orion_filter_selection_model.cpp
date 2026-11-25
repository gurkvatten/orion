#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "orion_filter_selection_model.h"
#include <common/src/defer.h>

//FIXME: do not change tab on collapsing a group

struct _OrionFilterSelectionModel
{
	GObject object;
    GListModel* model;
    GtkBitset* valid_items;
    GtkFilter* filter;
};


static void orion_filter_selection_model_g_list_model_interface_init(GListModelInterface* iface);
static void orion_filter_selection_model_gtk_selection_model_interface_init(GtkSelectionModelInterface* iface);
G_DEFINE_TYPE_WITH_CODE(
    OrionFilterSelectionModel,
    orion_filter_selection_model,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL, orion_filter_selection_model_g_list_model_interface_init)
    G_IMPLEMENT_INTERFACE(GTK_TYPE_SELECTION_MODEL, orion_filter_selection_model_gtk_selection_model_interface_init)
)

enum {
    PROP_0,
    PROP_MODEL,
    PROP_FILTER,
    PROP_N_ITEMS,
    N_PROPERTIES
};

static GParamSpec* obj_properties[N_PROPERTIES] = {NULL,};

static uint
orion_filter_selection_model_get_actual_pos_from_wrapped(OrionFilterSelectionModel* selection_model, uint wrapped_pos)
{
    if(wrapped_pos >= gtk_bitset_get_size(selection_model->valid_items)) return GTK_INVALID_LIST_POSITION;
    
    return gtk_bitset_get_nth(selection_model->valid_items, wrapped_pos);
}

static void
orion_filter_selection_model_on_items_changed(GListModel* model, guint position, guint removed, guint added, gpointer user_data)
{
    OrionFilterSelectionModel* specific_model = ORION_FILTER_SELECTION_MODEL(user_data);
    uint wrapped_removed_count = 0;
    if(removed>0)
    {
        wrapped_removed_count = gtk_bitset_get_size_in_range(specific_model->valid_items, position, position+removed-1);
    }

    gtk_bitset_splice(specific_model->valid_items, position, removed, added);
    
    //finds the index in the wrapping list corresponding to position in the wrapped list
    uint wrapped_pos = (position == 0) ? 0 : gtk_bitset_get_size_in_range(specific_model->valid_items, 0, position-1);

    //filters items newly added to the underlying model
    uint wrapped_added_count = 0;
    for(uint i = position; i<(added+position); i++)
    {
        gpointer item = g_list_model_get_item(model, i);
        defer([item]{ g_object_unref(item); });
        if(gtk_filter_match(specific_model->filter, item))
        {
            if(gtk_bitset_add(specific_model->valid_items, i))
            {
                wrapped_added_count++;
            }
        }
    }

    g_list_model_items_changed(G_LIST_MODEL(specific_model), wrapped_pos, wrapped_removed_count, wrapped_added_count);
    g_object_notify_by_pspec(G_OBJECT(specific_model), obj_properties[PROP_N_ITEMS]);
}

static void
orion_filter_selection_model_on_selection_changed(GtkSelectionModel* model, guint position, guint n_items, gpointer user_data)
{
    OrionFilterSelectionModel* specific_model = ORION_FILTER_SELECTION_MODEL(user_data);
    uint wrapped_pos = (position == 0) ? 0 : gtk_bitset_get_size_in_range(specific_model->valid_items, 0, position-1);
    uint wrapped_n_items = (n_items == 0) ? 0 : gtk_bitset_get_size_in_range(specific_model->valid_items, position, position+n_items-1);

    if(wrapped_n_items)
    {
        gtk_selection_model_selection_changed(GTK_SELECTION_MODEL(user_data), wrapped_pos, wrapped_n_items);
    }
}

static void
orion_filter_selection_model_run_filter(OrionFilterSelectionModel* selection_model)
{
    //temporary bitset that holds the new state until selection_model->valid_items can be replaced by it
    //using g_autoptr as tmp should be "stolen" and transfered into selection_model->valid_items, thus defer is dangerous
    g_autoptr(GtkBitset) tmp = gtk_bitset_new_empty();
    
    //runs the filter to find new state
    uint len = g_list_model_get_n_items(selection_model->model);
    if(selection_model->filter)
    {
        for(uint i = 0; i<len; i++)
        {
            gpointer item = g_list_model_get_item(selection_model->model, i);
            defer([item]{ g_object_unref(item); });
            if(gtk_filter_match(selection_model->filter, G_OBJECT(item)))
            {
                gtk_bitset_add(tmp, i);
            }
        }
    }
    else
    {
        gtk_bitset_add_range(tmp, 0, len);
    }
    //finds the items in new state, not in old state
    GtkBitset* added = gtk_bitset_copy(tmp);
    defer([added]{ gtk_bitset_unref(added); });
    gtk_bitset_subtract(added, selection_model->valid_items);

    //finds the items in old state, not in new state
    GtkBitset* removed = gtk_bitset_copy(selection_model->valid_items);
    defer([removed]{ gtk_bitset_unref(removed); });
    gtk_bitset_subtract(removed, tmp);


    //for now lazily runs g_list_model_items_changed one position at a time for both added and removed items
    for(uint i = 0; i<gtk_bitset_get_size(removed); i++)
    {
        uint wrapped_pos = gtk_bitset_get_nth(removed, i);
        uint wrapping_pos = (wrapped_pos == 0)?0:gtk_bitset_get_size_in_range(selection_model->valid_items, 0, wrapped_pos-1);
        if(gtk_bitset_remove(selection_model->valid_items, wrapped_pos))
        {
            g_list_model_items_changed(G_LIST_MODEL(selection_model), wrapping_pos, 1, 0);
        }
        else
        {
            orion::log_debug("orion_filter_selection_model_run_filter(): failed to find removed item in the wrapping bitset");
        }
    }
    
    for(uint i = 0; i<gtk_bitset_get_size(added); i++)
    {
        uint wrapped_pos = gtk_bitset_get_nth(added, i);
        uint wrapping_pos = wrapped_pos == 0?0:gtk_bitset_get_size_in_range(selection_model->valid_items, 0, wrapped_pos - 1);
        if(gtk_bitset_add(selection_model->valid_items, wrapped_pos))
        {
            g_list_model_items_changed(G_LIST_MODEL(selection_model), wrapping_pos, 0, 1);  
        }
        else
        {
            orion::log_debug("orion_filter_selection_model_run_filter(): value was somehow already a part of the valid bitset");
        }
    }

    g_clear_pointer(&selection_model->valid_items, gtk_bitset_unref);
    selection_model->valid_items = g_steal_pointer(&tmp);
    g_object_notify_by_pspec(G_OBJECT(selection_model), obj_properties[PROP_N_ITEMS]);
}

static void
orion_filter_selection_model_on_filter_changed(GtkFilter* filter, GtkFilterChange* change, gpointer user_data)
{
    OrionFilterSelectionModel* specific_model = ORION_FILTER_SELECTION_MODEL(user_data);
    orion_filter_selection_model_run_filter(specific_model);
}

//takes a reference to incoming data
void
orion_filter_selection_model_set_filter(OrionFilterSelectionModel* selection_model, GtkFilter* filter)
{
    if(selection_model->filter != filter)
    {
        if(selection_model->filter)
        {
            g_signal_handlers_disconnect_by_data(selection_model->filter, selection_model);
            g_clear_object(&selection_model->filter);
        }

        selection_model->filter = g_object_ref(filter);
        orion_filter_selection_model_run_filter(selection_model);
        g_signal_connect(selection_model->filter, "changed", G_CALLBACK(orion_filter_selection_model_on_filter_changed), selection_model);
        g_object_notify_by_pspec(G_OBJECT(selection_model), obj_properties[PROP_FILTER]);
    }
}

//takes a reference to incoming data
void
orion_filter_selection_model_set_model(OrionFilterSelectionModel* selection_model, GtkSelectionModel* model)
{
    if(selection_model->model != G_LIST_MODEL(model))
    {
        if(selection_model->model)
        {
            g_signal_handlers_disconnect_by_data(selection_model->model, selection_model);
            g_clear_object(&selection_model->model);
        }
        selection_model->model = G_LIST_MODEL(g_object_ref(model));


        
        uint prev_size = gtk_bitset_get_size(selection_model->valid_items);
        gtk_bitset_remove_all(selection_model->valid_items);

        uint len = g_list_model_get_n_items(selection_model->model);
        if(selection_model->filter)
        {
            for(uint i = 0; i<len; i++)
            {
                gpointer item = g_list_model_get_item(selection_model->model, i);
                defer([item]{ g_object_unref(item); });
                if(gtk_filter_match(selection_model->filter, G_OBJECT(item)))
                {
                    gtk_bitset_add(selection_model->valid_items, i);
                }
            }
        }
        else
        {
            gtk_bitset_add_range(selection_model->valid_items, 0, len);
        }
        g_list_model_items_changed(G_LIST_MODEL(selection_model), 0, prev_size, gtk_bitset_get_size(selection_model->valid_items));
        g_object_notify_by_pspec(G_OBJECT(selection_model), obj_properties[PROP_N_ITEMS]);

        g_signal_connect(selection_model->model, "items-changed", G_CALLBACK(orion_filter_selection_model_on_items_changed), selection_model);
        g_signal_connect(selection_model->model, "selection-changed", G_CALLBACK(orion_filter_selection_model_on_selection_changed), selection_model);
        g_object_notify_by_pspec(G_OBJECT(selection_model), obj_properties[PROP_MODEL]);
    }
}

static void
orion_filter_selection_model_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    OrionFilterSelectionModel* model = ORION_FILTER_SELECTION_MODEL(object);

    switch (property_id) {
        case PROP_MODEL:
            orion_filter_selection_model_set_model(model, GTK_SELECTION_MODEL(g_value_get_object(value)));
            break;
        case PROP_FILTER:
            orion_filter_selection_model_set_filter(model, GTK_FILTER(g_value_get_object(value)));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static guint orion_filter_selection_model_get_n_items(GListModel* model);

static void
orion_filter_selection_model_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    OrionFilterSelectionModel* model = ORION_FILTER_SELECTION_MODEL(object);

    switch (property_id) {
        case PROP_MODEL:
            g_value_set_object(value, model->model);
            break;
        case PROP_FILTER:
            g_value_set_object(value, model->filter);
            break;
        case PROP_N_ITEMS:
            g_value_set_uint(value, orion_filter_selection_model_get_n_items(G_LIST_MODEL(model)));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

OrionFilterSelectionModel*
orion_filter_selection_model_new(GtkSelectionModel* model)
{
    OrionFilterSelectionModel* selection_model = (OrionFilterSelectionModel*)g_object_new(ORION_TYPE_FILTER_SELECTION_MODEL, "model", model, NULL);
    return selection_model;
}

void
orion_filter_selection_model_dispose(GObject* object)
{
    OrionFilterSelectionModel* selection_model = ORION_FILTER_SELECTION_MODEL(object);
    if(selection_model->model)
    {
        g_signal_handlers_disconnect_by_data(selection_model->model, selection_model);
        g_clear_object(&(selection_model->model));
    }
    if(selection_model->filter)
    {
        g_signal_handlers_disconnect_by_data(selection_model->filter, selection_model);
        g_clear_object(&(selection_model->filter));
    }
    g_clear_pointer(&selection_model->valid_items, gtk_bitset_unref);
    G_OBJECT_CLASS(orion_filter_selection_model_parent_class)->dispose(G_OBJECT(selection_model));
}

void
orion_filter_selection_model_init(OrionFilterSelectionModel* model)
{
    model->valid_items = gtk_bitset_new_empty();
}

static void
orion_filter_selection_model_class_init(OrionFilterSelectionModelClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = orion_filter_selection_model_dispose;

    object_class->set_property = orion_filter_selection_model_set_property;
    object_class->get_property = orion_filter_selection_model_get_property;
    obj_properties[PROP_MODEL] = 
        g_param_spec_object("model",
            "Model",
            "Underlying GtkSelectionModel",
            GTK_TYPE_SELECTION_MODEL,
            GParamFlags(G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_FILTER] = 
        g_param_spec_object("filter",
            "Filter",
            "Item filter",
            GTK_TYPE_FILTER,
            GParamFlags(G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_N_ITEMS] = 
        g_param_spec_uint("n-items",
            "n-items",
            "Number of items",
            0,
            GTK_INVALID_LIST_POSITION-1,
            0,
            GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);

}

static gpointer
orion_filter_selection_model_get_item(GListModel* model, guint pos)
{
    OrionFilterSelectionModel* selection_model = ORION_FILTER_SELECTION_MODEL(model);
    if(pos >= gtk_bitset_get_size(selection_model->valid_items)) return NULL;
    return g_list_model_get_item(selection_model->model, gtk_bitset_get_nth(selection_model->valid_items, pos));
}

static GType
orion_filter_selection_model_get_item_type(GListModel* model)
{
    return g_list_model_get_item_type(ORION_FILTER_SELECTION_MODEL(model)->model);
}

static guint
orion_filter_selection_model_get_n_items(GListModel* model)
{
    return gtk_bitset_get_size(ORION_FILTER_SELECTION_MODEL(model)->valid_items);
}

static void
orion_filter_selection_model_g_list_model_interface_init(GListModelInterface* iface)
{
    iface->get_item = orion_filter_selection_model_get_item;
    iface->get_item_type = orion_filter_selection_model_get_item_type;
    iface->get_n_items = orion_filter_selection_model_get_n_items;
}

static GtkBitset*
orion_filter_selection_model_get_selection_in_range(GtkSelectionModel* model, guint position, guint n_items)
{
    OrionFilterSelectionModel* specific_model = ORION_FILTER_SELECTION_MODEL(model);

    GtkSelectionModel* selection_model = GTK_SELECTION_MODEL(specific_model->model);
    GtkBitset* selection = gtk_selection_model_get_selection(selection_model);
    defer([selection]{ gtk_bitset_unref(selection); });


    uint64_t valid_items_size = gtk_bitset_get_size(specific_model->valid_items);
    uint64_t end = std::min((uint64_t)position + n_items, valid_items_size);
    GtkBitset* result = gtk_bitset_new_empty();
    for(uint i = position; i<end; i++)
    {
        if(gtk_bitset_contains(selection, gtk_bitset_get_nth(specific_model->valid_items, i)))
        {
            gtk_bitset_add(result, i);
        }
    }
    return result;
}

static gboolean
orion_filter_selection_model_is_selected(GtkSelectionModel* model, guint position)
{
    OrionFilterSelectionModel* specific_model = ORION_FILTER_SELECTION_MODEL(model);
    GtkSelectionModel* selection_model = GTK_SELECTION_MODEL(specific_model->model);
    GtkBitset* selection = gtk_selection_model_get_selection(selection_model);
    defer([selection]{ gtk_bitset_unref(selection); });

    if(!gtk_bitset_get_size(selection)) return false;
    if(position >= gtk_bitset_get_size(specific_model->valid_items)) return false;

    uint wrapped_index = gtk_bitset_get_nth(specific_model->valid_items, position);
    return gtk_bitset_contains(selection, wrapped_index);
}

static gboolean
orion_filter_selection_model_select_all(GtkSelectionModel* model)
{
    return FALSE;
}

//NOTE: these are dummy methods used for intercepting item selection in a gtk listview,
//they emit a custom signal to inform us that we should change the current window
static gboolean
orion_filter_selection_model_select_item(GtkSelectionModel* model, guint position, gboolean unselect_rest)
{
    OrionFilterSelectionModel* specific_model = ORION_FILTER_SELECTION_MODEL(model);
    return gtk_selection_model_select_item(GTK_SELECTION_MODEL(specific_model->model), orion_filter_selection_model_get_actual_pos_from_wrapped(specific_model, position), unselect_rest);
}

static gboolean
orion_filter_selection_model_select_range(GtkSelectionModel* model, guint position, guint n_items, gboolean unselect_rest)
{
    return FALSE;
}

static gboolean
orion_filter_selection_model_set_selection(GtkSelectionModel* model, GtkBitset* selected, GtkBitset* mask)
{
    return FALSE;
}

static gboolean
orion_filter_selection_model_unselect_all(GtkSelectionModel* model)
{
    return FALSE;
}

static gboolean
orion_filter_selection_model_unselect_item(GtkSelectionModel* model, guint position)
{
    return FALSE;
}

static gboolean
orion_filter_selection_model_unselect_range(GtkSelectionModel* model, guint position, guint n_items)
{
    return FALSE;
}

static void
orion_filter_selection_model_gtk_selection_model_interface_init(GtkSelectionModelInterface* iface)
{
    iface->get_selection_in_range = orion_filter_selection_model_get_selection_in_range;
    iface->is_selected = orion_filter_selection_model_is_selected;
    iface->select_all = orion_filter_selection_model_select_all;
    iface->select_item = orion_filter_selection_model_select_item;
    iface->select_range = orion_filter_selection_model_select_range;
    iface->set_selection = orion_filter_selection_model_set_selection;
    iface->unselect_all = orion_filter_selection_model_unselect_all;
    iface->unselect_item = orion_filter_selection_model_unselect_item;
    iface->unselect_range = orion_filter_selection_model_unselect_range;
}
