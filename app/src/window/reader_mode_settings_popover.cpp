#include <gtk/gtk.h>
#include <adwaita.h>

#include "app.h"

#include <core/settings.h>

#include <common/src/defer.h>

#include "reader_mode_settings_popover.h"

#include "util/action_util.h"
#include "util/css_util.h"

#include "window/main_window/main_window.h"

#include "data_item/int64_item.h"

static constexpr std::string_view menu_path_separator = " → ";
static constexpr size_t menu_path_separator_length = menu_path_separator.length();

template <auto Min, auto Max, auto Step>
constexpr auto make_range_array() {
    static_assert(Max >= Min, "Max must be greater than or equal to Min");
    static_assert(Step > 0, "Step must be greater than zero");

    constexpr std::size_t size = (Max - Min) / Step + 1;
    using T = decltype(Min); 
    
    std::array<T, size> arr{};
    
    for (std::size_t i = 0; i < size; ++i) {
        arr[i] = Min + (i * Step);
    }
    
    return arr;
}

static constexpr int64_t min_paper_width = 800;
static constexpr int64_t max_paper_width = 1300;
static constexpr int64_t paper_width_step = 250;
static constexpr auto paper_width_possible_values = make_range_array<min_paper_width, max_paper_width, paper_width_step>();



namespace {
    struct CppWrapper {
    };
}
struct _ReaderModeSettingsPopover
{
	GtkPopover parent;

    AdwToggleGroup* background_type_toggle_group;
    GtkListView* font_list_view;
    GtkScale* paper_width_scale;
    AdwToggleGroup* text_justify_toggle_group;
    AdwToggleGroup* border_shadow_toggle_group;

    CppWrapper cpp_data;
};

G_DEFINE_TYPE(ReaderModeSettingsPopover, reader_mode_settings_popover, GTK_TYPE_POPOVER)

enum {
    PROP_0,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS] = {NULL, };

GtkPopover*
reader_mode_settings_popover_new()
{
	return GTK_POPOVER(g_object_new(APP_TYPE_READER_MODE_SETTINGS_POPOVER, NULL));
}

static void
font_row_bind(GtkSignalListItemFactory* factory, GObject* object, gpointer user_data)
{
    GtkListItem* list_item = GTK_LIST_ITEM(object);
    Int64Item* item = APP_INT64_ITEM(gtk_list_item_get_item(list_item));
    int64_t value = int64_item_get_value(item);
    auto font = orion::settings::Font(value);

    std::string name = "System";
    switch(font)
    {
        case orion::settings::Font::Merriweather:
        case orion::settings::Font::Charter:
        case orion::settings::Font::Gelasio:
        case orion::settings::Font::Ubuntu:
        case orion::settings::Font::Vollkorn:
            name = rfl::enum_to_string(font);
            break;
        case orion::settings::Font::TeXGyrePagella:
            name = "TeX Gyre Pagella";
            break;
        case orion::settings::Font::NewYork:
            name = "New York";
            break;
        case orion::settings::Font::LiberationSerif:
            name = "Liberation Serif";
            break;
        case orion::settings::Font::Dyslexic:
            name = "Dyslexic";
            break;
        case orion::settings::Font::not_set:
        case orion::settings::Font::System:
            name = "System";
            break;
    }

    // PangoFontMap* map = pango_cairo_font_map_get_default();
    // gtk_list_item_set_selectable(list_item, pango_font_map_get_family(map, orion::font_name_to_css_font_family(font).c_str()) ? TRUE : FALSE);

    GtkWidget* label = gtk_label_new(name.c_str());
    gtk_widget_add_css_class(label, orion::font_name_to_css_class_name(font).c_str());

    gtk_list_item_set_child(list_item, label);
}

static void
on_background_type_toggle_group_changed(AdwToggleGroup* self, GParamSpec* pspec, ReaderModeSettingsPopover* popover)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(popover)));
    if(!win) return;

    guint pos = adw_toggle_group_get_active(self);
    orion::settings::ReaderModeBackgroundType type{};
    if(pos == 0)
    {
        type = orion::settings::ReaderModeBackgroundType::white;
    }
    else if(pos == 1)
    {
        type = orion::settings::ReaderModeBackgroundType::sepia;
    }
    else if(pos == 2)
    {
        type = orion::settings::ReaderModeBackgroundType::gray;
    }
    else if(pos == 3)
    {
        type = orion::settings::ReaderModeBackgroundType::night;
    }

    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::reader_mode::background_type, type,
        [](orion::SetSettingReply<orion::settings::ReaderModeBackgroundType> reply){
            VERIFY_LOG(reply.err);
        });
}

static void
font_selection_changed(GtkSingleSelection* self, guint position, guint n_items, ReaderModeSettingsPopover* popover)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(popover)));
    if(!win) return;

    Int64Item* item = APP_INT64_ITEM(gtk_single_selection_get_selected_item(self));
    if(!item) return;

    orion::settings::Font font = orion::settings::Font(int64_item_get_value(item));
    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::reader_mode::font_name, font,
        [](orion::SetSettingReply<orion::settings::Font> reply){
            VERIFY_LOG(reply.err);
        });
}

static void
on_paper_width_scale_value_changed(GtkAdjustment* adjustment, ReaderModeSettingsPopover* popover)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(popover)));
    if(!win) return;

    double val = gtk_adjustment_get_value(adjustment);
    size_t index = std::clamp(size_t(std::round(val)), size_t(0), size_t(paper_width_possible_values.size()-1));

    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::reader_mode::document_width, paper_width_possible_values[index],
        [](orion::SetSettingReply<int64_t> reply){
            VERIFY_LOG(reply.err);
        });
}

static void
on_text_justify_toggle_group_changed(AdwToggleGroup* self, GParamSpec* pspec, ReaderModeSettingsPopover* popover)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(popover)));
    if(!win) return;

    guint pos = adw_toggle_group_get_active(self);
    orion::settings::TextAlignment alignment{};
    if(pos == 0)
    {
        alignment = orion::settings::TextAlignment::left;
    }
    else
    {
        alignment = orion::settings::TextAlignment::justify;
    }

    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::reader_mode::text_alignment, alignment,
        [](orion::SetSettingReply<orion::settings::TextAlignment> reply){
            VERIFY_LOG(reply.err);
        });
}

static void
on_border_shadow_toggle_group_changed(AdwToggleGroup* self, GParamSpec* pspec, ReaderModeSettingsPopover* popover)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(popover)));
    if(!win) return;

    guint pos = adw_toggle_group_get_active(self);
    orion::settings::ReaderModeBorderShadowEnabled enabled{};
    if(pos == 0)
    {
        enabled = orion::settings::ReaderModeBorderShadowEnabled::off;
    }
    else
    {
        enabled = orion::settings::ReaderModeBorderShadowEnabled::on;
    }

    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::reader_mode::border_shadow_enabled, enabled,
        [](orion::SetSettingReply<orion::settings::ReaderModeBorderShadowEnabled> reply){
            VERIFY_LOG(reply.err);
        });
}

static void
reader_mode_settings_popover_init(ReaderModeSettingsPopover* popover)
{
    std::construct_at(&popover->cpp_data);
	gtk_widget_init_template(GTK_WIDGET(popover));

    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "bind", G_CALLBACK(font_row_bind), NULL);
    gtk_list_view_set_factory(popover->font_list_view, factory);


    GListStore* store = g_list_store_new(APP_TYPE_INT64_ITEM);
    auto font_name_vals = enchantum::values<orion::settings::Font>;
    for(orion::settings::Font val : font_name_vals)
    {
        if(val != orion::settings::Font::not_set)
        {
            Int64Item* item = int64_item_new(int64_t(val));
            g_list_store_append(store, item);
            g_object_unref(item);
        }
    }
    GtkSingleSelection* selection = gtk_single_selection_new(G_LIST_MODEL(store));
    g_signal_connect(selection, "selection-changed", G_CALLBACK(font_selection_changed), popover);
    gtk_list_view_set_model(popover->font_list_view, GTK_SELECTION_MODEL(selection));

    gtk_widget_set_font_map(GTK_WIDGET(popover), orion_app_get_reader_mode_font_map());
}

static void
reader_mode_settings_popover_dispose(GObject* object)
{
    //ReaderModeSettingsPopover* self = APP_READER_MODE_SETTINGS_POPOVER(object);
    G_OBJECT_CLASS(reader_mode_settings_popover_parent_class)->dispose(object);
}

static void
reader_mode_settings_popover_finalize(GObject* object)
{
    ReaderModeSettingsPopover* self = APP_READER_MODE_SETTINGS_POPOVER(object);
    std::destroy_at(&self->cpp_data);
    G_OBJECT_CLASS(reader_mode_settings_popover_parent_class)->finalize(object);
}

static void
reader_mode_settings_popover_map(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(reader_mode_settings_popover_parent_class)->map(widget);

    ReaderModeSettingsPopover* self = APP_READER_MODE_SETTINGS_POPOVER(widget);

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(widget));
    if(!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    orion::gtk::WeakRefWrapper self_weak(self);

    global_core_api->get_setting(profile_id, orion::settings::reader_mode::background_type, [self_weak](orion::GetSettingReply<orion::settings::ReaderModeBackgroundType> reply){
        VERIFY_LOG(reply.err);

        auto self_strong = self_weak.lock();
        if(!self_strong) return;
        ReaderModeSettingsPopover* self = APP_READER_MODE_SETTINGS_POPOVER(self_strong.get());

        uint pos = 0;
        switch(reply.value)
        {
            case orion::settings::ReaderModeBackgroundType::not_set:
            case orion::settings::ReaderModeBackgroundType::white:
                pos = 0;
                break;
            case orion::settings::ReaderModeBackgroundType::sepia:
                pos = 1;
                break;
            case orion::settings::ReaderModeBackgroundType::gray:
                pos = 2;
                break;
            case orion::settings::ReaderModeBackgroundType::night:
                pos = 3;
                break;
        }
        g_signal_handlers_block_by_func(self->background_type_toggle_group, (gpointer)on_background_type_toggle_group_changed, self);
        adw_toggle_group_set_active(self->background_type_toggle_group, pos);
        g_signal_handlers_unblock_by_func(self->background_type_toggle_group, (gpointer)on_background_type_toggle_group_changed, self);
    });

    global_core_api->get_setting(profile_id, orion::settings::reader_mode::font_name, [self_weak](orion::GetSettingReply<orion::settings::Font> reply){
        VERIFY_LOG(reply.err);

        auto self_strong = self_weak.lock();
        if(!self_strong) return;
        ReaderModeSettingsPopover* self = APP_READER_MODE_SETTINGS_POPOVER(self_strong.get());

        GtkSelectionModel* selection_model = gtk_list_view_get_model(self->font_list_view);
        
        GListModel* model = G_LIST_MODEL(selection_model);
        uint n_items = g_list_model_get_n_items(model);
        for(uint i = 0; i<n_items; i++)
        {
            Int64Item* item = APP_INT64_ITEM(g_list_model_get_item(model, i));
            defer([item]{ g_object_unref(item); });
            if(orion::settings::Font(int64_item_get_value(item)) == reply.value)
            {
                g_signal_handlers_block_by_func(selection_model, (gpointer)font_selection_changed, self);
                gtk_single_selection_set_selected(GTK_SINGLE_SELECTION(model), i);
                g_signal_handlers_unblock_by_func(selection_model, (gpointer)font_selection_changed, self);
                break;
            }
        }
    });

    global_core_api->get_setting(profile_id, orion::settings::reader_mode::document_width, [self_weak](orion::GetSettingReply<int64_t> reply){
        VERIFY_LOG(reply.err);

        auto self_strong = self_weak.lock();
        if(!self_strong) return;
        ReaderModeSettingsPopover* self = APP_READER_MODE_SETTINGS_POPOVER(self_strong.get());

        float val = std::round(std::clamp((float(reply.value-min_paper_width))/paper_width_step, 0.0f, (float)(paper_width_possible_values.size()-1)));

        GtkAdjustment* adjustment = gtk_range_get_adjustment(GTK_RANGE(self->paper_width_scale));
        g_signal_handlers_block_by_func(adjustment, (gpointer)on_paper_width_scale_value_changed, self);
        gtk_adjustment_set_value(adjustment, val);
        g_signal_handlers_unblock_by_func(adjustment, (gpointer)on_paper_width_scale_value_changed, self);
    });

    global_core_api->get_setting(profile_id, orion::settings::reader_mode::text_alignment, [self_weak](orion::GetSettingReply<orion::settings::TextAlignment> reply){
        VERIFY_LOG(reply.err);

        auto self_strong = self_weak.lock();
        if(!self_strong) return;
        ReaderModeSettingsPopover* self = APP_READER_MODE_SETTINGS_POPOVER(self_strong.get());

        g_signal_handlers_block_by_func(self->text_justify_toggle_group, (gpointer)on_text_justify_toggle_group_changed, self);
        adw_toggle_group_set_active(self->text_justify_toggle_group, reply.value == orion::settings::TextAlignment::justify ? 1 : 0);
        g_signal_handlers_unblock_by_func(self->text_justify_toggle_group, (gpointer)on_text_justify_toggle_group_changed, self);
    });

    global_core_api->get_setting(profile_id, orion::settings::reader_mode::border_shadow_enabled,
    [self_weak](orion::GetSettingReply<orion::settings::ReaderModeBorderShadowEnabled> reply){
        VERIFY_LOG(reply.err);

        auto self_strong = self_weak.lock();
        if(!self_strong) return;
        ReaderModeSettingsPopover* self = APP_READER_MODE_SETTINGS_POPOVER(self_strong.get());

        g_signal_handlers_block_by_func(self->border_shadow_toggle_group, (gpointer)on_border_shadow_toggle_group_changed, self);
        adw_toggle_group_set_active(self->border_shadow_toggle_group, reply.value == orion::settings::ReaderModeBorderShadowEnabled::on ? 1 : 0);
        g_signal_handlers_unblock_by_func(self->border_shadow_toggle_group, (gpointer)on_border_shadow_toggle_group_changed, self);
    });
}


static void
reader_mode_settings_popover_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    ReaderModeSettingsPopover* self = APP_READER_MODE_SETTINGS_POPOVER(object);

    switch(property_id)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
reader_mode_settings_popover_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    ReaderModeSettingsPopover* self = APP_READER_MODE_SETTINGS_POPOVER(object);

    switch(property_id)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
on_decrease_font_size_clicked(GtkButton* button, ReaderModeSettingsPopover* popover)
{
    GActionMap* map = G_ACTION_MAP(gtk_widget_get_root(GTK_WIDGET(popover)));
    if(!map) return;

    int64_t current = orion::gtk::get_int64_action_state(map, "reader-mode-font-size-index");
    int64_t new_val = std::clamp(current - 1, 0l, 11l);

    global_core_api->set_setting(main_window_get_current_profile_id(MAIN_WINDOW(map)), orion::settings::reader_mode::font_size_index, new_val,
        [](orion::SetSettingReply<int64_t> reply){
            VERIFY_LOG(reply.err);
        });
}

static void
on_increase_font_size_clicked(GtkButton* button, ReaderModeSettingsPopover* popover)
{
    GActionMap* map = G_ACTION_MAP(gtk_widget_get_root(GTK_WIDGET(popover)));
    if(!map) return;

    int64_t current = orion::gtk::get_int64_action_state(map, "reader-mode-font-size-index");
    int64_t new_val = std::clamp(current + 1, 0l, 11l);

    global_core_api->set_setting(main_window_get_current_profile_id(MAIN_WINDOW(map)), orion::settings::reader_mode::font_size_index, new_val,
        [](orion::SetSettingReply<int64_t> reply){
            VERIFY_LOG(reply.err);
        });
}

static void
reader_mode_settings_popover_class_init(ReaderModeSettingsPopoverClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = reader_mode_settings_popover_finalize;
    object_class->set_property = reader_mode_settings_popover_set_property;
    object_class->get_property = reader_mode_settings_popover_get_property;

    //obj_properties[PROP_ID] = g_param_spec_int64("id", "id", NULL, INT64_MIN, INT64_MAX, 0, GParamFlags(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    //g_object_class_install_properties(object_class, N_PROPS, obj_properties);

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->map = reader_mode_settings_popover_map;
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/reader_mode_settings_popover.ui");
    gtk_widget_class_bind_template_child(widget_class, ReaderModeSettingsPopover, background_type_toggle_group);
    gtk_widget_class_bind_template_child(widget_class, ReaderModeSettingsPopover, font_list_view);
    gtk_widget_class_bind_template_child(widget_class, ReaderModeSettingsPopover, paper_width_scale);
    gtk_widget_class_bind_template_child(widget_class, ReaderModeSettingsPopover, text_justify_toggle_group);
    gtk_widget_class_bind_template_child(widget_class, ReaderModeSettingsPopover, border_shadow_toggle_group);
    gtk_widget_class_bind_template_callback(widget_class, on_decrease_font_size_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_increase_font_size_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_background_type_toggle_group_changed);
    gtk_widget_class_bind_template_callback(widget_class, on_paper_width_scale_value_changed);
    gtk_widget_class_bind_template_callback(widget_class, on_text_justify_toggle_group_changed);
    gtk_widget_class_bind_template_callback(widget_class, on_border_shadow_toggle_group_changed);
}
