#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define CRASH_REPORTER_DIALOG_TYPE (crash_reporter_dialog_get_type ())

G_DECLARE_FINAL_TYPE(CrashReporterDialog, crash_reporter_dialog, CRASH_REPORTER, DIALOG, AdwDialog)

AdwDialog* crash_reporter_dialog_new();