#include <common/src/defer.h>

#include "profiles_button_menu.h"
#include "profile_windows_submenu.h"
#include "util/menu_util.h"
#include "util/text_util.h"

static void
fill_current_profile_submenu(GMenu* menu, orion::ProfileDto current_profile, MainWindow* win, std::function<void()> done_cb)
{
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });

        g_menu_append(section, _("Edit…"), "win.edit-profile");

        if (!current_profile.is_internal) {
            g_menu_append(section, _("Delete"), "win.delete-current-profile");
        }

        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });

        GMenu* windows_submenu = g_menu_new();
        defer([windows_submenu]{ g_object_unref(windows_submenu); });

        update_profile_windows_submenu(windows_submenu, current_profile.id, win, std::move(done_cb));
        g_menu_append_submenu(section, _("Windows"), G_MENU_MODEL(windows_submenu));

        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    }
    if (!current_profile.is_default) {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });

        g_menu_append(section, _("Set Default"), "win.set-current-profile-as-default");

        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });

        // TODO: show last snapshot time if available (as disabled menu item)
        g_menu_append(section, _("Take Snapshot"), "win.take-workspace-snapshot");
        g_menu_append(section, _("Restore Snapshot…"), "win.restore-snapshot-menu");

        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        
        GMenuItem* item = g_menu_item_new(NULL, NULL);
        defer([item]{ g_object_unref(item); });

        g_menu_item_set_attribute(item, "custom", "s", "profiles_icon_selection");
        g_menu_append_item(section, item);

        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    }
}

void
recreate_profiles_menu(GMenu* menu, orion::ProfileId current_profile_id, MainWindow* win, std::function<void()> done_cb)
{
    g_menu_remove_all(menu);

    orion::gtk::WeakRefWrapper menu_weak(menu);
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_profile_ids(
        [menu_weak,current_profile_id,done_cb=std::move(done_cb),win_weak]
        (orion::GetProfileIdsReply reply){
            VERIFY_LOG(reply.err);
            
            global_core_api->get_profiles(reply.ids,
                [menu_weak,current_profile_id,done_cb=std::move(done_cb),win_weak]
                (orion::GetProfilesReply reply){
                    VERIFY_LOG(reply.err);

                    orion::ProfileDto current_profile;
                    std::vector<orion::ProfileDto> other_profiles;

                    for (orion::ProfileDto profile : reply.profiles) {
                        if (profile.id == current_profile_id) {
                            current_profile = profile;
                            continue;
                        }
                        other_profiles.push_back(profile);
                    }

                    if (current_profile.id == orion::ProfileId(0)) {
                        orion::log_error("recreate_profiles_menu: current profile not found, id=%" PRId64,
                            current_profile_id.value
                        );
                        return;
                    }

                    std::sort(other_profiles.begin(), other_profiles.end(), [](const orion::ProfileDto& a, const orion::ProfileDto& b){
                        return a.seq_num > b.seq_num;
                    });

                    auto menu_strong = menu_weak.lock();
                    if (!menu_strong) return;
                    GMenu* menu = G_MENU(menu_strong.get());

                    auto win_strong = win_weak.lock();
                    if (!win_strong) return;
                    MainWindow* win = MAIN_WINDOW(win_strong.get());
                
                    {
                        GMenu* section = g_menu_new();
                        defer([section]{ g_object_unref(section); });

                        GMenu* profile_submenu = g_menu_new();
                        defer([profile_submenu]{ g_object_unref(profile_submenu); });

                        fill_current_profile_submenu(profile_submenu, current_profile, win, nullptr); // TODO: use done_cb?
                        
                        g_menu_append_submenu(menu, orion::text::ellipsize_string(current_profile.name, MAX_CONTEXT_MENU_USER_STRING_LENGTH).c_str(), G_MENU_MODEL(profile_submenu));

                        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
                    }
                    {
                        GMenu* section = g_menu_new();
                        defer([section]{ g_object_unref(section); });

                        for (orion::ProfileDto profile : other_profiles) {
                            GMenuItem* item = g_menu_item_new(NULL, NULL);
                            defer([item]{ g_object_unref(item); });
                            std::string label_string = (profile.icon_name+" "+orion::text::ellipsize_string(profile.name, MAX_CONTEXT_MENU_USER_STRING_LENGTH-2));
                            g_menu_item_set_attribute(item, "label", "s", label_string.c_str());
                            g_menu_item_set_attribute(item, "use-markup", "s", "true");
                            g_menu_item_set_action_and_target(item, "win.change-profile", "x", profile.id.value);
                            g_menu_append_item(section, item);
                        }

                        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
                    }
                    {
                        GMenu* section = g_menu_new();
                        defer([section]{ g_object_unref(section); });

                        g_menu_append(section, _("New Profile…"), "win.new-profile");
                        g_menu_append(section, _("Manage Profiles…"), "win.manage-profiles");

                        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
                    }
                    {
                        // TODO: this section should only be present if this is Profile Button menu,
                        // not main menu File -> Profiles submenu
                        GMenu* section = g_menu_new();
                        defer([section]{ g_object_unref(section); });

                        orion::gtk::menu_append_item_full(
                            section, _("Show Icon Only"), "win.profiles-button-style", "1", "radio"
                        );
                        orion::gtk::menu_append_item_full(
                            section, _("Show Name Only"), "win.profiles-button-style", "2", "radio"
                        );
                        orion::gtk::menu_append_item_full(
                            section, _("Show Icon and Name"), "win.profiles-button-style", "3", "radio"
                        );

                        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
                    }

                    if (done_cb) {
                        done_cb();
                    }
                });
        });
}