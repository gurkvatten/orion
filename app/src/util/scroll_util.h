#pragma once

#include <gtk/gtk.h>

#include <vector>


namespace orion::gtk {

void widget_scroll_to(GtkWidget* widget);

//last 2 arguments specify the fraction of page size above which a widget is considered as currently viewed by the user
//needs to be run after an adjustment value change is applied in the gtk allocation cycle, example in popular_extensions_dialog.cpp::on_vadjustment_changed()
size_t get_scrolled_to_widget_index(const std::vector<GtkWidget*>& widgets, double trigger_location_lower, double trigger_location_upper);

} // namespace orion