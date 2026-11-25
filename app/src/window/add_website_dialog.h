#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <string>
#include <vector>
#include <core/dto/profile_dto.h>
#include "../app.h"

#define ADD_WEBSITE_DIALOG_TYPE (add_website_dialog_get_type ())

G_DECLARE_FINAL_TYPE(AddWebsiteDialog, add_website_dialog, ADD_WEBSITE, DIALOG, AdwDialog)

struct AddWebsiteDialogSettingOption {
    std::string value;
    std::string label;
};

AdwDialog* add_website_dialog_new(
    orion::ProfileId profile_id,
    const std::string& key_name,
    const std::string& type,
    std::vector<AddWebsiteDialogSettingOption> options
);
