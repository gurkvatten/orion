#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "orion_window_selection_model.h"

struct _OrionWindowSelectionModel
{
	GObject object;
    GListModel* model;
    guint pos;
};

static void orion_window_selection_model_g_list_model_interface_init(GListModelInterface* iface);
static void orion_window_selection_model_gtk_selection_model_interface_init(GtkSelectionModelInterface* iface);
G_DEFINE_TYPE_WITH_CODE(
    OrionWindowSelectionModel,
    orion_window_selection_model,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL, orion_window_selection_model_g_list_model_interface_init)
    G_IMPLEMENT_INTERFACE(GTK_TYPE_SELECTION_MODEL, orion_window_selection_model_gtk_selection_model_interface_init)
)

enum {
    SIGNAL_WINDOW_SELECTED,
    N_SIGNALS
};

static guint obj_signals[N_SIGNALS] = {0};

enum {
    PROP_0,
    PROP_MODEL,
    N_PROPERTIES
};

static GParamSpec* obj_properties[N_PROPERTIES] = {NULL,};

static void
orion_window_selection_model_on_items_changed(GListModel* model, guint position, guint removed, guint added, gpointer user_data)
{
    g_signal_emit(user_data, obj_signals[SIGNAL_WINDOW_SELECTED], 0, position, removed, added);
}

//takes a reference to incoming data
void
orion_window_selection_model_set_model(OrionWindowSelectionModel* selection_model, GListModel* model)
{
    g_return_if_fail(ORION_IS_WINDOW_SELECTION_MODEL(selection_model));

    if(selection_model->model)
    {
        g_signal_handlers_disconnect_by_data(selection_model->model, selection_model);
    }
    g_object_ref(model);
    selection_model->model = model;
    g_signal_connect(selection_model->model, "items-changed", G_CALLBACK(orion_window_selection_model_on_items_changed), selection_model);
}

static void
orion_window_selection_model_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    //using a C style cast because set methods do the dynamic type check instead
    OrionWindowSelectionModel* model = ORION_WINDOW_SELECTION_MODEL(object);

    switch (property_id) {
        case PROP_MODEL:
            orion_window_selection_model_set_model(model, G_LIST_MODEL(g_value_get_object(value)));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
orion_window_selection_model_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    OrionWindowSelectionModel* model = ORION_WINDOW_SELECTION_MODEL(object);

    switch (property_id) {
        case PROP_MODEL:
            g_value_set_object(value, model->model);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

OrionWindowSelectionModel*
orion_window_selection_model_new(GListModel* model)
{
    OrionWindowSelectionModel* selection_model = (OrionWindowSelectionModel*)g_object_new(ORION_TYPE_WINDOW_SELECTION_MODEL, "model", model, NULL);
    return selection_model;
}

void
orion_window_selection_model_dispose(GObject* object)
{
    OrionWindowSelectionModel* selection_model = ORION_WINDOW_SELECTION_MODEL(object);
    g_signal_handlers_disconnect_by_data(selection_model->model, selection_model);
    g_signal_handlers_disconnect_matched(selection_model, G_SIGNAL_MATCH_ID, obj_signals[SIGNAL_WINDOW_SELECTED], 0, NULL, NULL, NULL);
    g_clear_object(&(selection_model->model));
    G_OBJECT_CLASS(orion_window_selection_model_parent_class)->dispose(G_OBJECT(selection_model));
}

void
orion_window_selection_model_init(OrionWindowSelectionModel* item)
{
}

static void
orion_window_selection_model_constructed(GObject* object)
{
    g_signal_connect(ORION_WINDOW_SELECTION_MODEL(object)->model, "items-changed", G_CALLBACK(orion_window_selection_model_on_items_changed), object);
    G_OBJECT_CLASS(orion_window_selection_model_parent_class)->constructed(object);
}

static void
orion_window_selection_model_class_init(OrionWindowSelectionModelClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = orion_window_selection_model_dispose;

    object_class->set_property = orion_window_selection_model_set_property;
    object_class->get_property = orion_window_selection_model_get_property;
    obj_properties[PROP_MODEL] = 
        g_param_spec_object("model",
            "Model",
            "Underlying GListModel",
            G_TYPE_LIST_MODEL,
            GParamFlags(G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);

    object_class->constructed = orion_window_selection_model_constructed;

    obj_signals[SIGNAL_WINDOW_SELECTED] = 
        g_signal_new(
            "window-selected",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            G_TYPE_UINT);
}

static gpointer
orion_window_selection_model_get_item(GListModel* model, guint pos)
{
    return g_list_model_get_item(ORION_WINDOW_SELECTION_MODEL(model)->model, pos);
}

static GType
orion_window_selection_model_get_item_type(GListModel* model)
{
    return g_list_model_get_item_type(ORION_WINDOW_SELECTION_MODEL(model)->model);
}

static guint
orion_window_selection_model_get_n_items(GListModel* model)
{
    return g_list_model_get_n_items(ORION_WINDOW_SELECTION_MODEL(model)->model);
}

static void
orion_window_selection_model_g_list_model_interface_init(GListModelInterface* iface)
{
    iface->get_item = orion_window_selection_model_get_item;
    iface->get_item_type = orion_window_selection_model_get_item_type;
    iface->get_n_items = orion_window_selection_model_get_n_items;
}

static GtkBitset*
orion_window_selection_model_get_selection_in_range(GtkSelectionModel* model, guint position, guint n_items)
{
    OrionWindowSelectionModel* specific_model = ORION_WINDOW_SELECTION_MODEL(model);
    if((specific_model->pos > position) && (specific_model->pos < position + n_items))
    {
        return gtk_bitset_new_range(specific_model->pos, 1);
    }
    else
    {
        return gtk_bitset_new_empty();
    }
}

static gboolean
orion_window_selection_model_is_selected(GtkSelectionModel* model, guint position)
{
    OrionWindowSelectionModel* specific_model = ORION_WINDOW_SELECTION_MODEL(model);
    return specific_model->pos == position;
}

static gboolean
orion_window_selection_model_select_all(GtkSelectionModel* model)
{
    return FALSE;
}

//NOTE: these are dummy methods used for intercepting item selection in a gtk listview,
//they emit a custom signal to inform us that we should change the current window
static gboolean
orion_window_selection_model_select_item(GtkSelectionModel* model, guint position, gboolean unselect_rest)
{
    g_signal_emit(model, obj_signals[SIGNAL_WINDOW_SELECTED], 0, position);
    return TRUE;
}

static gboolean
orion_window_selection_model_select_range(GtkSelectionModel* model, guint position, guint n_items, gboolean unselect_rest)
{
    if(n_items == 1)
    {
        g_signal_emit(model, obj_signals[SIGNAL_WINDOW_SELECTED], 0, position);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static gboolean
orion_window_selection_model_set_selection(GtkSelectionModel* model, GtkBitset* selected, GtkBitset* mask)
{
    return FALSE;
}

static gboolean
orion_window_selection_model_unselect_all(GtkSelectionModel* model)
{
    return FALSE;
}

static gboolean
orion_window_selection_model_unselect_item(GtkSelectionModel* model, guint position)
{
    return FALSE;
}

static gboolean
orion_window_selection_model_unselect_range(GtkSelectionModel* model, guint position, guint n_items)
{
    return FALSE;
}

static void
orion_window_selection_model_gtk_selection_model_interface_init(GtkSelectionModelInterface* iface)
{
    iface->get_selection_in_range = orion_window_selection_model_get_selection_in_range;
    iface->is_selected = orion_window_selection_model_is_selected;
    iface->select_all = orion_window_selection_model_select_all;
    iface->select_item = orion_window_selection_model_select_item;
    iface->select_range = orion_window_selection_model_select_range;
    iface->set_selection = orion_window_selection_model_set_selection;
    iface->unselect_all = orion_window_selection_model_unselect_all;
    iface->unselect_item = orion_window_selection_model_unselect_item;
    iface->unselect_range = orion_window_selection_model_unselect_range;
}

gboolean
orion_window_selection_model_actual_select_item(OrionWindowSelectionModel* model, guint position)
{
    guint len = g_list_model_get_n_items(ORION_WINDOW_SELECTION_MODEL(model)->model);
    //NOTE: this should possibly be using 
    if(position < len && model->pos != position)
    {
        guint old_pos = model->pos;
        model->pos = position;
        gtk_selection_model_selection_changed(GTK_SELECTION_MODEL(model), std::min(position, old_pos), (position>old_pos?position-old_pos:old_pos-position));
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}