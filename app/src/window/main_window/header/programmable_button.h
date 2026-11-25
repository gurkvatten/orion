#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#include <core/dto/programmable_button_dto.h>

#define APP_PROGRAMMABLE_BUTTON_TYPE (programmable_button_get_type ())

enum class ProgrammableButtonActionType {
    menu_action,
    open_uri,
    run_code,
};

G_DECLARE_FINAL_TYPE(ProgrammableButton, programmable_button, APP, PROGRAMMABLE_BUTTON, GtkButton)

ProgrammableButton* programmable_button_new(orion::ProgrammableButtonId id);