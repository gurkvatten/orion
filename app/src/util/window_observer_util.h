#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#include <functional>

#include <core/dto/window_dto.h>
#include <core/dto/profile_dto.h>
#include <common/src/observable.h>

namespace orion {

//returns the id of the observer this creates
void bind_callback_to_window_id(
    orion::WindowId id,
    std::function<void(const orion::WindowDto&)> cb,
    std::function<void(orion::ObserverId)> observer_creation_cb);


} // namespace orion

//returns a g_strdupped empty string, compatible with blueprint bindings
//creates an observer that is then attached to the label object using g_object_set_data_full
char* bind_object_with_label_to_window(GObject* closure_context_object, GObject* object, int64_t id);

gchar* bind_window_icon(GObject* closure, GtkImage* image, GtkLabel* label, int64_t window_id, GtkStack* stack);