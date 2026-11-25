# Creating new dialog 

It is possible to create new dialog from scratch, or it can be created by copying an existing dialog and editing it. The process below is based on copying and editing an existing dialog. This is an example of new dialog with the name `Reset Orion` (which will cover the functionality of Orion browser restoration to default settings and the like):
1. Create new file `/app/src/window/reset_orion_dialog.blp` that is a copy of another dialog, for example `/app/src/window/add_bookmark_dialog.blp`
2. Edit Blueprint file `/app/src/window/reset_orion_dialog.blp`:
- change template name `template $AddBookmarkDialog` –> `template $ResetOrionDialog`
- change the dialog title text `title: _("Add This Page to");` –> `title: _("Reset Orion");`
  - this is convenient to do to be able to check easily at the end of the whole process that everything was done properly
- change dialog name `name: "add-bookmark-dialog";` -> `name: "reset-orion-dialog";` (note that the `name` property is not always needed, so it may not be present in the original dialog)
3. In `/CMakeLists.txt` file, add new entry `"/app/src/window/reset_orion_dialog.cpp"` to `add_executable()` function:
```
add_executable("orion-linux"
    ...
    src/window/about_dialog.cpp
    src/window/settings_dialog.cpp
    src/window/reset_orion_dialog.cpp
)
```
4. In `/CMakeLists.txt` file, add new entry `/app/src/window/reset_orion_dialog.blp` to `set(BLUEPRINT_FILES)` list:
```
set(BLUEPRINT_FILES
    ...
    src/window/about_dialog.blp
    src/window/settings_dialog.blp
    src/window/reset_orion_dialog.blp
)
```
5. In `/app/src/window/` directory, create a copy of file `add_bookmark_dialog.cpp` named `reset_orion_dialog.cpp` and copy of file `add_bookmark_dialog.h` named `reset_orion_dialog.h`
6. In `/app/src/window/reset_orion_dialog.cpp` and `/app/src/window/reset_orion_dialog.h` files, change each reference to the original About dialog to the new ResetOrion dialog, for example in `reset_orion_dialog.h` file:
```
#define RESET_ORION_DIALOG_TYPE (reset_orion_dialog_get_type ())

G_DECLARE_FINAL_TYPE(ResetOrionDialog, reset_orion_dialog, RESET_ORION, DIALOG, AdwDialog)

AdwDialog* reset_orion_dialog_new();
```
And in `reset_orion_dialog.cpp` file:
```
#include "reset_orion_dialog.h"

struct _ResetOrionDialog
{
    AdwDialog parent;
};

G_DEFINE_TYPE(ResetOrionDialog, reset_orion_dialog, ADW_TYPE_DIALOG)

AdwDialog*
reset_orion_dialog_new()
{
    return ADW_DIALOG(g_object_new(RESET_ORION_DIALOG_TYPE, NULL));
}

static void
reset_orion_dialog_init(ResetOrionDialog* win)
{
    gtk_widget_init_template(GTK_WIDGET(win));
}

static void
reset_orion_dialog_class_init(ResetOrionDialogClass* klass)
{
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/reset_orion_dialog.ui");

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_button_clicked);
}
```
7. The application should now be compilable. For testing purposes and development options, it is convenient to set the new dialog as a primary dialog after running the application – edit `/app/src/app.cpp` file (but do not commit these changes to Git master branch):
- add include at the top of the file:
```
#include "window/reset_orion_dialog.h"
```
- change `orion_app_activate()` function so that it presents new `ResetOrionDialog` dialog on top of the main application window immediately:
```
static void
orion_app_activate()
{
    ...
    //dialog = orion_app_dialog_new();
    //dialog = about_dialog_new();
    dialog = reset_orion_dialog_new();

    ...
}
```
8. Recompile the application and run it – a new dialog should appear which will be titled "Reset Orion".
