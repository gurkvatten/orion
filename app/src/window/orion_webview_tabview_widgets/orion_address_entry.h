#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

G_BEGIN_DECLS

//not sure how much of this is necessary but it's here to enable a custom class struct with custom signals

GType orion_address_entry_get_type (void);

typedef struct _OrionAddressEntry             OrionAddressEntry;
typedef struct _OrionAddressEntryClass        OrionAddressEntryClass;

#define ORION_ADDRESS_ENTRY_TYPE (orion_address_entry_get_type ())
#define ORION_ADDRESS_ENTRY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), ORION_ADDRESS_ENTRY_TYPE, OrionAddressEntry))
#define ORION_ADDRESS_ENTRY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), ORION_ADDRESS_ENTRY_TYPE, OrionAddressEntryClass))
#define ORION_IS_ADDRESS_ENTRY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ORION_ADDRESS_ENTRY_TYPE))
#define ORION_IS_ADDRESS_ENTRY_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), ORION_ADDRESS_ENTRY_TYPE))
#define ORION_ADDRESS_ENTRY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), ORION_ADDRESS_ENTRY_TYPE, OrionAddressEntryClass))

G_END_DECLS

OrionAddressEntry* orion_address_entry_new(gboolean editable);