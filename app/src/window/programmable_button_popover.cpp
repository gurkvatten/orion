
#include "app.h"

#include <gtk/gtk.h>
#include <adwaita.h>

#include <iostream>
#include <string_view>

#include <common/src/log.h>
#include <common/src/defer.h>

#include "programmable_button_popover.h"
#include "util/action_util.h"
#include "util/menu_util.h"
#include "import_from_url_dialog.h"
#include "window/columnview/orion_filter_selection_model.h"
#include "window/accelerator_widget/orion_accelerator_entry.h"
#include "window/main_menu/main_menu.h"
#include "window/columnview/orion_filter_selection_model.h"

#include "data_item/menu_item.h"

static constexpr std::string_view menu_path_separator = " → ";
static constexpr size_t menu_path_separator_length = menu_path_separator.length();

namespace {
    struct CppWrapper {
        orion::ProgrammableButtonId id;
        std::vector<std::string> search_terms;
    };
}
struct _ProgrammableButtonPopover
{
	GtkPopover parent;

    GtkEntry* title_entry;

#ifdef ENABLE_ADW_17_API
    AdwToggleGroup* icon_toggle_group;
#endif
    AdwViewStack* view_stack;

    GtkDropDown* menu_item_dropdown;

    GtkEntry* target_url_entry;
    GtkDropDown* open_in_dropdown;

    GtkTextView* code_text_view;
    GtkDropDown* code_language_dropdown;
    GtkCheckButton* show_output_in_sidebar_check_button;

    GtkGridView* icon_grid_view;
    GtkFilter* icon_filter;

    OrionAcceleratorEntry* accelerator_entry;
    GtkMenuButton* menu_button;

    CppWrapper cpp_data;
};

G_DEFINE_TYPE(ProgrammableButtonPopover, programmable_button_popover, GTK_TYPE_POPOVER)

enum {
    PROP_0,
    PROP_ID,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS] = {NULL, };


static constexpr std::string icon_prefix = "icon:";
static constexpr std::string emoji_prefix = "emoji:";
static constexpr std::vector<std::string> get_toggle_group_icons()
{       
    return {
        "emoji:❖",
        "icon:play-symbolic",
        "icon:running-symbolic",
        "icon:navigate-symbolic",
        "icon:shoe-box-symbolic",
        "icon:brain-symbolic",
        "icon:search-folder-symbolic",
        "icon:share-symbolic",
    };
}

ProgrammableButtonImage
programmable_button_parse_image(const orion::ProgrammableButtonDto& dto)
{
    if (dto.icon_string.empty())
    {
        return {"", ProgrammableButtonImageType::not_set};
    }
    else if (dto.icon_string.find(emoji_prefix, 0) == 0)
    {
        return {dto.icon_string.substr(emoji_prefix.length()), ProgrammableButtonImageType::emoji};
    }
    else if (dto.icon_string.find(icon_prefix, 0) == 0)
    {
        return {dto.icon_string.substr(icon_prefix.length()), ProgrammableButtonImageType::icon_name};
    }
    else
    {
        return {dto.icon_string, ProgrammableButtonImageType::icon_name};
    }
}

GtkPopover*
programmable_button_popover_new(orion::ProgrammableButtonId id)
{
	return GTK_POPOVER(g_object_new(PROGRAMMABLE_BUTTON_POPOVER_TYPE, "id", id.value, NULL));
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    gtk_popover_popdown(GTK_POPOVER(user_data));
}

static void
on_save_clicked(GtkButton* button, ProgrammableButtonPopover* self)
{
    defer([self]{ gtk_popover_popdown(GTK_POPOVER(self)); });

    const char* visible_name = adw_view_stack_get_visible_child_name(self->view_stack);
    if(!visible_name) return;

    orion::UpdateProgrammableButtonQuery query{
        .id = self->cpp_data.id,
    };
    query.update_action_type = true;
    query.update_action_string = true;
    if(!strcmp(visible_name, "menu_action_page"))
    {
        query.action_type = orion::ProgrammableButtonActionType::menu_action;

        query.update_action_string = true;
        AppMenuItem* item = APP_MENU_ITEM(gtk_drop_down_get_selected_item(self->menu_item_dropdown));
        if(item)
        {
            query.action_string = std::move(menu_item_get_detailed_action_name(item));
        }
        else
        {
            query.action_string = "";
        }
    }
    else if(!strcmp(visible_name, "open_uri_page"))
    {
        uint selected = gtk_drop_down_get_selected(self->open_in_dropdown);
        switch(selected)
        {
            case 0:
                query.action_type = orion::ProgrammableButtonActionType::open_uri_in_current_tab;
                break;
            case 1:
                query.action_type = orion::ProgrammableButtonActionType::open_uri_in_new_tab;
                break;
            case 2:
                query.action_type = orion::ProgrammableButtonActionType::open_uri_in_link_preview;
                break;
            case 3:
                query.action_type = orion::ProgrammableButtonActionType::open_uri_in_background;
                break;
            default:
                g_assert_not_reached();
                break;
        }
        const char* target = gtk_editable_get_text(GTK_EDITABLE(self->target_url_entry)); //should guarantee non NULL
        query.action_string = target;
    }
    else if(!strcmp(visible_name, "run_code_page"))
    {
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(self->code_text_view);

        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        char* target = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);

        if(target)
        {
            query.action_string = target;
            g_free(target);
        }

        gboolean show_in_sidebar = gtk_check_button_get_active(self->show_output_in_sidebar_check_button);
        uint selected = gtk_drop_down_get_selected(self->code_language_dropdown);
        switch(selected)
        {
            case 0:
                query.action_type = show_in_sidebar ?
                    orion::ProgrammableButtonActionType::run_javascript_code_open_sidebar :
                    orion::ProgrammableButtonActionType::run_javascript_code;
                break;
            default:
                g_assert_not_reached();
                break;
        }


    }
    else
    {
        g_assert_not_reached();
    }
    query.update_title = true;
    query.title = gtk_editable_get_text(GTK_EDITABLE(self->title_entry));

#ifdef ENABLE_ADW_17_API
    query.update_icon_string = true;
    
    guint selected_pos = adw_toggle_group_get_active(self->icon_toggle_group);
    AdwToggle* toggle = adw_toggle_group_get_toggle(self->icon_toggle_group, selected_pos);
    if(const char* label = adw_toggle_get_label(toggle))
    {
        query.icon_string = std::string("emoji:") + label;
    }
    else if(const char* icon_name = adw_toggle_get_icon_name(toggle))
    {
        query.icon_string = std::string("icon:") + icon_name;
    }
#endif

    query.update_keyboard_shortcut = true;
    query.keyboard_shortcut = orion_accelerator_entry_get_accelerator_string(self->accelerator_entry);

    //TODO: keyboard shortcut handling
    global_core_api->update_programmable_button(query, [](orion::Error err){ VERIFY_LOG(err); });
}

//vector of lowercase strings split from input based on whitespace
static inline std::vector<std::string>
split_to_search_terms(const char* text)
{
    std::vector<std::string> terms{};
    if(!text) return terms;
    std::string word;
    word.reserve(15);

    for(const char* c_ptr = text; *c_ptr != '\0'; c_ptr++ )
    {
        unsigned char c = static_cast<unsigned char>(*c_ptr);

        if(std::isspace(c))
        {
            if(!word.empty())
            {
                terms.push_back(std::move(word));
                word.clear();
                word.reserve(15);
            }
        }
        else
        {
            word.push_back(std::tolower(c));
        }
    }
    if(!word.empty())
    {
        terms.push_back(std::move(word));
    }
    return terms;
}

static void
on_icon_search_entry_search_changed(GtkSearchEntry* entry, ProgrammableButtonPopover* self)
{
    if(!self->icon_filter)
    {
        return;
    }
    const char* text = gtk_editable_get_text(GTK_EDITABLE(entry));
    
    self->cpp_data.search_terms = std::move(split_to_search_terms(text));
    //TODO: technically could emit a more efficient signal here but that would require comparison of new and old terms
    gtk_filter_changed(self->icon_filter, GTK_FILTER_CHANGE_DIFFERENT);
}

gboolean
icon_grid_view_filter(gpointer item, gpointer user_data) {
    GtkStringObject* object = GTK_STRING_OBJECT(item);
    const char* icon_name_c = gtk_string_object_get_string(object);
    ProgrammableButtonPopover* popover = PROGRAMMABLE_BUTTON_POPOVER(user_data);

    if(!(popover->cpp_data.search_terms.size()) || !icon_name_c)
    {
        return TRUE;
    }

    std::string_view icon_name(icon_name_c);

    //check if every word exists as a substring in the icon_name
    for(const auto& term : popover->cpp_data.search_terms)
    {
        //NOTE: these search terms are already lower case based on the entry search changed signal handler
        if(icon_name.find(term) == std::string_view::npos)
        {
            return FALSE; 
        }
    }
    return TRUE;
}

static char*
join_strv_and_str(GObject* this_object, const char* str, const GStrv strv)
{
    if(!strv) return g_strdup(str); //NULL safe through g_strdup

    char* joined_vec = g_strjoinv(" ", strv);

    if(!str) return joined_vec;
    
    char* joined_string = g_strconcat(joined_vec, " ", str, NULL);

    g_free(joined_vec);
    
    return joined_string;
}

static void
on_custom_icon_selection_changed(GtkSelectionModel* model, guint pos, guint n_items, ProgrammableButtonPopover* self)
{
    GtkBitset* selection = gtk_selection_model_get_selection(model);
    defer([selection]{ gtk_bitset_unref(selection); });
    if(!gtk_bitset_get_size(selection)) return;

    guint selected = gtk_bitset_get_nth(selection, 0);
    GtkStringObject* item = GTK_STRING_OBJECT(g_list_model_get_item(G_LIST_MODEL(model), selected));
    defer([item]{ g_object_unref(item); });

    if(guint n_toggles = adw_toggle_group_get_n_toggles(self->icon_toggle_group))
    {
        adw_toggle_set_icon_name(adw_toggle_group_get_toggle(self->icon_toggle_group, n_toggles-1), gtk_string_object_get_string(item));
    }
}

static void
programmable_button_popover_init(ProgrammableButtonPopover* win)
{
    std::construct_at(&win->cpp_data);
    g_type_ensure(APP_TYPE_MENU_ITEM);
	gtk_widget_init_template(GTK_WIDGET(win));

#ifdef ENABLE_ADW_17_API
    std::vector<std::string> toggle_icon_names = get_toggle_group_icons();
    for(const std::string& toggle_icon_name : toggle_icon_names)
    {
        AdwToggle* toggle = adw_toggle_new();
        const char* string_start = toggle_icon_name.c_str();
        if(toggle_icon_name.starts_with(emoji_prefix))
        {
            adw_toggle_set_label(toggle, string_start + emoji_prefix.length());
        }
        else if(toggle_icon_name.starts_with(icon_prefix))
        {
            adw_toggle_set_icon_name(toggle, string_start + icon_prefix.length());
        }
        adw_toggle_group_add(win->icon_toggle_group, toggle);
    }
#endif

    OrionApp* app = ORION_APP(g_application_get_default());
    GtkStringList* list = orion_app_get_icon_theme_string_list(app);
    GtkSingleSelection* selection_model = gtk_single_selection_new(g_object_ref(G_LIST_MODEL(list)));
    g_signal_connect(selection_model, "selection-changed", G_CALLBACK(on_custom_icon_selection_changed), win);
    OrionFilterSelectionModel* filtered_model = orion_filter_selection_model_new(GTK_SELECTION_MODEL(selection_model));
    g_object_unref(selection_model);

    win->icon_filter = GTK_FILTER(g_object_ref(gtk_custom_filter_new(icon_grid_view_filter, win, NULL)));
    orion_filter_selection_model_set_filter(filtered_model, win->icon_filter);
    gtk_grid_view_set_model(win->icon_grid_view, GTK_SELECTION_MODEL(filtered_model));

    GListModel* main_menu_model = create_main_menu_list_model();
    //orion::log_debug("%i", g_list_model_get_n_items(main_menu_model));
    GtkSelectionModel* main_menu_selection_model = GTK_SELECTION_MODEL(gtk_single_selection_new(main_menu_model));
    OrionFilterSelectionModel* main_menu_filtered_model = orion_filter_selection_model_new(main_menu_selection_model);
    g_object_unref(main_menu_selection_model);

    gtk_drop_down_set_model(win->menu_item_dropdown, G_LIST_MODEL(main_menu_filtered_model));
    defer([main_menu_filtered_model]{ g_object_unref(main_menu_filtered_model); });
}

static void
programmable_button_popover_dispose(GObject* object)
{
    ProgrammableButtonPopover* self = PROGRAMMABLE_BUTTON_POPOVER(object);
    g_clear_object(&self->icon_filter);
    G_OBJECT_CLASS(programmable_button_popover_parent_class)->dispose(object);
}

static void
programmable_button_popover_finalize(GObject* object)
{
    ProgrammableButtonPopover* self = PROGRAMMABLE_BUTTON_POPOVER(object);
    std::destroy_at(&self->cpp_data);
    G_OBJECT_CLASS(programmable_button_popover_parent_class)->finalize(object);
}

guint
get_menu_item_dropdown_index_from_action(ProgrammableButtonPopover* self, std::string& action_string)
{
    GListModel* model = gtk_drop_down_get_model(self->menu_item_dropdown);

    guint n_items = g_list_model_get_n_items(model);
    for(guint i = 0; i<n_items; i++)
    {
        AppMenuItem* item = APP_MENU_ITEM(g_list_model_get_item(model, i));
        defer([item]{ g_object_unref(item); });

        if(menu_item_matches_action_string(item, action_string))
        {
            return i;
        }
    }
    return GTK_INVALID_LIST_POSITION;
}

static void
init_share_menu_model(GMenu* menu, orion::ProgrammableButtonId id)
{
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_action_with_int64_target(
            section, _("Import Button from URL…"), "win.import-button-from-url", id.value);
        orion::gtk::menu_append_action_with_int64_target(
            section, _("Import Button from File…"), "win.import-button-from-file", id.value);
        orion::gtk::menu_append_action_with_int64_target(
            section, _("Export to File…"), "win.export-button-to-file", id.value);
    }
}

static void
programmable_button_popover_setup_menu(ProgrammableButtonPopover* self, const orion::ProgrammableButtonDto& dto)
{
    GMenu* menu = g_menu_new();
    init_share_menu_model(menu, dto.id.get());
    gtk_menu_button_set_menu_model(self->menu_button, G_MENU_MODEL(menu));
}

static void
programmable_button_popover_init_from_dto(ProgrammableButtonPopover* self, orion::ProgrammableButtonDto& dto)
{
    programmable_button_popover_setup_menu(self, dto);
    gtk_editable_set_text(GTK_EDITABLE(self->title_entry), dto.title.c_str());
    switch(dto.action_type)
    {
        case orion::ProgrammableButtonActionType::not_set:
            adw_view_stack_set_visible_child_name(self->view_stack, "menu_action_page");
            break;
        case orion::ProgrammableButtonActionType::menu_action:
            {
                adw_view_stack_set_visible_child_name(self->view_stack, "menu_action_page");
                guint index = get_menu_item_dropdown_index_from_action(self, dto.action_string);
                gtk_drop_down_set_selected(self->menu_item_dropdown, index);
                //TODO: correctly init the entry, gonna be a pain
                //essentially, lookup the action string in our main menu non dynamic prototype, then pretty print it, this part is gonna be wild as heecccccc
                break;
            }
        case orion::ProgrammableButtonActionType::open_uri_in_current_tab:
            adw_view_stack_set_visible_child_name(self->view_stack, "open_uri_page");
            gtk_editable_set_text(GTK_EDITABLE(self->target_url_entry), dto.action_string.c_str());
            gtk_drop_down_set_selected(self->open_in_dropdown, 0);
            break;
        case orion::ProgrammableButtonActionType::open_uri_in_new_tab:
            adw_view_stack_set_visible_child_name(self->view_stack, "open_uri_page");
            gtk_editable_set_text(GTK_EDITABLE(self->target_url_entry), dto.action_string.c_str());
            gtk_drop_down_set_selected(self->open_in_dropdown, 1);
            break;
        case orion::ProgrammableButtonActionType::open_uri_in_link_preview:
            adw_view_stack_set_visible_child_name(self->view_stack, "open_uri_page");
            gtk_editable_set_text(GTK_EDITABLE(self->target_url_entry), dto.action_string.c_str());
            gtk_drop_down_set_selected(self->open_in_dropdown, 2);
            break;
        case orion::ProgrammableButtonActionType::open_uri_in_background:
            adw_view_stack_set_visible_child_name(self->view_stack, "open_uri_page");
            gtk_editable_set_text(GTK_EDITABLE(self->target_url_entry), dto.action_string.c_str());
            gtk_drop_down_set_selected(self->open_in_dropdown, 3);
            break;
        case orion::ProgrammableButtonActionType::run_javascript_code:
            gtk_check_button_set_active(self->show_output_in_sidebar_check_button, FALSE);
            adw_view_stack_set_visible_child_name(self->view_stack, "run_code_page");
            gtk_text_buffer_set_text(gtk_text_view_get_buffer(self->code_text_view), dto.action_string.c_str(), dto.action_string.length());
            gtk_drop_down_set_selected(self->code_language_dropdown, 0);
            break;
        case orion::ProgrammableButtonActionType::run_javascript_code_open_sidebar:
            gtk_check_button_set_active(self->show_output_in_sidebar_check_button, TRUE);
            adw_view_stack_set_visible_child_name(self->view_stack, "run_code_page");
            gtk_text_buffer_set_text(gtk_text_view_get_buffer(self->code_text_view), dto.action_string.c_str(), dto.action_string.length());
            gtk_drop_down_set_selected(self->code_language_dropdown, 0);
            break;
    }

#ifdef ENABLE_ADW_17_API
    std::vector<std::string> possible_icon_names = get_toggle_group_icons();
    auto it = std::find(possible_icon_names.cbegin(), possible_icon_names.cend(), dto.icon_string);
    if(!(it == possible_icon_names.cend()))
    {
        adw_toggle_group_set_active(self->icon_toggle_group, std::distance(possible_icon_names.cbegin(), it));
    }
    else
    {
        if(dto.icon_string.starts_with("icon:"))
        {
            guint last_toggle_index = adw_toggle_group_get_n_toggles(self->icon_toggle_group) - 1;
            AdwToggle* last_toggle = adw_toggle_group_get_toggle(self->icon_toggle_group, last_toggle_index);


            adw_toggle_group_set_active(self->icon_toggle_group, last_toggle_index);
            std::string_view icon_string = dto.icon_string;
            icon_string = icon_string.substr(5, icon_string.length()-1);

            adw_toggle_set_icon_name(last_toggle, icon_string.data()); //guaranteed to be safely null terminated by originating from an std::string
        }
        //TODO: FIXME: set the last icon toggle to the correct string
    }
#endif

    orion_accelerator_entry_set_accelerator_from_string(self->accelerator_entry, dto.keyboard_shortcut.c_str());
}

static void
programmable_button_popover_set_id(ProgrammableButtonPopover* self, orion::ProgrammableButtonId id)
{
    self->cpp_data.id = id;
    orion::GetProgrammableButtonsQuery query{};
    query.ids = {self->cpp_data.id};
    orion::gtk::WeakRefWrapper self_weak(self);
    global_core_api->get_programmable_buttons(std::move(query), [self_weak](orion::GetProgrammableButtonsReply reply){
        VERIFY_LOG(reply.err);
        if(reply.items.size()!=1) return;
        
        auto self_strong = self_weak.lock();
        if(!self_strong) return;

        ProgrammableButtonPopover* self = PROGRAMMABLE_BUTTON_POPOVER(self_strong.get());
        auto& dto = reply.items.at(0);

        programmable_button_popover_init_from_dto(self, dto);
    });
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_ID]);
}

static void
programmable_button_popover_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    ProgrammableButtonPopover* self = PROGRAMMABLE_BUTTON_POPOVER(object);

    switch(property_id)
    {
        case PROP_ID:
            programmable_button_popover_set_id(self, orion::ProgrammableButtonId(g_value_get_int64(value)));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
programmable_button_popover_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    ProgrammableButtonPopover* self = PROGRAMMABLE_BUTTON_POPOVER(object);

    switch(property_id)
    {
        case PROP_ID:
            g_value_set_int64(value, self->cpp_data.id.value); 
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static gchar*
path_to_string(GtkListItem* item, GStrv path)
{
    if(!path) return NULL;
    
    size_t total_length = 0;
    for(char** p = path; *p; p++)
    {
        total_length += strlen(*p) + menu_path_separator_length;
    }
    gchar* result = g_new0(char, total_length+1);
    size_t current_index = 0;
    for(char** p = path; *p; p++)
    {
        for(char* c = *p; *c; c++)
        {
            result[current_index] = *c;
            current_index++;
        }
        for(char c : menu_path_separator)
        {
            result[current_index] = c;
            current_index++;
        }
    }
    return result;
}

static void
programmable_button_popover_class_init(ProgrammableButtonPopoverClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = programmable_button_popover_dispose;
    object_class->finalize = programmable_button_popover_finalize;
    object_class->set_property = programmable_button_popover_set_property;
    object_class->get_property = programmable_button_popover_get_property;

    obj_properties[PROP_ID] = g_param_spec_int64("id", "id", NULL, INT64_MIN, INT64_MAX, 0, GParamFlags(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    g_object_class_install_properties(object_class, N_PROPS, obj_properties);

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/programmable_button_popover.ui");

	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, title_entry);
#ifdef ENABLE_ADW_17_API
	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, icon_toggle_group);
#endif
	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, view_stack);
	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, menu_item_dropdown);
	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, target_url_entry);
	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, open_in_dropdown);
	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, code_text_view);
	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, code_language_dropdown);
	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, icon_grid_view);
	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, accelerator_entry);
	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, menu_button);
	gtk_widget_class_bind_template_child(widget_class, ProgrammableButtonPopover, show_output_in_sidebar_check_button);

    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_save_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_icon_search_entry_search_changed);
    gtk_widget_class_bind_template_callback(widget_class, path_to_string);
    gtk_widget_class_bind_template_callback(widget_class, join_strv_and_str);
}
