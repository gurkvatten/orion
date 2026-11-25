#include "scroll_util.h"

#include <algorithm>

#define ENABLE_VERBOSE_LOGS 0

#include <common/src/log.h>

#define SCROLL_EDGE_OVERRIDE_SIZE 2 //must be positive, exists mostly to guard against rounding errors
#define PAGE_SIZE_FRACTION_MARGIN_ABOVE_SCROLL_TO_TARGET 0.05

namespace orion::gtk {

void
widget_scroll_to(GtkWidget* widget)
{
    GtkScrolledWindow* window = GTK_SCROLLED_WINDOW(gtk_widget_get_ancestor(widget, GTK_TYPE_SCROLLED_WINDOW));
    if(!window)
    {
        orion::log_error("widget_scroll_to(): failed to find scrolled window for %p", widget);
        return;
    }

    GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(window);

    
    graphene_point_t target = GRAPHENE_POINT_INIT_ZERO;
    if(!gtk_widget_compute_point(widget, GTK_WIDGET(window), &target, &target))
    {
        return; //this should be impossible considering this widget is definitely inside the content root's structure
    }

    double value = gtk_adjustment_get_value(vadjustment);
    double page_size = gtk_adjustment_get_page_size(vadjustment);
    double upper = gtk_adjustment_get_upper(vadjustment);
    double new_val = std::clamp(
        (double)target.y + value - page_size * PAGE_SIZE_FRACTION_MARGIN_ABOVE_SCROLL_TO_TARGET, 
        0.0, 
        upper - page_size
    );
    orion::log_verbose("scroll to: val %f page %f upper %f new_val %f", value, page_size, upper, new_val);

    //kinetic scroll can actually bypass manually setting an adjustment value, applying on the next frame, so this intentionally fully flushes that pipeline
    gboolean original = gtk_scrolled_window_get_kinetic_scrolling(window);
    if(original)
    {
        gtk_scrolled_window_set_kinetic_scrolling(window, FALSE);
    }
    gtk_adjustment_set_value(vadjustment, new_val);
    if(original)
    {
        gtk_scrolled_window_set_kinetic_scrolling(window, original);
    }
}

size_t
get_scrolled_to_widget_index(const std::vector<GtkWidget*>& widgets, double trigger_location_lower, double trigger_location_upper)
{
    if(!widgets.size())
    {
        orion::log_error("get_scrolled_to_widget_index(): got a 0 length vector");
        return 0;
    }

    GtkScrolledWindow* window = GTK_SCROLLED_WINDOW(gtk_widget_get_ancestor(widgets.at(0), GTK_TYPE_SCROLLED_WINDOW));

    GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(window);

    //heuristic: dynamic "reading line" trigger with top out/bottom out overrides
    
    double value = gtk_adjustment_get_value(vadjustment);
    double page_size = gtk_adjustment_get_page_size(vadjustment);
    double max_scroll = gtk_adjustment_get_upper(vadjustment) - page_size;
    orion::log_verbose("get scrolled: val %f page %f max %f new_val %f", value, page_size, max_scroll);

    //top out override
    if(value < (0 + SCROLL_EDGE_OVERRIDE_SIZE))
    {
        return 0;
    }

    //bottom out override
    if(value > (max_scroll - SCROLL_EDGE_OVERRIDE_SIZE))
    {
        return widgets.size()-1;
    }

    //linearly interpolates the fraction of page_size that the trigger should be located at
    //max scroll != 0 is already protected against through the top out/bottom out overrides as long as SCROLL_EDGE_OVERRIDE_SIZE is positive
    double progress = std::clamp(value / max_scroll, 0.0, 1.0);
    double trigger = page_size * (trigger_location_lower + (trigger_location_upper - trigger_location_lower) * progress);

    //GtkWidget* content_root = gtk_scrolled_window_get_child(window); //should always exist based on the gtk_widget_get_ancestor call
    //if(GTK_IS_VIEWPORT(content_root)) content_root = gtk_viewport_get_child(GTK_VIEWPORT(content_root));

    graphene_point_t origin = GRAPHENE_POINT_INIT_ZERO;
    graphene_point_t target = GRAPHENE_POINT_INIT_ZERO;
    for(size_t i = widgets.size()-1; i<widgets.size(); i--)
    {
        if(!gtk_widget_is_ancestor(widgets.at(i), GTK_WIDGET(window)))
        {
            orion::log_error("get_scrolled_to_widget_index(): widget not a child of scrolled window");
            return 0;
        }

        if(gtk_widget_compute_point(widgets.at(i), GTK_WIDGET(window), &origin, &target))
        {
            if(target.y < trigger)
            {
                
                return (int)i;
            }
        }
        else
        {
            orion::log_error("get_scrolled_to_widget_index(): failed to compute point for widget %p", widgets.at(i));
        }
    }
    
    if(gtk_widget_compute_point(widgets.at(0), GTK_WIDGET(window), &origin, &target))
    {
        if(trigger < target.y)
        {
            return 0;
        }
    }
    else
    {
        orion::log_error("get_scrolled_to_widget_index(): failed to compute point for widget %p", widgets.at(0));
    }


    return widgets.size()-1;
}

} // namespace orion