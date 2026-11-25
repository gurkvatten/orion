#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

// TODO: rename this to PageTweaksDialog
#define PAGE_TWEAKS_POPOVER_TYPE (page_tweaks_popover_get_type ())

G_DECLARE_FINAL_TYPE(PageTweaksPopover, page_tweaks_popover, PAGE_TWEAKS, POPOVER, AdwDialog)

AdwDialog* page_tweaks_popover_new();