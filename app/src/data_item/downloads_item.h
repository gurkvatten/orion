#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#include <core/dto/download_dto.h>

#include "app.h"

#include <string>

#define APP_TYPE_DOWNLOADS_ITEM (downloads_item_get_type())

G_DECLARE_FINAL_TYPE(DownloadsItem, downloads_item, APP, DOWNLOADS_ITEM, GObject)


#define APP_TYPE_DOWNLOAD_OPERATION_STATE (download_operation_state_get_type());
GType download_operation_state_get_type(void) G_GNUC_CONST;

DownloadsItem* downloads_item_new_from_dto(const orion::DownloadDto& dto);
void downloads_item_update_from_dto(DownloadsItem* item, const orion::DownloadDto& dto);

gboolean downloads_item_compare(gconstpointer item_a, gconstpointer item_b, gpointer user_data);

orion::DownloadId downloads_item_get_id(DownloadsItem* item);
std::filesystem::path downloads_item_get_downloads_path(DownloadsItem* item);
std::string downloads_item_get_file_name(DownloadsItem* item);
std::string downloads_item_get_origin_uri(DownloadsItem* item);
orion::DownloadOperationState downloads_item_get_operation_state(DownloadsItem* item);
orion::UnixTimeUtc64 downloads_item_get_timestamp(DownloadsItem* item);
int64_t downloads_item_get_total_size(DownloadsItem* item);
int64_t downloads_item_get_current_size(DownloadsItem* item);
