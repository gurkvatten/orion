#pragma once

#include "app.h"

#include <gtk/gtk.h>
#include <adwaita.h>

#define PROGRAMMABLE_BUTTON_POPOVER_TYPE (programmable_button_popover_get_type ())

G_DECLARE_FINAL_TYPE(ProgrammableButtonPopover, programmable_button_popover, PROGRAMMABLE_BUTTON, POPOVER, GtkPopover)

GtkPopover* programmable_button_popover_new(orion::ProgrammableButtonId id);

enum class ProgrammableButtonImageType {
    not_set,
    emoji,
    icon_name,
};

struct ProgrammableButtonImage {
    std::string text; // for example: "❖" or "play-symbolic"
    ProgrammableButtonImageType type;
};

ProgrammableButtonImage programmable_button_parse_image(const orion::ProgrammableButtonDto& dto);