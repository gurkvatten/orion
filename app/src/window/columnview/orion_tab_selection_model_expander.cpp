#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <common/src/defer.h>

#include "orion_tab_selection_model_expander.h"
#include "orion_tab_selection_model.h"
struct _OrionTabSelectionModelExpander
{
	GtkWidget parent;
};

G_DEFINE_TYPE(OrionTabSelectionModelExpander, orion_tab_selection_model_expander, GTK_TYPE_WIDGET)

static void
orion_tab_selection_model_expander_init(OrionTabSelectionModelExpander* self)
{

}

static void
orion_tab_selection_model_expander_class_init(OrionTabSelectionModelExpanderClass* klass)
{
    
}