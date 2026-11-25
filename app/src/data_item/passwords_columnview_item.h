#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define PASSWORDS_COLUMNVIEW_ITEM_TYPE (passwords_columnview_item_get_type())

G_DECLARE_FINAL_TYPE(PasswordsColumnviewItem, passwords_columnview_item, PASSWORDS_COLUMNVIEW, ITEM, GObject)

PasswordsColumnviewItem* passwords_columnview_item_new_from_dto(const orion::PasswordItemDto& dto);
orion::PasswordItemId passwords_columnview_item_get_id(PasswordsColumnviewItem* item);
GString* passwords_columnview_item_get_url(PasswordsColumnviewItem* item);
GString* passwords_columnview_item_get_username(PasswordsColumnviewItem* item);
GString* passwords_columnview_item_get_password(PasswordsColumnviewItem* item);
GString* passwords_columnview_item_get_verification_key(PasswordsColumnviewItem* item);
GString* passwords_columnview_item_get_verification_code(PasswordsColumnviewItem* item);
double passwords_columnview_item_get_seq_num(PasswordsColumnviewItem* item);
GtkLabel* passwords_columnview_item_get_password_label(PasswordsColumnviewItem* item);
GtkWidget* passwords_columnview_item_get_verification_code_widget(PasswordsColumnviewItem* item);
void passwords_columnview_item_set_url(PasswordsColumnviewItem* item, const char* url);
void passwords_columnview_item_set_username(PasswordsColumnviewItem* item, const char* username);
void passwords_columnview_item_set_password(PasswordsColumnviewItem* item, const char* password);
void passwords_columnview_item_set_verification_key(PasswordsColumnviewItem* item, const char* verification_key);
void passwords_columnview_item_set_verification_code(PasswordsColumnviewItem* item, const char* verification_code);
void passwords_columnview_item_set_password_label(PasswordsColumnviewItem* item, GtkLabel* label);
void passwords_columnview_item_set_verification_code_widget(PasswordsColumnviewItem* item, GtkWidget* widget);
