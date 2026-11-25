cmake_minimum_required (VERSION 3.24)

project(${APPLICATION_COMMAND})

include(FetchContent)
include(ExternalProject)
include(GNUInstallDirs)

set(CMAKE_PREFIX_PATH
	${CMAKE_PREFIX_PATH}
	${CMAKE_INSTALL_PREFIX}
)

set(CMAKE_INSTALL_RPATH
    "${CMAKE_INSTALL_PREFIX}:${CMAKE_INSTALL_PREFIX}/lib64:${CMAKE_INSTALL_PREFIX}/lib"
)

string(REPLACE "." "/" GRESOURCE_PREFIX ".${APPLICATION_ID}")
set(GRESOURCE_XML ${CMAKE_CURRENT_BINARY_DIR}/app.gresource.xml)
set(GRESOURCE_XML_UI ${CMAKE_CURRENT_BINARY_DIR}/app-ui.gresource.xml)
set(GRESOURCE_CPP gresource.cpp)
set(GRESOURCE_CPP_UI gresource_ui.cpp)

add_executable(${APPLICATION_COMMAND}
  ${GRESOURCE_CPP}
  ${GRESOURCE_CPP_UI}
  src/main.cpp
  src/app.cpp
  src/app_downloader.cpp
  src/app_password_storage.cpp
  src/app_profile_data_holder.cpp
  src/platform/libsecret/libsecret.cpp
  src/accel/accel.cpp
  src/util/action_util.cpp
  src/util/dialog_util.cpp
  src/util/data_util.cpp
  src/util/download_cleanup.cpp
  src/util/coro_util.cpp
  src/util/tree_list_row_util.cpp
  src/util/webkit_util.cpp
  src/util/compatibility_mode.cpp
  src/util/click_util.cpp
  src/util/text_util.cpp
  src/util/favicon_util.cpp
  src/util/ref_util.cpp
  src/util/image_util.cpp
  src/util/html_description_util.cpp
  src/util/tab_context_menu.cpp
  src/util/custom_uri_util.cpp
  src/util/window_observer_util.cpp
  src/util/downloads_util.cpp
  src/util/cookies_util.cpp
  src/util/toolbar_util.cpp
  src/util/variant_util.cpp
  src/util/css_util.cpp
  src/util/html_injector.cpp
  src/util/file_util.cpp
  src/util/bookmarks_util.cpp
  src/util/menu_util.cpp
  src/util/profile_manager.cpp
  src/util/hit_test_util.cpp
  src/util/url_or_search_detector.cpp
  src/util/search_engines.cpp
  src/util/stop_cancellable.cpp
  src/util/kagi_service.cpp
  src/util/private_windows.cpp
  src/util/settings_util.cpp
  src/util/debounce_util.cpp
  src/util/uri_util.cpp
  src/util/jsc_util.cpp
  src/util/scroll_util.cpp
  src/action/main_window/action.cpp
  src/action/main_window/action_map.cpp
  src/webextensions/context_menu_host.cpp
  src/webextensions/profile_host.cpp
  src/webextensions/webview_host.cpp
  src/webextensions/clipboard_host.cpp
  src/webextensions/webview_factory_host.cpp
  src/data_item/style_sheet_list.cpp
  src/data_item/overflow_menu_item.cpp
  src/data_item/toolbar_item.cpp
  src/data_item/bookmarks_columnview_item.cpp
  src/data_item/history_columnview_item.cpp
  src/data_item/reading_list_columnview_item.cpp
  src/data_item/profiles_columnview_item.cpp
  src/data_item/passwords_columnview_item.cpp
  src/data_item/search_engines_columnview_item.cpp
  src/data_item/search_completion_columnview_item_full.cpp
  src/data_item/wrapping_item.cpp
  src/data_item/g_string_item.cpp
  src/data_item/int64_item.cpp
  src/data_item/tab_item.cpp
  src/data_item/extension_item.cpp
  src/data_item/popular_extension_item.cpp
  src/data_item/window_item.cpp
  src/data_item/g_string_and_icon_name_item.cpp
  src/data_item/add_bookmark_location_dropdown_item.cpp
  src/data_item/downloads_item.cpp
  src/data_item/menu_item.cpp
  src/data_item/back_forward_list.cpp
  src/data_item/website_data_storage_item.cpp
  src/window/url_image/url_image.cpp
  src/window/columnview/overflow_menu_listview.cpp
  src/window/columnview/orion_window_selection_model.cpp
  src/window/columnview/orion_tab_selection_model.cpp
  src/window/columnview/orion_filter_selection_model.cpp
  src/window/columnview/search_completion_model.cpp
  src/window/columnview/orion_tab_selection_model_expander.cpp
  src/window/columnview/downloads_item_widget.cpp
  src/window/settings_dialog/dropdown_settings.cpp
  src/window/settings_dialog/entry_settings.cpp
  src/window/settings_dialog/check_button_settings.cpp
  src/window/settings_dialog/radio_button_settings.cpp
  src/window/accelerator_widget/orion_accelerator_entry.cpp
  src/window/editable_cell_widget/orion_editable_cell_entry.cpp
  src/window/orion_webview_tabview_widgets/orion_webview_tab_content.cpp
  src/window/orion_webview_tabview_widgets/orion_webview_tab_view.cpp
  src/window/orion_webview_tabview_widgets/orion_address_entry.cpp
  src/window/orion_webview_tabview_widgets/orion_webkit_webview.cpp
  src/window/orion_webview_tabview_widgets/orion_webview_tab_container.cpp
  src/window/orion_webview_tabview_widgets/orion_webview_preview_window.cpp
  src/window/columnview/bookmarks_columnview.cpp
  src/window/columnview/bookmarks_bar_listview.cpp
  src/window/columnview/history_columnview.cpp
  src/window/columnview/reading_list_columnview.cpp
  src/window/columnview/profiles_columnview.cpp
  src/window/columnview/passwords_columnview.cpp
  src/window/columnview/search_engines_columnview.cpp
  src/window/columnview/window_list_synced_devices_listview.cpp
  src/window/columnview/extensions_listview.cpp
  src/window/main_window/main_window.cpp
  src/window/main_window/orion_window_iface.cpp
  src/window/main_window/header/header_bar.cpp
  src/window/main_window/header/bookmarks_bar.cpp
  src/window/main_window/header/find_bar.cpp
  src/window/main_window/header/erase_elements_bar.cpp
  src/window/main_window/header/tab_bar.cpp
  src/window/main_window/header/downloads_button_content.cpp
  src/window/main_window/header/programmable_button.cpp
  src/window/main_window/sidebar/sidebar_reading_list_list_view.cpp
  src/window/main_window/sidebar/reading_list_toggle_group.cpp
  src/window/main_window/sidebar/sidebar.cpp
  src/window/main_window/sidebar/sidebar_bookmarks_columnview.cpp
  src/window/main_window/sidebar/sidebar_current_window_tabs_view.cpp
  src/window/main_window/sidebar/sidebar_windows_list_view.cpp
  src/window/main_window/js_sidebar/js_sidebar.cpp
  src/window/orion_gizmo.cpp
  src/window/about_dialog.cpp
  src/window/settings_dialog.cpp
  src/window/reset_orion_dialog.cpp
  src/window/check_for_updates_dialog.cpp
  src/window/software_update_dialog.cpp
  src/window/settings_for_website_dialog.cpp
  src/window/bookmarks_dialog.cpp
  src/window/updating_orion_dialog.cpp
  src/window/up_to_date_dialog.cpp
  src/window/get_orion_plus_dialog.cpp
  src/window/log_in_to_kagi_dialog.cpp
  src/window/import_browser_data_dialog.cpp
  src/window/import_from_browser_dialog.cpp
  src/window/import_from_extension_dialog.cpp
  src/window/default_font_dialog.cpp
  src/window/add_password_dialog.cpp
  src/window/profile_dialog.cpp
  src/window/save_window_popover.cpp
  src/window/search_popover.cpp
  src/window/search_completion_popover.cpp
  src/window/customize_theme_popover.cpp
  src/window/page_tweaks_popover.cpp
  src/window/privacy_popover.cpp
  src/window/rss_feeds_popover.cpp
  src/window/install_site_as_app_dialog.cpp
  src/window/add_bookmarks_for_tabs_dialog.cpp
  src/window/popular_extensions_dialog.cpp
  src/window/add_bookmark_dialog.cpp
  src/window/downloads_dialog.cpp
  src/window/extension_popover.cpp
  src/window/move_downloads_to_trash_dialog.cpp
  src/window/allow_downloads_dialog.cpp
  src/window/window_list_popover.cpp
  src/window/window_list_box.cpp
  src/window/add_web_extension_dialog.cpp
  src/window/clear_history_dialog.cpp
  src/window/yes_cancel_dialog.cpp
  src/window/import_from_url_dialog.cpp
  src/window/certificate_info_dialog.cpp
  src/window/customize_toolbar_dialog.cpp
  src/window/overflow_menu_popover.cpp
  src/window/extensions_keyboard_shortcuts_dialog.cpp
  src/window/error_url_dialog.cpp
  src/window/summary_unavailable_dialog.cpp
  src/window/import_profile_data_dialog.cpp
  src/window/export_profile_data_dialog.cpp
  src/window/add_website_dialog.cpp
  src/window/add_website_play_dialog.cpp
  src/window/edit_bookmark_dialog.cpp
  src/window/low_power_mode_dialog.cpp
  src/window/start_page_settings_popover.cpp
  src/window/programmable_button_popover.cpp
  src/window/search_recommendations.cpp
  src/window/profiles_icon_selection_box.cpp
  src/window/crash_reporter_dialog.cpp
  src/window/main_window/menu/profile_windows_submenu.cpp
  src/window/main_window/menu/profiles_button_menu.cpp
  src/window/main_window/menu/new_tab_menu.cpp
  src/window/main_window/menu/toolbar_menu.cpp
  src/window/scaled_container/scaled_container.cpp
  src/window/scaled_container/forced_size_container.cpp
  src/window/scaled_container/max_width_container.cpp
  src/window/import_csv_passwords_dialog.cpp
  src/window/delete_csv_passwords_dialog.cpp
  src/window/quit_orion_dialog.cpp
  src/window/import_html_bookmarks_dialog.cpp
  src/window/reader_mode_settings_popover.cpp
  src/config/default_config.cpp
  src/window/manage_search_engines_dialog.cpp
  src/window/add_search_engine_dialog.cpp
  src/window/filter_lists_dialog.cpp
  src/window/custom_filter_url_dialog.cpp
  src/window/manage_website_data_dialog.cpp
  src/window/invalid_setup_key_dialog.cpp
  src/window/delete_verification_code_dialog.cpp
  src/window/edit_password_dialog.cpp
  src/test/file_download_test.cpp
  src/window/auth_dialog.cpp
  src/window/permissions_request_dialog.cpp
  src/window/permissions_request_binary_dialog.cpp
  src/window/remember_password_dialog.cpp
  src/window/update_password_dialog.cpp
  src/window/orion_webview_tabview_widgets/error_page_helper.cpp
  src/window/orion_webview_tabview_widgets/certificate_error_page_helper.cpp
  src/window/visit_unsecure_website_dialog.cpp
  src/window/submit_non_secure_form_dialog.cpp
  src/window/orion_webview_tabview_widgets/context_menu_helper.cpp
  src/window/orion_webview_tabview_widgets/permission_handler.cpp
  src/window/main_menu/main_menu.cpp
  src/window/fill_password_dialog.cpp
  src/window/orion_webview_tabview_widgets/scripts/safeguard_scripts.cpp
  src/window/orion_webview_tabview_widgets/scripts/page_tweaks_scripts.cpp
  src/window/orion_webview_tabview_widgets/scripts/autofill_scripts.cpp
  src/window/orion_webview_tabview_widgets/scripts/console_redirect_scripts.cpp
  src/window/orion_webview_tabview_widgets/scripts/find_selection_scripts.cpp
  src/window/orion_webview_tabview_widgets/scripts/certificate_warning_scripts.cpp
  src/window/orion_webview_tabview_widgets/scripts/orion_internals_scripts.cpp

  src/window/tab_switcher.cpp
  src/window/set_default_search_engine_dialog.cpp
  src/window/scaled_container/popover_listview_container.cpp
  src/content_block/embedded_filter_lists.cpp
  src/content_block/filter_store.cpp
  src/global_core_api.cpp
)

target_include_directories(${APPLICATION_COMMAND}
	PRIVATE src
    PRIVATE src/core_module
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${APPLICATION_COMMAND} PROPERTY CXX_STANDARD 20)
endif()

target_compile_options(${APPLICATION_COMMAND}
    # disable exceptions to detect dependencies that use them (avoid such dependencies)
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wno-unused-parameter
        -Wno-unused-function
        -Wno-unused-variable
        -Wno-maybe-uninitialized
        -fno-exceptions
)

# uncomment below to enable optimizations to catch more errors/warnings (for example, maybe-uninitialized):
#target_compile_options(${APPLICATION_COMMAND} PRIVATE -O2 -flto)

# uncomment 2 lines below to enable address sanitizer (this cannot be used with debugger):
#target_compile_options(${APPLICATION_COMMAND} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
#target_link_options(${APPLICATION_COMMAND} PRIVATE -fsanitize=address)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.18)
pkg_check_modules(LIBADWAITA1 REQUIRED IMPORTED_TARGET libadwaita-1>=1.7)
pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
pkg_check_modules(PANGOFC REQUIRED IMPORTED_TARGET pangofc)


set(LIBBACKTRACE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/core_module/third_party/libbacktrace)
set(UNIFDEF_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/core_module/third_party/unifdef)
set(WEBKIT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/core_module/third_party/WebKit)

include(src/core_module/cmake/gtk/LibBacktrace.cmake)
include(src/core_module/cmake/gtk/Unifdef.cmake)
include(src/core_module/cmake/gtk/WebKit.cmake)

if(WebKit_FOUND)
    target_include_directories(${APPLICATION_COMMAND}
        PRIVATE ${LIBSOUP_INCLUDE_DIRS}
    )
    target_link_libraries(${APPLICATION_COMMAND}
        PRIVATE ${LIBSOUP_LIBRARIES}
        PRIVATE WebKit
    )
else()
    pkg_check_modules(LIBSOUP REQUIRED IMPORTED_TARGET libsoup-3.0>=3.6)
    target_link_libraries(${APPLICATION_COMMAND}
        PRIVATE PkgConfig::LIBSOUP
    )
endif()

message(LIBADWAITA1_VERSION=${LIBADWAITA1_VERSION})

add_subdirectory(src)

add_dependencies(${APPLICATION_COMMAND} webextensions)

target_link_libraries(${APPLICATION_COMMAND}
    PRIVATE PkgConfig::GTK4
    PRIVATE PkgConfig::LIBADWAITA1
    PRIVATE PkgConfig::LIBSECRET
    PRIVATE PkgConfig::FONTCONFIG
    PRIVATE PkgConfig::PANGOFC
    orion_core
)

set_target_properties(${APPLICATION_COMMAND} PROPERTIES
	LINK_WHAT_YOU_USE 1
	OUTPUT_NAME "${APPLICATION_COMMAND}"
)

# Blueprint -> UI conversion -> GResource conversion

find_program(GLIB_COMPILE_RESOURCES NAMES glib-compile-resources REQUIRED)

set(BLUEPRINT_FILES
    src/window/search_recommendations.blp
	src/window/main_window/main_window.blp
    src/window/main_window/header/header_bar.blp
    src/window/main_window/header/bookmarks_bar.blp
    src/window/main_window/header/erase_elements_bar.blp
    src/window/main_window/header/find_bar.blp
    src/window/main_window/sidebar/sidebar.blp
    src/window/main_window/sidebar/context_menus.blp
    src/window/main_window/js_sidebar/js_sidebar.blp
	src/window/main_menu/main_menu.blp
    src/window/main_window_context_menus.blp
	src/window/about_dialog.blp
	src/window/settings_dialog.blp
	src/window/reset_orion_dialog.blp
	src/window/check_for_updates_dialog.blp
	src/window/software_update_dialog.blp
	src/window/settings_for_website_dialog.blp
	src/window/bookmarks_dialog.blp
	src/window/updating_orion_dialog.blp
	src/window/up_to_date_dialog.blp
   	src/window/get_orion_plus_dialog.blp
    src/window/log_in_to_kagi_dialog.blp
    src/window/import_browser_data_dialog.blp
    src/window/import_from_browser_dialog.blp
    src/window/import_from_extension_dialog.blp
    src/window/default_font_dialog.blp
    src/window/add_password_dialog.blp
   	src/window/install_site_as_app_dialog.blp
   	src/window/add_bookmarks_for_tabs_dialog.blp
    src/window/search_popover.blp
    src/window/search_completion_popover.blp
    src/window/customize_theme_popover.blp
    src/window/page_tweaks_popover.blp
    src/window/rss_feeds_popover.blp
    src/window/rss_feeds_popover_item.blp
    src/window/popular_extensions_dialog.blp
    src/window/popular_extensions_dialog_item.blp
	src/window/add_bookmark_dialog.blp
	src/window/reader_mode_unavailable_popover.blp
	src/window/downloads_dialog.blp
    src/window/extension_popover.blp
	src/window/move_downloads_to_trash_dialog.blp
	src/window/allow_downloads_dialog.blp
    src/window/window_list_popover.blp
    src/window/window_list_box.blp
    src/window/add_web_extension_dialog.blp
    src/window/clear_history_dialog.blp
    src/window/yes_cancel_dialog.blp
    src/window/import_from_url_dialog.blp
    src/window/certificate_info_dialog.blp
    src/window/overflow_menu_popover.blp
    src/window/extensions_keyboard_shortcuts_dialog.blp
    src/window/error_url_dialog.blp
    src/window/summary_unavailable_dialog.blp
    src/window/import_profile_data_dialog.blp
    src/window/export_profile_data_dialog.blp
    src/window/add_website_dialog.blp
    src/window/add_website_play_dialog.blp
    src/window/edit_bookmark_dialog.blp
    src/window/low_power_mode_dialog.blp
    src/window/start_page_settings_popover.blp
    src/window/crash_reporter_dialog.blp
    src/window/manage_search_engines_dialog.blp
    src/window/add_search_engine_dialog.blp
    src/window/filter_lists_dialog.blp
    src/window/custom_filter_url_dialog.blp
    src/window/manage_website_data_dialog.blp
    src/window/bookmarks_dialog_profiles_popover_menu.blp
    src/window/profile_dialog.blp
    src/window/invalid_setup_key_dialog.blp
    src/window/delete_verification_code_dialog.blp
    src/window/edit_password_dialog.blp
    src/window/auth_dialog.blp
    src/window/permissions_request_dialog.blp
    src/window/permissions_request_binary_dialog.blp
    src/window/remember_password_dialog.blp
    src/window/update_password_dialog.blp
    src/window/visit_unsecure_website_dialog.blp
    src/window/submit_non_secure_form_dialog.blp
    src/window/import_csv_passwords_dialog.blp
    src/window/delete_csv_passwords_dialog.blp
    src/window/main_window/header/tab_bar.blp
    src/window/fill_password_dialog.blp
    src/window/tab_switcher.blp
    src/window/columnview/downloads_item_widget.blp
    src/window/quit_orion_dialog.blp
    src/window/import_html_bookmarks_dialog.blp
    src/window/orion_webview_tabview_widgets/orion_webview_preview_window.blp
    src/window/reader_mode_settings_popover.blp
)

if(LIBADWAITA1_VERSION VERSION_GREATER_EQUAL "1.7")
   list(APPEND BLUEPRINT_FILES src/window/privacy_popover.blp)
   list(APPEND BLUEPRINT_FILES src/window/save_window_popover.blp)
   list(APPEND BLUEPRINT_FILES src/window/customize_toolbar_dialog.blp)
   list(APPEND BLUEPRINT_FILES src/window/main_window/sidebar/reading_list_toggle_group.blp)
   list(APPEND BLUEPRINT_FILES src/window/programmable_button_popover.blp)
   list(APPEND BLUEPRINT_FILES src/window/profiles_icon_selection_box.blp)
   add_compile_definitions(ENABLE_ADW_17_API)
else()
   # Older Adwaita version
   list(APPEND BLUEPRINT_FILES src/window/privacy_popover.compat.blp)
   list(APPEND BLUEPRINT_FILES src/window/save_window_popover.compat.blp)
   list(APPEND BLUEPRINT_FILES src/window/customize_toolbar_dialog.compat.blp)
   list(APPEND BLUEPRINT_FILES src/window/main_window/sidebar/reading_list_toggle_group.compat.blp)
   list(APPEND BLUEPRINT_FILES src/window/programmable_button_popover.compat.blp)
   list(APPEND BLUEPRINT_FILES src/window/profiles_icon_selection_box.compat.blp)
endif()

list(GET BLUEPRINT_FILES 0 MAIN_BLUEPRINT_FILE)

add_custom_command(
	OUTPUT ${GRESOURCE_CPP_UI}
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND mkdir data/ui/ -p
	VERBATIM
	MAIN_DEPENDENCY ${MAIN_BLUEPRINT_FILE}
	DEPENDS ${BLUEPRINT_FILES} ${GRESOURCE_XML_UI}
)

function(add_blueprint_file BLP_FILE XML_FILES)
	string(REGEX REPLACE "[.]blp$" ".ui" UI_FILE ${BLP_FILE})

    # Remove compatibility tags from UI files
    string(REGEX REPLACE "[.]compat[.]" "." UI_FILE ${UI_FILE})

    list(APPEND ${XML_FILES} ${UI_FILE})
    set(${XML_FILES} "${${XML_FILES}}" PARENT_SCOPE)

	cmake_path(GET UI_FILE PARENT_PATH UI_DIR)
	#message("add_blueprint_file(): " ${BLP_FILE} " -> " ${UI_FILE})

    set(DEPENDENCY ${PROJECT_SOURCE_DIR}/${BLP_FILE})

	add_custom_command(
		OUTPUT ${GRESOURCE_CPP_UI} APPEND
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMAND mkdir ${CMAKE_CURRENT_BINARY_DIR}/${UI_DIR} -p && python3 ${PROJECT_SOURCE_DIR}/tools/blueprint-compiler/blueprint-compiler.py
			compile --output ${CMAKE_CURRENT_BINARY_DIR}/${UI_FILE} ${PROJECT_SOURCE_DIR}/${BLP_FILE}
		DEPENDS ${DEPENDENCY}
	)
endfunction()

foreach (BLP_FILE ${BLUEPRINT_FILES})
	add_blueprint_file(${BLP_FILE} GRESOURCE_XML_FILES)
endforeach()

set(XML_GLUE_OPEN "<file compressed=\"true\">")
set(XML_GLUE_CLOSE "</file>")
list(JOIN GRESOURCE_XML_FILES ${XML_GLUE_CLOSE}${XML_GLUE_OPEN} GRESOURCE_XML_FILES_GLUED)
set(GRESOURCE_XML_FILES_RESULT ${XML_GLUE_OPEN}${GRESOURCE_XML_FILES_GLUED}${XML_GLUE_CLOSE})

# We need app metainfo file for AdwAboutDialog (adw_about_dialog_new_from_appdata) to work properly
configure_file(../cmake/app.metainfo.xml.in app.metainfo.xml)

configure_file(data/app.gresource.xml.in app.gresource.xml)
configure_file(data/app-ui.gresource.xml.in app-ui.gresource.xml)

add_custom_command(
	OUTPUT ${GRESOURCE_CPP}
	COMMAND ${GLIB_COMPILE_RESOURCES}
		--target=${GRESOURCE_CPP}
		--generate-source
		--sourcedir=${PROJECT_SOURCE_DIR}
        --sourcedir=${CMAKE_CURRENT_BINARY_DIR}
        --dependency-file=${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_CPP}.d
		${GRESOURCE_XML}
    MAIN_DEPENDENCY ${GRESOURCE_XML}
    DEPFILE ${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_CPP}.d
    VERBATIM
)

add_custom_command(
	OUTPUT ${GRESOURCE_CPP_UI} APPEND
	COMMAND ${GLIB_COMPILE_RESOURCES}
		--target=${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_CPP_UI}
		--generate-source
		--sourcedir=${CMAKE_CURRENT_BINARY_DIR}
		${GRESOURCE_XML_UI}
)

add_custom_target(
	dummy-gresource
	DEPENDS ${GRESOURCE_CPP}
)

add_custom_target(
	dummy-gresource-ui
	DEPENDS ${GRESOURCE_CPP_UI}
)

set_source_files_properties(
	${GRESOURCE_CPP}
	PROPERTIES GENERATED TRUE
)

set_source_files_properties(
	${GRESOURCE_CPP_UI}
	PROPERTIES GENERATED TRUE
)

add_custom_target(
	dummy-gresource-c
	DEPENDS dummy-gresource
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_CPP}
)

add_custom_target(
	dummy-gresource-c-ui
	DEPENDS dummy-gresource-ui
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_CPP_UI}
)

set_source_files_properties(
	${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_CPP}
	PROPERTIES GENERATED TRUE
)

set_source_files_properties(
	${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_CPP_UI}
	PROPERTIES GENERATED TRUE
)

target_compile_definitions(${APPLICATION_COMMAND} PUBLIC
	$<$<CONFIG:Debug>:_DEBUG>
	$<$<CONFIG:Release>:NDEBUG>
	$<$<CONFIG:Debug>:DEBUG_LOG>
	DEVELOPER_ID="${DEVELOPER_ID}"
	DEVELOPER_NAME="${DEVELOPER_NAME}"
	APPLICATION_ID="${APPLICATION_ID}"
    APPLICATION_VERSION="${APPLICATION_VERSION}"
	APPLICATION_COMMAND="${APPLICATION_COMMAND}"
	APPLICATION_NAME="${APPLICATION_NAME}"
	APPLICATION_SUMMARY="${APPLICATION_SUMMARY}"
    APPLICATION_IS_BETA=${APPLICATION_IS_BETA}
	GRESOURCE_PREFIX="${GRESOURCE_PREFIX}"
)

if(FLATPAK_BUILD)
	target_compile_definitions(${APPLICATION_COMMAND} PUBLIC
		FLATPAK_BUILD=${FLATPAK_BUILD}
	)
endif()

add_dependencies(${APPLICATION_COMMAND} dummy-gresource-c)
add_dependencies(${APPLICATION_COMMAND} dummy-gresource-c-ui)
add_dependencies(${APPLICATION_COMMAND} webextensions)

install(TARGETS ${APPLICATION_COMMAND})

configure_file(cmake/app.gdbserver.sh.in app.gdbserver.sh)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/app.gdbserver.sh
    TYPE BIN
	PERMISSIONS
		OWNER_EXECUTE OWNER_READ OWNER_WRITE
		GROUP_EXECUTE GROUP_READ
		WORLD_EXECUTE WORLD_READ
)
