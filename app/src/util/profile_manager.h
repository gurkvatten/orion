#pragma once

#include <core/core_api.h>

namespace orion {

namespace profile {

enum class FocusMode { no, yes };

/**
 * @brief Opens a profile by loading all its windows from the stored session
 * 
 * This will check the profile's app_opens_with setting and either:
 * - Restore all windows from last session (if configured)
 * - Open a new tab group (if configured to start fresh)
 * 
 * @param profile_id The profile to open
 */
void open(orion::ProfileId profile_id);

/**
 * @brief Opens a profile from a snapshot
 * 
 * Similar to open() but specifically for restoring from a snapshot state.
 * 
 * @param profile_id The profile to open
 */
void open_from_snapshot(orion::ProfileId profile_id);

// Complementary operation to open / open_from_snapshot
void close(orion::ProfileId profile_id);

// Closes all open profiles, see also: close(ProfileId)
void close_all();

/**
 * @brief Opens a new tab group window for the specified profile
 * 
 * Creates a new backend window with is_tab_group=true and opens
 * a UI window for it.
 * 
 * @param profile_id The profile to create the window for
 * @param focus_mode The focus mode for the new window
 * @param private_mode The private mode for the new window
 */
void open_new_window(orion::ProfileId profile_id, FocusMode focus_mode, PrivateMode private_mode);

// The same as open_new_window, but also opens a specific URI instead of default Start Page
void open_new_window_with_uri(
    orion::ProfileId profile_id, FocusMode focus_mode, PrivateMode private_mode, const std::string& uri);

/**
 * @brief Opens an existing backend window for the specified profile
 * 
 * Creates a UI window to display an existing backend window.
 * 
 * @param profile_id The profile that owns the window
 * @param window_id The backend window ID to open
 */
void open_window(orion::ProfileId profile_id, orion::WindowId window_id, PrivateMode private_mode);

/**
 * @brief Quits a profile by closing all its windows and cleaning up resources
 * 
 * This will:
 * - Close all UI windows belonging to the profile
 * - Optionally delete backend tab groups (based on app_opens_with setting)
 * - Clean up profile-specific resources (factories, runtimes, kagi services)
 * 
 * @param profile_id The profile to quit
 */
void quit(orion::ProfileId profile_id);

/**
 * @brief Internal callback when a window is finalized
 * 
 * Checks if this was the last window for the profile, and if so,
 * automatically quits the profile.
 * 
 * This should be called from main_window_finalize().
 * Should be called before CoreApi::unregister_ui_window is called.
 * 
 * @param profile_id The profile that owned the finalized window
 * @param window_id The backend window ID that was finalized
 */
void on_window_finalized(orion::ProfileId profile_id, orion::WindowId window_id, bool has_windows_for_profile);

/**
 * @brief Checks if a profile should preserve its windows on quit
 * 
 * This is used by main_window_finalize() to determine whether to
 * keep backend windows for session restoration.
 * 
 * @param profile_id The profile to check
 * @return true if windows should be preserved, false otherwise
 */
bool should_preserve_windows_on_quit(orion::ProfileId profile_id);

// Finds a unique (translated) window name for a new (potential) window. The name is based on the pattern:
//   - "Window N" for regular windows
//   - "Private Window N" for private windows
void find_unique_window_name(ProfileId profile_id, PrivateMode private_mode, std::function<void(std::string)> cb);

orion::settings::AppOpensWith get_app_opens_with(orion::ProfileId profile_id);

// Returns true if there is at least one UI window registered for the given profile_id
bool has_windows_for_profile(orion::ProfileId profile_id);

} // namespace profile
} // namespace orion
