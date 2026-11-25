#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <string>
#include "app.h"

G_BEGIN_DECLS

//not sure how much of this is necessary but it's here to enable a custom class struct with custom signals

GType orion_accelerator_entry_get_type (void);

typedef struct _OrionAcceleratorEntry             OrionAcceleratorEntry;
typedef struct _OrionAcceleratorEntryClass        OrionAcceleratorEntryClass;

#define ORION_ACCELERATOR_ENTRY_TYPE (orion_accelerator_entry_get_type ())
#define ORION_ACCELERATOR_ENTRY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), ORION_ACCELERATOR_ENTRY_TYPE, OrionAcceleratorEntry))
#define ORION_ACCELERATOR_ENTRY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), ORION_ACCELERATOR_ENTRY_TYPE, OrionAcceleratorEntryClass))
#define ORION_IS_ACCELERATOR_ENTRY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ORION_ACCELERATOR_ENTRY_TYPE))
#define ORION_IS_ACCELERATOR_ENTRY_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), ORION_ACCELERATOR_ENTRY_TYPE))
#define ORION_ACCELERATOR_ENTRY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), ORION_ACCELERATOR_ENTRY_TYPE, OrionAcceleratorEntryClass))

G_END_DECLS

OrionAcceleratorEntry* orion_accelerator_entry_new();

std::string orion_accelerator_entry_get_accelerator_string(OrionAcceleratorEntry* entry);

void orion_accelerator_entry_set_accelerator_from_string(OrionAcceleratorEntry* entry, const char* string);
