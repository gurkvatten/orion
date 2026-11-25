#include "programmable_button.h"

#include <common/src/defer.h>

#include "window/main_window/main_window.h"
#include "window/main_window/menu/toolbar_menu.h"
#include "window/programmable_button_popover.h"

#include "util/toolbar_util.h"
#include "util/menu_util.h"
#include "util/dialog_util.h"

namespace {
    struct CppWrapper {
        orion::ProgrammableButtonId id;
        orion::ObserverId change_observer_id;
    };
}

struct _ProgrammableButton
{
    GtkWidget parent_type;

    GtkBox* box; // box containing the button and label
    GtkButton* button;
    GtkLabel* label; // label below the button (optionally hidden)
    GtkPopoverMenu* context_menu;

    CppWrapper cpp_data;
};

enum {
    PROP_0,
    PROP_ID,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS] = {NULL, };

G_DEFINE_TYPE(ProgrammableButton, programmable_button, GTK_TYPE_WIDGET)

static void
programmable_button_finalize(GObject* gobject)
{
    ProgrammableButton* self = APP_PROGRAMMABLE_BUTTON(gobject);
    std::destroy_at(&self->cpp_data);
    G_OBJECT_CLASS(programmable_button_parent_class)->finalize(gobject);
}

static void
programmable_button_dispose(GObject* gobject)
{
    ProgrammableButton* self = APP_PROGRAMMABLE_BUTTON(gobject);
    g_clear_pointer(&self->button, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->context_menu, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->label, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->box, orion::gtk::widget_unparent_wrapper);
    global_core_api->programmable_buttons_change().clear_observer(&self->cpp_data.change_observer_id);
    G_OBJECT_CLASS(programmable_button_parent_class)->dispose(gobject);
}

ProgrammableButton*
programmable_button_new(orion::ProgrammableButtonId id)
{
    return APP_PROGRAMMABLE_BUTTON(g_object_new(programmable_button_get_type(), "id", id.value, NULL));
}

static void
rmb_released(GtkWidget* widget, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    ProgrammableButton* self = APP_PROGRAMMABLE_BUTTON(widget);
    
    GMenu* context_menu = g_menu_new();
    defer([context_menu]{ g_object_unref(context_menu); });

    {
        GMenu* subsection = g_menu_new();
        defer([subsection]{ g_object_unref(subsection); });
        g_menu_append_section(context_menu, NULL, G_MENU_MODEL(subsection));

        orion::gtk::menu_append_action_with_int64_target(subsection, _("Edit…"), "win.edit-programmable-button", self->cpp_data.id.value);
    }
    {
        GMenu* subsection = g_menu_new();
        defer([subsection]{ g_object_unref(subsection); });
        g_menu_append_section(context_menu, NULL, G_MENU_MODEL(subsection));

        orion::gtk::menu_append_action_with_int64_target(subsection, _("Import Button from URL…"), "win.import-button-from-url", self->cpp_data.id.value);
        orion::gtk::menu_append_action_with_int64_target(subsection, _("Import Button from File…"), "win.import-button-from-file", self->cpp_data.id.value);
        orion::gtk::menu_append_action_with_int64_target(subsection, _("Export to File…"), "win.export-button-to-file", self->cpp_data.id.value);
    }

    auto toolbar_item_id = static_cast<orion::ToolbarItemId>(self->cpp_data.id.value);
    create_toolbar_button_context_menu(context_menu, orion::ToolbarItemType::programmable_button, toolbar_item_id);

    gtk_popover_menu_set_menu_model(self->context_menu, G_MENU_MODEL(context_menu));

    GdkRectangle rect{.x = (int)x, .y = (int)y, .width = 0, .height = 0};
    gtk_popover_set_pointing_to(GTK_POPOVER(self->context_menu), &rect);

    gtk_popover_popup(GTK_POPOVER(self->context_menu));
}

static gboolean
state_to_sensitive(GBinding* binding, const GValue* from, GValue* to, gpointer user_data)
{
    GVariant *state = g_value_get_variant(from);

    gboolean visible = FALSE;

    if (state && g_variant_is_of_type(state, G_VARIANT_TYPE_STRING))
        visible = g_strcmp0(g_variant_get_string(state, NULL), "1") == 0; // icon and text

    g_value_set_boolean(to, visible);
    return TRUE;
}

static void
programmable_button_init_from_dto(ProgrammableButton* self, const orion::ProgrammableButtonDto& dto)
{
    if(self->cpp_data.id != dto.id.value()) return;

    ProgrammableButtonImage image = programmable_button_parse_image(dto);
    if (image.type == ProgrammableButtonImageType::icon_name) {
        gtk_button_set_icon_name(self->button, image.text.c_str());
    } else if (image.type == ProgrammableButtonImageType::emoji) {
        gtk_button_set_label(self->button, image.text.c_str());
        // Removes the button frame, must be done after setting the label
        gtk_widget_add_css_class(GTK_WIDGET(self->button), "image-button");
    } else {
        // Typically: ❖ = Super key (roughly the equivalent of Mac Command key)  
        gtk_button_set_label(self->button, "❖");
        // Removes the button frame, must be done after setting the label
        gtk_widget_add_css_class(GTK_WIDGET(self->button), "image-button");
    }

    GtkWidget* parent = gtk_widget_get_parent(GTK_WIDGET(self));
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!win) {
        orion::log_warning("programmable_button_init_from_dto: could not find root main window");
        return;
    }

    GtkActionable* button_actionable = GTK_ACTIONABLE(self->button);
    if (dto.action_string.empty()) {
        // This button has no action yet, we should open the edit dialog the button is triggered.
        // If we set a non-existing action name (for example empty string), the widget will be
        // completely disabled (sensitive == FALSE) which is not what we want.
        gtk_actionable_set_action_target_value(button_actionable, g_variant_new_int64(self->cpp_data.id.value));
        std::string action_name = "win.edit-programmable-button";
        gtk_actionable_set_action_name(button_actionable, action_name.c_str());
        orion::log_verbose("setting programmable button action name: %s (id=%lld)",
            action_name.c_str(), self->cpp_data.id.value);
    } else {
        std::string action_name = orion::gtk::programmable_button_parse_action_name_raw(dto);
        orion::log_verbose("setting programmable button action name: %s (id=%lld)",
            action_name.c_str(), self->cpp_data.id.value);
        gtk_actionable_set_action_name(button_actionable, action_name.c_str());
        gtk_actionable_set_action_target_value(button_actionable, orion::gtk::programmable_button_parse_args(dto));
    }
}

static void
programmable_button_init(ProgrammableButton* self)
{           
    std::construct_at(&self->cpp_data);

    GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_name(GTK_WIDGET(box), "programmable_button");
    gtk_widget_set_valign(GTK_WIDGET(box), GTK_ALIGN_CENTER);

    GtkWidget* button = gtk_button_new();
    gtk_widget_set_tooltip_text(button, _("Programmable Button"));
    gtk_widget_set_margin_start(button, 2);
    {
        const char* button_classes[] = {"toolbar-button", "title-2", NULL};
        gtk_widget_set_css_classes(button, button_classes);
    }
    gtk_box_append(box, button);

    GtkLabel* label = GTK_LABEL(gtk_label_new(_("Programmable Button")));
    {
        const char* label_classes[] = {"toolbar-item-label", NULL};
        gtk_widget_set_css_classes(GTK_WIDGET(label), label_classes);
    }
    gtk_box_append(box, GTK_WIDGET(label));

    GtkWidget* context_menu = gtk_popover_menu_new_from_model(NULL);
    gtk_popover_set_has_arrow(GTK_POPOVER(context_menu), FALSE);
    self->box = box;
    self->button = GTK_BUTTON(button);
    self->label = label;
    self->context_menu = GTK_POPOVER_MENU(context_menu);
    gtk_widget_set_parent(GTK_WIDGET(box), GTK_WIDGET(self));
    gtk_widget_set_parent(context_menu, GTK_WIDGET(self));

    orion::gtk::register_right_click(GTK_WIDGET(self), button, rmb_released);
    orion::gtk::WeakRefWrapper self_weak(self);

    self->cpp_data.change_observer_id = global_core_api->programmable_buttons_change().add_observer(
    [self_weak](const orion::ProgrammableButtonsChange& change){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        ProgrammableButton* self = APP_PROGRAMMABLE_BUTTON(self_strong.get());

        for (const auto& dto : change.updated)
        {
            if (dto.id.get() == self->cpp_data.id)
            {
                programmable_button_init_from_dto(self, dto);
                break;
            }
        }
    });
}

static void
programmable_button_set_id(ProgrammableButton* self, orion::ProgrammableButtonId id)
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

        ProgrammableButton* self = APP_PROGRAMMABLE_BUTTON(self_strong.get());
        auto& dto = reply.items.at(0);

        programmable_button_init_from_dto(self, dto);
    });
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_ID]);
}

static void
programmable_button_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    ProgrammableButton* self = APP_PROGRAMMABLE_BUTTON(object);

    switch(property_id)
    {
        case PROP_ID:
            programmable_button_set_id(self, orion::ProgrammableButtonId(g_value_get_int64(value)));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
programmable_button_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    ProgrammableButton* self = APP_PROGRAMMABLE_BUTTON(object);

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

static gboolean
programmable_button_contains(GtkWidget* widget, double x, double y)
{
    ProgrammableButton* self = APP_PROGRAMMABLE_BUTTON(widget);
    return gtk_widget_contains(GTK_WIDGET(self->button), x, y);
}

static void
setup_label_visibility_binding(ProgrammableButton* self)
{
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!win) {
        orion::log_warning("programmable_button_root: could not find root main window");
        return;
    }

    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(win), "toolbar-item-style");
    if (!action) {
        orion::log_warning("programmable_button_root: could not find action toolbar-item-style");
        return;
    }

    g_object_bind_property_full(
        action,
        "state",
        self->label,
        "visible",
        G_BINDING_SYNC_CREATE,
        state_to_sensitive,
        NULL,
        NULL, // user_data
        NULL
    );
}

static void
programmable_button_root(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(programmable_button_parent_class)->root(widget); // always call up

    ProgrammableButton* self = APP_PROGRAMMABLE_BUTTON(widget);

    setup_label_visibility_binding(self);
}

static void
programmable_button_class_init(ProgrammableButtonClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    obj_class->dispose = programmable_button_dispose;
    obj_class->finalize = programmable_button_finalize;
    obj_class->set_property = programmable_button_set_property;
    obj_class->get_property = programmable_button_get_property;

    obj_properties[PROP_ID] = g_param_spec_int64("id", "id", NULL, INT64_MIN, INT64_MAX, 0, GParamFlags(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    g_object_class_install_properties(obj_class, N_PROPS, obj_properties);


    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

    widget_class->contains = programmable_button_contains;
    widget_class->root = programmable_button_root;

    //so apparently, I could set this per widget, at any time, gtk_widget_set_layout_manager, that sounds like horrifying architecture,
    //I'm surprised this opinionated of a framework would ever leave something like that in the api, I need to bleach my eyes with overloaded comma operators
    gtk_widget_class_set_layout_manager_type(widget_class, GTK_TYPE_BIN_LAYOUT);
}
