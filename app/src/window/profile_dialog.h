#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <functional>
#include "../app.h"

#define PROFILE_DIALOG_TYPE (profile_dialog_get_type ())

G_DECLARE_FINAL_TYPE(ProfileDialog, profile_dialog, PROFILE, DIALOG, AdwDialog)

using ProfileDialogCreateDoneCb = std::function<void(orion::CreateProfileReply)>;

AdwDialog* profile_dialog_new_create(ProfileDialogCreateDoneCb done_cb = nullptr);
AdwDialog* profile_dialog_new_import(ProfileDialogCreateDoneCb done_cb = nullptr);
AdwDialog* profile_dialog_new_edit(orion::ProfileId id);
