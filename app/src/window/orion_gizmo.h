#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#include <functional>

//since GtkGizmo is not a part of public api, we still need something similar to use as a child widget to have an extra css node for the purpose of widgets with custom rendering (see DownloadsButtonCOntent)
#define ORION_TYPE_GIZMO (orion_gizmo_get_type ())

G_DECLARE_FINAL_TYPE(OrionGizmo, orion_gizmo, ORION, GIZMO, GtkWidget)

typedef std::function<void(GtkWidget*, GtkOrientation, int, int*, int*, int*, int*)> OrionGizmoMeasureFunc;
typedef std::function<void(GtkWidget*, int, int, int)> OrionGizmoAllocateFunc;
typedef std::function<void(GtkWidget*, GtkSnapshot*)> OrionGizmoSnapshotFunc;
typedef std::function<gboolean(GtkWidget*, double, double)> OrionGizmoContainsFunc;
typedef std::function<gboolean(GtkWidget*, GtkDirectionType)> OrionGizmoFocusFunc;
typedef std::function<gboolean(GtkWidget*)> OrionGizmoGrabFocusFunc;


OrionGizmo* orion_gizmo_new(OrionGizmoMeasureFunc measure, OrionGizmoAllocateFunc allocate, OrionGizmoSnapshotFunc snapshot/*, OrionGizmoContainsFunc contains, OrionGizmoFocusFunc focus, OrionGizmoGrabFocusFunc grab_focus*/);