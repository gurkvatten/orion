#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define SUMMARY_UNAVAILABLE_DIALOG_TYPE (summary_unavailable_dialog_get_type ())

G_DECLARE_FINAL_TYPE(SummaryUnavailableDialog, summary_unavailable_dialog, SUMMARY_UNAVAILABLE, DIALOG, AdwDialog)

AdwDialog* summary_unavailable_dialog_new();