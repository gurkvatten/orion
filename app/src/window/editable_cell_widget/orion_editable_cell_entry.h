#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

G_BEGIN_DECLS

//not sure how much of this is necessary but it's here to enable a custom class struct with custom signals

GType orion_editable_cell_entry_get_type (void);

typedef struct _OrionEditableCellEntry             OrionEditableCellEntry;
typedef struct _OrionEditableCellEntryClass        OrionEditableCellEntryClass;

#define ORION_EDITABLE_CELL_ENTRY_TYPE (orion_editable_cell_entry_get_type ())
#define ORION_EDITABLE_CELL_ENTRY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), ORION_EDITABLE_CELL_ENTRY_TYPE, OrionEditableCellEntry))
#define ORION_EDITABLE_CELL_ENTRY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), ORION_EDITABLE_CELL_ENTRY_TYPE, OrionEditableCellEntryClass))
#define ORION_IS_EDITABLE_CELL_ENTRY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ORION_EDITABLE_CELL_ENTRY_TYPE))
#define ORION_IS_EDITABLE_CELL_ENTRY_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), ORION_EDITABLE_CELL_ENTRY_TYPE))
#define ORION_EDITABLE_CELL_ENTRY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), ORION_EDITABLE_CELL_ENTRY_TYPE, OrionEditableCellEntryClass))

G_END_DECLS

OrionEditableCellEntry* orion_editable_cell_entry_new(gboolean editable);
void orion_editable_cell_entry_set_text(OrionEditableCellEntry* entry, const char* text);
void orion_editable_cell_entry_set_editable(OrionEditableCellEntry* entry, gboolean value);
gboolean orion_editable_cell_entry_get_editable(OrionEditableCellEntry* entry);