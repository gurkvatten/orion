#include "orion_gizmo.h"

#include <common/src/defer.h>

#include "window/main_window/main_window.h"

namespace {
    struct CppWrapper {
        OrionGizmoMeasureFunc measure_func;
        OrionGizmoAllocateFunc allocate_func;
        OrionGizmoSnapshotFunc snapshot_func;
        // OrionGizmoContainsFunc contains_func;
        // OrionGizmoFocusFunc focus_func;
        // OrionGizmoGrabFocusFunc grab_focus_func;
    };
}

struct _OrionGizmo
{
    GtkButton parent_type;

    GtkExpressionWatch* profile_id_watch;
    CppWrapper cpp_data;
};

G_DEFINE_TYPE(OrionGizmo, orion_gizmo, GTK_TYPE_WIDGET)

OrionGizmo*
orion_gizmo_new(OrionGizmoMeasureFunc measure, OrionGizmoAllocateFunc allocate, OrionGizmoSnapshotFunc snapshot/*, OrionGizmoContainsFunc contains, OrionGizmoFocusFunc focus, OrionGizmoGrabFocusFunc grab_focus*/)
{
    OrionGizmo* self = static_cast<OrionGizmo*>(g_object_new(ORION_TYPE_GIZMO, NULL));
    self->cpp_data.measure_func = std::move(measure);
    self->cpp_data.allocate_func = std::move(allocate);
    self->cpp_data.snapshot_func = std::move(snapshot);
    // self->cpp_data.contains_func = std::move(contains);
    // self->cpp_data.focus_func = std::move(focus);
    // self->cpp_data.grab_focus_func = std::move(grab_focus);
    return self;
}

static void
orion_gizmo_finalize(GObject* gobject)
{
    OrionGizmo* self = ORION_GIZMO(gobject);
    std::destroy_at(&self->cpp_data);
    G_OBJECT_CLASS(orion_gizmo_parent_class)->finalize(gobject);
}

static void
orion_gizmo_init(OrionGizmo* self)
{
    std::construct_at(&self->cpp_data);
}

static void
orion_gizmo_measure(GtkWidget* widget, GtkOrientation orientation, int for_size, int* min, int* nat, int* min_baseline, int* nat_baseline)
{
    OrionGizmo *self = ORION_GIZMO(widget);

    if (self->cpp_data.measure_func)
    {
        self->cpp_data.measure_func(widget, orientation, for_size, min, nat, min_baseline, nat_baseline);
    }
    else
    {
        GTK_WIDGET_CLASS(orion_gizmo_parent_class)->measure(widget, orientation, for_size, min, nat, min_baseline, nat_baseline);
    }
}

static void
orion_gizmo_size_allocate(GtkWidget* widget, int width, int height, int baseline)
{
    OrionGizmo *self = ORION_GIZMO(widget);

    if (self->cpp_data.allocate_func)
    {
        self->cpp_data.allocate_func(widget, width, height, baseline);
    }
    else
    {
        GTK_WIDGET_CLASS(orion_gizmo_parent_class)->size_allocate(widget, width, height, baseline);
    }
}

static void
orion_gizmo_snapshot(GtkWidget* widget, GtkSnapshot* snapshot)
{
    OrionGizmo *self = ORION_GIZMO(widget);
    if (self->cpp_data.snapshot_func)
    {
        self->cpp_data.snapshot_func(widget, snapshot);
    }
    else
    {
        GTK_WIDGET_CLASS(orion_gizmo_parent_class)->snapshot(widget, snapshot);
    }
}

// static gboolean
// orion_gizmo_contains(GtkWidget* widget, double x, double y)
// {
//     OrionGizmo *self = ORION_GIZMO(widget);

//     if (self->cpp_data.contains_func)
//     {
//         self->cpp_data.contains_func(widget, x, y);
//     }
// }

// static gboolean
// orion_gizmo_focus(GtkWidget* widget, GtkDirectionType direction)
// {
//     OrionGizmo *self = ORION_GIZMO(widget);

//     if (self->cpp_data.focus_func)
//     {
//         return self->cpp_data.focus_func(widget, direction);
//     }
//     else
//     {
//         return FALSE;
//     }
// }

// static gboolean
// orion_gizmo_grab_focus(GtkWidget* widget)
// {
//     OrionGizmo *self = ORION_GIZMO(widget);

//     if (self->cpp_data.grab_focus_func)
//     {
//         return self->cpp_data.grab_focus_func(widget);
//     }
//     else
//     {
//         return FALSE;
//     }
// }


static void
orion_gizmo_class_init(OrionGizmoClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    obj_class->finalize = orion_gizmo_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->measure = orion_gizmo_measure;
    widget_class->size_allocate = orion_gizmo_size_allocate;
    widget_class->snapshot = orion_gizmo_snapshot;
    // widget_class->contains = orion_gizmo_contains;
    // widget_class->focus = orion_gizmo_focus;
    // widget_class->grab_focus = orion_gizmo_grab_focus;
}
