#pragma once

#include <functional>
#include <string>

#include <gtk/gtk.h>
#include <adwaita.h>
#include <core/dto/filter_list_dto.h>
#include "../app.h"

#define CUSTOM_FILTER_URL_DIALOG_TYPE (custom_filter_url_dialog_get_type ())

G_DECLARE_FINAL_TYPE(CustomFilterUrlDialog, custom_filter_url_dialog, CUSTOM_FILTER_URL, DIALOG, AdwDialog)

using CustomFilterUrlDialogSubmitCallback =
    std::function<void(const std::string& url, const std::string& title, const std::string& description)>;
using CustomFilterUrlDialogUrlExistsCallback =
    std::function<bool(const std::string& url)>;

AdwDialog* custom_filter_url_dialog_new();
void custom_filter_url_dialog_set_values(
    CustomFilterUrlDialog* self, const char* url, const char* title, const char* description);
void custom_filter_url_dialog_set_filter_list(
    CustomFilterUrlDialog* self, const orion::FilterListDto& filter_list);
void custom_filter_url_dialog_set_submit_callback(
    CustomFilterUrlDialog* self, CustomFilterUrlDialogSubmitCallback callback);
void custom_filter_url_dialog_set_url_exists_callback(
    CustomFilterUrlDialog* self, CustomFilterUrlDialogUrlExistsCallback callback);
