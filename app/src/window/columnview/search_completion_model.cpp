#define ENABLE_VERBOSE_LOGS 0

#include "search_completion_model.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <common/src/defer.h>

#include "data_item/wrapping_item.h"
#include "data_item/g_string_item.h"

namespace {
    struct CppWrapper {
        std::vector<std::pair<GListModel*, std::string>> models;
    };
}

struct _SearchCompletionModel
{
	GObject object;
    CppWrapper cpp_data;
};

static void search_completion_model_g_list_model_interface_init(GListModelInterface* iface);
G_DEFINE_TYPE_WITH_CODE(
    SearchCompletionModel,
    search_completion_model,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL, search_completion_model_g_list_model_interface_init)
)

static uint
search_completion_model_get_n_previous_items(SearchCompletionModel* self, GListModel* section)
{
    uint n_prev_items = 0;
    auto it = self->cpp_data.models.begin();
    for(; it->first != section; ++it)
    {
        n_prev_items += g_list_model_get_n_items(it->first);
    }
    if(it != self->cpp_data.models.end())
    {
        return n_prev_items;
    }
    else
    {
        g_assert_not_reached();
        return GTK_INVALID_LIST_POSITION;
    }
}

static void
on_section_items_changed(GListModel* section, guint position, guint removed, guint added, GListModel* self)
{
    g_list_model_items_changed(self, position+search_completion_model_get_n_previous_items(APP_SEARCH_COMPLETION_MODEL(self), section), removed, added);
}

void
search_completion_model_append_section(SearchCompletionModel* self, GListModel* section, std::string name)
{
    self->cpp_data.models.push_back({g_object_ref(section), name});
    g_list_model_items_changed(G_LIST_MODEL(self), search_completion_model_get_n_previous_items(self, section), 0, g_list_model_get_n_items(section));
    g_signal_connect(section, "items-changed", G_CALLBACK(on_section_items_changed), self);
}

static void
search_completion_model_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    //SearchCompletionModel* model = APP_SEARCH_COMPLETION_MODEL(object);

    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
search_completion_model_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    //SearchCompletionModel* model = APP_SEARCH_COMPLETION_MODEL(object);

    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

void
search_completion_model_dispose(GObject* object)
{
    //SearchCompletionModel* selection_model = APP_SEARCH_COMPLETION_MODEL(object);
    G_OBJECT_CLASS(search_completion_model_parent_class)->dispose(object);
}

void
search_completion_model_finalize(GObject* object)
{
    SearchCompletionModel* selection_model = APP_SEARCH_COMPLETION_MODEL(object);
    G_OBJECT_CLASS(search_completion_model_parent_class)->finalize(object);
    std::destroy_at(&selection_model->cpp_data);
}


void
search_completion_model_init(SearchCompletionModel* model)
{
    std::construct_at(&model->cpp_data);
}

static void
search_completion_model_class_init(SearchCompletionModelClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = search_completion_model_dispose;
    object_class->finalize = search_completion_model_finalize;

    object_class->set_property = search_completion_model_set_property;
    object_class->get_property = search_completion_model_get_property;
}

static gpointer
search_completion_model_get_item(GListModel* model, guint pos)
{
    uint n_previous_items = 0;
    uint pos_in_individual_model = GTK_INVALID_LIST_POSITION;
    SearchCompletionModel* specific_model = APP_SEARCH_COMPLETION_MODEL(model);
    gpointer item = NULL;
    std::string header_name;
    for(auto& pair: specific_model->cpp_data.models)
    {
        uint n_items = g_list_model_get_n_items(pair.first);
        if(pos - n_previous_items < n_items)
        {
            item = g_list_model_get_item(pair.first, pos-n_previous_items);
            pos_in_individual_model = pos-n_previous_items;
            header_name = pair.second;
            break;
        }
        n_previous_items += n_items;
    }
    if(!item)
    {
        g_assert_not_reached();
        return NULL;
    }
    else
    {
        if(pos_in_individual_model)
        {
            //TODO: FIXME: correct wrapper_data
            return wrapping_item_new(G_OBJECT(item), G_OBJECT(gtk_string_object_new(header_name.c_str())));
        }
        else
        {
            return wrapping_item_new(G_OBJECT(item), NULL);
        }
        return item;
    }
}

static GType
search_completion_model_get_item_type(GListModel* model)
{
    return APP_TYPE_WRAPPING_ITEM;
}

static guint
search_completion_model_get_n_items(GListModel* model)
{
    uint n_items = 0;
    SearchCompletionModel* specific_model = APP_SEARCH_COMPLETION_MODEL(model);
    for(auto& pair: specific_model->cpp_data.models)
    {
        n_items += g_list_model_get_n_items(pair.first);
    }
    return n_items;
}

static void
search_completion_model_g_list_model_interface_init(GListModelInterface* iface)
{
    iface->get_item = search_completion_model_get_item;
    iface->get_item_type = search_completion_model_get_item_type;
    iface->get_n_items = search_completion_model_get_n_items;
}