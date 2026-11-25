#pragma once

#include <gtk/gtk.h>
#include <core/dto/programmable_button_dto.h>

namespace orion::gtk {

//produces a floating ref GVariant*
GVariant* programmable_button_parse_args(const ProgrammableButtonDto& dto);

// Returns for example: action(win.about-orion)
std::string programmable_button_parse_action_name(const ProgrammableButtonDto& dto);

// Returns for example: win.about-orion
std::string programmable_button_parse_action_name_raw(const ProgrammableButtonDto& dto);

GtkShortcutAction* programmable_button_parse_action(const ProgrammableButtonDto& dto);

GtkShortcutTrigger* programmable_button_parse_trigger(const ProgrammableButtonDto& dto);

GtkShortcut* programmable_button_parse_shortcut(const ProgrammableButtonDto& dto);


} //namespace orion::gtk