# flatpak reverse domain
set(DEVELOPER_ID "com.kagi")
set(DEVELOPER_NAME "Kagi")

set(APPLICATION_ID_SUFFIX "Orion")
set(NATIVE_APPLICATION_ID_SUFFIX "native")

# lower case, no dashes
set(APPLICATION_COMMAND "oriongtk")

# Note: Set this to "Orion" when building stable release versions
set(APPLICATION_NAME "Orion Beta")

# Note: When the version string is in the form "0.1.0-dev", GNOME Software will only show "0.1.0" and the description
# of Release Notes will be displayed as empty.
set(APPLICATION_VERSION "0.4.3")

# Note: Set this to FALSE when building stable release versions
set(APPLICATION_IS_BETA TRUE)

set(APPLICATION_SUMMARY "Orion Web Browser by Kagi")

set(APPLICATION_HOMEPAGE "https://kagi.com/orion/")
set(APPLICATION_BUGTRACKER "https://orionfeedback.org/")

set(APPICATION_SCREENSHOTS
    "https://storage.googleapis.com/prod-kagi-lab-kagi-cms/Orion_StartPage_system.png"
    "https://storage.googleapis.com/prod-kagi-lab-kagi-cms/Orion_Kagi_dark.png"
    "https://storage.googleapis.com/prod-kagi-lab-kagi-cms/Orion_Settings_dark.png"
    "https://storage.googleapis.com/prod-kagi-lab-kagi-cms/Orion_WebsiteSettings_dark.png"
)

string(TIMESTAMP APPLICATION_BUILD_TIMESTAMP)

# Note: GNOME Software expects date format to be YYYY-MM-DD, not full timestamp.
string(TIMESTAMP APPLICATION_BUILD_DATE "%Y-%m-%d")

set(FLATPAK_RUNTIME "org.gnome.Platform")
set(FLATPAK_RUNTIME_VERSION "51")
set(FLATPAK_SDK "org.gnome.Sdk")
set(FLATPAK_DEBUG_PORT "23579")

include(UserManifest.cmake OPTIONAL)

# flatpak reverse domain
set(APPLICATION_ID "${DEVELOPER_ID}.${APPLICATION_ID_SUFFIX}")

if(NOT FLATPAK_HOST AND NOT FLATPAK_BUILD)
    set(APPLICATION_ID "${APPLICATION_ID}.${NATIVE_APPLICATION_ID_SUFFIX}")
endif()

