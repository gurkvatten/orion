#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <gio/gio.h>
#include "app.h"

#define CERTIFICATE_INFO_DIALOG_TYPE (certificate_info_dialog_get_type())

G_DECLARE_FINAL_TYPE(CertificateInfoDialog, certificate_info_dialog, CERTIFICATE_INFO, DIALOG, AdwDialog)

AdwDialog* certificate_info_dialog_new(
    GTlsCertificate* certificate = NULL,
    GTlsCertificateFlags tls_errors = static_cast<GTlsCertificateFlags>(0),
    const char* uri = NULL
);
