#include <vector>
#include <string>

#include "accel.h"
#include "app.h"

namespace orion {

struct ActionAccel
{
	std::string action_name;
	std::vector<std::string> accels;
    GtkPropagationPhase phase = GTK_PHASE_BUBBLE; //currently only supports bubble and capture
};

// See also: gdk/gdkkeysyms.h
static const std::vector<ActionAccel> action_accels
{
	// Main Menu: Orion
	{ "win.settings", {"<Control>comma"} },
	{ "win.settings-for-current-page", {"<Control><Alt>comma"} },
	{ "win.reset-orion", {"<Control><Shift>BackSpace"} },
	{ "win.hide-orion", {"<Control>h"}, GTK_PHASE_CAPTURE },
	{ "win.hide-others", {"<Control><Alt>h"}, GTK_PHASE_CAPTURE },
	{ "win.quit-orion", {"<Control>q"}, GTK_PHASE_CAPTURE },

	// Main Menu: File
	{ "win.new-tab", {"<Control>t"}, GTK_PHASE_CAPTURE },
	{ "win.new-window", {"<Control>n"}, GTK_PHASE_CAPTURE },
	{ "win.new-focus-window", {"<Control>u"}, GTK_PHASE_CAPTURE },
	{ "win.new-private-window", {"<Control><Shift>n"}, GTK_PHASE_CAPTURE },
	{ "win.open-file", {"<Control>o"} },
	{ "win.edit-current-window-group", {"<Control><Shift>s"} },
	{ "win.close-current-window", {"<Control><Shift>w"}, GTK_PHASE_CAPTURE },
	{ "win.close-all-windows", {"<Control><Alt><Shift>w"}, GTK_PHASE_CAPTURE },
	{ "win.close-current-tab", {"<Control>w"}, GTK_PHASE_CAPTURE },
	{ "win.save-page", {"<Control>s"} },
	{ "win.page-setup", {"<Control><Shift>p"} },
	{ "win.print", {"<Control>p"} },

	// Main Menu: Edit
	//{ "win.cut", {"<Control>x"} },
	//{ "win.copy", {"<Control>c"} },
	//{ "win.paste", {"<Control>v"} },
	//{ "win.select-all", {"<Control>a"} },
	{ "win.redo", {"<Control><Shift>z"}, GTK_PHASE_CAPTURE },
	{ "win.copy-page-url", {"<Control><Shift>c"} },
	{ "win.copy-url-as-markdown", {"<Control><Alt><Shift>c"} },
	{ "win.paste-and-match-style", {"<Control><Alt><Shift>v"} },
	{ "win.find", {"<Control>f"} },
	{ "win.find-next", {"F3"} },
	{ "win.find-previous", {"<Shift>F3"} },
	{ "win.use-selection-for-find", {"<Control>e"} },
	{ "win.jump-to-selection", {"<Control>j"} },
	{ "win.show-spelling-and-grammar", {"<Control>colon"} },
	{ "win.check-document-now", {"<Control>semicolon"} },
	// TODO: Emoji & Symbols: fn E

	// Main Menu: View
	{ "win.bookmarks-bar-visibility", {"<Control><Shift>b"} },
	{ "win.toggle-sidebar", {"<Control><Alt>s"}, GTK_PHASE_CAPTURE },
	{ "win.toggle-windows-sidebar", {"<Control><Alt>1"}, GTK_PHASE_CAPTURE },
	{ "win.toggle-vertical-tabs-sidebar", {"<Control><Alt>2"}, GTK_PHASE_CAPTURE },
	{ "win.toggle-bookmarks-sidebar", {"<Control><Alt>3"}, GTK_PHASE_CAPTURE },
	{ "win.toggle-reading-list-sidebar", {"<Control><Alt>4"}, GTK_PHASE_CAPTURE },
	{ "win.toggle-reader", {"<Shift><Super>r"}, GTK_PHASE_CAPTURE }, // Show Reader menu item
	{ "win.show-tab-overview", {"<Control><Super>t"}, GTK_PHASE_CAPTURE }, // cannot use "overview.open" action directly here
	{ "win.show-downloads", {"<Alt><Super>l"} },
	{ "win.toggle-focus-mode", {"<Shift><Super>f"} },
	{ "win.stop", {"Escape"} },
	{ "win.reload-this-page", {"<Control>r", "F5"}, GTK_PHASE_CAPTURE },
	{ "win.reload-current-page-bypass-cache", {"<Control>F5"}, GTK_PHASE_CAPTURE },
	{ "win.reload-all-tabs-in-window", {"<Control><Alt>r"}, GTK_PHASE_CAPTURE },
	{ "win.show-picture-in-picture-video", {"<Shift><Super>p"} },
	{ "win.reset-zoom", {"<Control>0"} },
	{ "win.zoom-in", {"<Control>equal", "<Control>KP_Add"} },
	{ "win.zoom-out", {"<Control>minus", "<Control>KP_Subtract"} },
    { "win.toggle-full-screen", {"F11"}, GTK_PHASE_CAPTURE },

	// Main Menu: History
	{ "win.show-history", {"<Control>y"}, GTK_PHASE_CAPTURE },
	{ "win.back", {"<Control>bracketleft"} },
	{ "win.forward", {"<Control>bracketright"} },
	{ "win.home", {"<Control><Shift>h"} },
	{ "win.reopen-last-closed-tab", {"<Control><Shift>t"}, GTK_PHASE_CAPTURE },

	// Main Menu: Bookmarks
	{ "win.show-bookmarks", {"<Control><Shift>b"}, GTK_PHASE_CAPTURE },
	{ "win.show-reading-list", {"<Alt><Shift>b"}, GTK_PHASE_CAPTURE },
	{ "win.add-bookmark", {"<Control>d"} },
	//{ "win.add-to-reading-list", {"<Control><Shift>d"} },

	// Main Menu: Tools
	{ "win.passwords", {"<Control><Shift>o"}, GTK_PHASE_CAPTURE },
	{ "win.notes", {"<Control><Super>i"}, GTK_PHASE_CAPTURE },
	{ "win.enable-compatibility-mode", {"<Control><Shift>e"}, GTK_PHASE_CAPTURE },
	{ "win.show-fonts", {"<Control><Alt><Shift>f"} },

	// Main Menu: Develop
	{ "win.toggle-web-inspector", {"F12", "<Control><Shift>i"}, GTK_PHASE_CAPTURE },
	{ "win.show-design-tools", {"<Control><Alt><Shift>d"}, GTK_PHASE_CAPTURE },
	{ "win.enter-responsive-design-mode", {"<Control><Super>r"} },
	{ "win.open-console", {"<Alt><Super>c"}, GTK_PHASE_CAPTURE },
	{ "win.open-network", {"<Alt><Super>n"}, GTK_PHASE_CAPTURE },
	{ "win.open-storage", {"<Alt><Super>o"}, GTK_PHASE_CAPTURE },
	{ "win.open-resources", {"<Alt><Super>a"}, GTK_PHASE_CAPTURE },
	{ "win.open-graphics", {"<Alt><Super>g"}, GTK_PHASE_CAPTURE },
	{ "win.start-performance-profile", {"<Alt><Super>p"}, GTK_PHASE_CAPTURE },
	{ "win.edit-cookies", {"<Control><Super>c"}, GTK_PHASE_CAPTURE },
	{ "win.empty-cache", {"<Alt><Super>e"}, GTK_PHASE_CAPTURE },
	{ "win.view-source", {"<Alt><Super>u"}, GTK_PHASE_CAPTURE },

	// Main Menu: Window
	{ "win.minimize", {"<Super>m"}, GTK_PHASE_CAPTURE },
	{ "win.fill", {} }, // TODO: Control+fn+F
	{ "win.center", {} }, // TODO: Control+fn+C
	{ "win.move-left", {} }, // TODO: Control+fn+Left
	{ "win.move-right", {} }, // TODO: Control+fn+Right
	{ "win.move-top", {} }, // TODO: Control+fn+Up
	{ "win.move-bottom", {} }, // TODO: Control+fn+Down
	{ "win.move-left-and-right", {} }, // TODO: Control+Shift+fn+Left
	{ "win.move-right-and-left", {} }, // TODO: Control+Shift+fn+Right
	{ "win.move-top-and-bottom", {} }, // TODO: Control+Shift+fn+Up
	{ "win.move-bottom-and-top", {} }, // TODO: Control+Shift+fn+Down
	{ "win.move-return-to-previous-size", {} }, // TODO: Control+fn+R
	{ "win.show-previous-tab-accelerator", {"<Control><Shift>Tab"}, GTK_PHASE_CAPTURE },
	{ "win.show-next-tab-accelerator", {"<Control>Tab"}, GTK_PHASE_CAPTURE },
	{ "win.go-to-previous-tab-group", {"<Shift><Super>Up"}, GTK_PHASE_CAPTURE },
	{ "win.go-to-next-tab-group", {"<Shift><Super>Down"}, GTK_PHASE_CAPTURE },
	{ "win.pin-tab", {"<Shift><Super>i"}, GTK_PHASE_CAPTURE },
	{ "win.suspend-current-tab", {"<Control><Shift><Super>s"}, GTK_PHASE_CAPTURE },

    // Other action accels
    { "win.select-address-entry", {"<Control>l", "F6"}, GTK_PHASE_CAPTURE },
#ifndef NDEBUG
    { "win.test-action", {"<Control><Alt><Shift>t"}, GTK_PHASE_CAPTURE }, //exists to wire up to whatever code you may need for testing
#endif
};

static void
set_accel_for_action(OrionApp* app, const char* action_name, const std::vector<std::string>& accels, GtkPropagationPhase phase)
{
    orion_app_set_accels_for_action(ORION_APP(app), action_name, accels, phase);
}

void
register_accels(OrionApp* app)
{
	for (const auto& action_accel : action_accels) {
        set_accel_for_action(app, action_accel.action_name.c_str(), action_accel.accels, action_accel.phase);
	}
}

void
undo_tab_close_accel_set_enabled(OrionApp* app, bool is_enabled)
{
    std::vector<std::string> accels{};
    if (is_enabled) {
        accels.push_back("<Control>z");
    }
    set_accel_for_action(app, "win.undo-tab-close", accels, GTK_PHASE_BUBBLE);
}

void
register_ctrl_num_accels(OrionApp* app)
{
    for (int i = 1; i <= 9; i++) {
        std::string action_name = "win.handle-ctrl-num-" + std::to_string(i);
        std::string accel = "<Control>" + std::to_string(i);
        set_accel_for_action(app, action_name.c_str(), std::vector<std::string>{accel}, GTK_PHASE_BUBBLE);
    }
}

} // namespace orion