
cmake_minimum_required (VERSION 3.24)

project(core_module)

option(ORION_USE_SYSTEM_DEPS "Use system/vcpkg-provided dependencies instead of building from submodules" OFF)

# Intentionally use two separate options which can be useful in the future
option(ORION_NET_USE_CURL "Use libcurl library as backend for network operations" ON)
option(ORION_NET_USE_SOUP "Use libsoup library as backend for network operations" OFF)
option(ORION_COMMON_USE_GLIB "Use glib library for orion_common" OFF)
IF (WIN32)
  # For Windows use vcpkg to build curl + openssl
  add_subdirectory(cmake/windows_deps)
ENDIF()

add_subdirectory(common)
add_subdirectory(sync)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

include(FetchContent)
include(ExternalProject)
include(GNUInstallDirs)
include(cmake/AddSubmodule.cmake)
include(cmake/EmbedFile.cmake)
include(cmake/PackageWebExtensions.cmake)

include(cmake/sqlite.cmake)
include(cmake/go.cmake)

set(WEBEXTENSIONS_OUTPUT_DIR "${CMAKE_BINARY_DIR}")

orion_package_webextensions(
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extensions"
    OUTPUT_DIR "${WEBEXTENSIONS_OUTPUT_DIR}"
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/core/webextensions/debug_extension_list.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/debug_extension_list.json"
    @ONLY
)

if(ORION_USE_SYSTEM_DEPS)
  find_package(unofficial-sqlite3 CONFIG REQUIRED)
  if(NOT TARGET sqlite3::sqlite3)
    add_library(sqlite3::sqlite3 INTERFACE IMPORTED)
    target_link_libraries(sqlite3::sqlite3 INTERFACE unofficial::sqlite3::sqlite3)
  endif()
else()
  find_package(sqlite3 CONFIG REQUIRED)
endif()

# Function to generate interface bindings from IDL
function(add_interface)
    cmake_parse_arguments(ARG "" "PREFIX" "" ${ARGN})

    if(NOT ARG_PREFIX)
        set(ARG_PREFIX "idl") # default prefix
    endif()

    # Prepend source dir to all IDL files
    set(IDL_FILES "")
    foreach(f ${ARG_UNPARSED_ARGUMENTS})
        list(APPEND IDL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${f})
    endforeach()

    set(CODEGEN_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/core/webkit_bridge/codegen/generate.go)

    # Output files
    set(OUT_API_H       ${CMAKE_CURRENT_BINARY_DIR}/${ARG_PREFIX}_api.h)
    set(OUT_API_H_PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/core/${ARG_PREFIX}_api.h)
    set(OUT_IMPL_CPP    ${CMAKE_CURRENT_BINARY_DIR}/${ARG_PREFIX}_impl.cpp)

    add_custom_command(
        OUTPUT ${OUT_API_H} ${OUT_IMPL_CPP} ${OUT_API_H_PUBLIC}
        COMMAND ${GO_EXECUTABLE} run ${CODEGEN_SCRIPT} --prefix=${ARG_PREFIX} ${IDL_FILES}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/core
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OUT_API_H} ${OUT_API_H_PUBLIC}
        DEPENDS ${IDL_FILES} ${CODEGEN_SCRIPT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating bindings from IDL: ${IDL_FILES}"
        VERBATIM
    )

    # Expose generated files to the caller
    set(GENERATED_V1_FILES
        ${OUT_API_H}
        ${OUT_API_H_PUBLIC}
        ${OUT_IMPL_CPP}
        PARENT_SCOPE
    )
endfunction()

# Generate v2 bindings alongside the original generator. V2 has an independent
# parser, ABI namespace, dispatch table, and JavaScript bridge.
function(add_interface_v2)
    cmake_parse_arguments(ARG "" "PREFIX;VISIBILITY" "" ${ARGN})

    if(NOT ARG_PREFIX)
        set(ARG_PREFIX "idl_v2")
    endif()

    set(IDL_FILES "")
    foreach(f ${ARG_UNPARSED_ARGUMENTS})
        list(APPEND IDL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${f})
    endforeach()

    set(CODEGEN_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/core/webkit_bridge/codegen/generate_v2.go)
    set(OUT_API_H ${CMAKE_CURRENT_BINARY_DIR}/${ARG_PREFIX}_api.h)
    set(OUT_API_H_PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/core/${ARG_PREFIX}_api.h)
    set(OUT_IMPL_CPP ${CMAKE_CURRENT_BINARY_DIR}/${ARG_PREFIX}_impl.cpp)

    set(VISIBILITY_ARGUMENT "")
    if(ARG_VISIBILITY)
        set(VISIBILITY_ARGUMENT --visibility=${ARG_VISIBILITY})
    endif()

    add_custom_command(
        OUTPUT ${OUT_API_H} ${OUT_IMPL_CPP} ${OUT_API_H_PUBLIC}
        COMMAND ${GO_EXECUTABLE} run ${CODEGEN_SCRIPT} --prefix=${ARG_PREFIX} ${VISIBILITY_ARGUMENT} ${IDL_FILES}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/core
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OUT_API_H} ${OUT_API_H_PUBLIC}
        DEPENDS ${IDL_FILES} ${CODEGEN_SCRIPT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating v2 bindings from IDL: ${IDL_FILES}"
        VERBATIM
    )

    set(GENERATED_V2_FILES
        ${OUT_API_H}
        ${OUT_API_H_PUBLIC}
        ${OUT_IMPL_CPP}
        PARENT_SCOPE
    )
endfunction()

add_interface(
    PREFIX webkit_bridge

    core/webkit_bridge/codegen/extension_list.json.idl
    core/webkit_bridge/codegen/manifest.json.idl

    core/webkit_bridge/codegen/browseraction.idl
    core/webkit_bridge/codegen/pageaction.idl
)

add_interface_v2(
    PREFIX webkit_bridge_v2

    core/webkit_bridge/codegen/alarms.idlv2
    core/webkit_bridge/codegen/contextmenus.idlv2
    core/webkit_bridge/codegen/runtime.idlv2
    core/webkit_bridge/codegen/permissions.idlv2
    core/webkit_bridge/codegen/extension.idlv2
    core/webkit_bridge/codegen/webrequest.idlv2
    core/webkit_bridge/codegen/tabs.idlv2
    core/webkit_bridge/codegen/windows.idlv2
    core/webkit_bridge/codegen/action.idlv2
    core/webkit_bridge/codegen/i18n.idlv2
    core/webkit_bridge/codegen/notifications.idlv2
    core/webkit_bridge/codegen/storage.idlv2
    core/webkit_bridge/codegen/webnavigation.idlv2
    core/webkit_bridge/codegen/scripting.idlv2
    core/webkit_bridge/codegen/test.idlv2
)

add_custom_target(orion_core_codegen
    DEPENDS ${GENERATED_V1_FILES} ${GENERATED_V2_FILES}
)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/core/webextensions/private/test_extern.js TEST_EXTERN_JS)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/core/webextensions/private/test_extern.js)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/core/webextensions/private/test_extern_js.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/test_extern_js.h
    @ONLY
)

option(ORION_CORE_BUILD_SHARED "Build orion_core as a shared library" ON)
if(ORION_CORE_BUILD_SHARED)
  set(_orion_core_lib_type SHARED)
else()
  set(_orion_core_lib_type STATIC)
endif()

option(ORION_CORE_BIGOBJ_ENABLED "Build orion_core with /bigobj enabled" ON)
if(ORION_CORE_BIGOBJ_ENABLED)
    if (WIN32)
        set(_orion_core_bigobj_flag /bigobj )
    else()
        # -Wa,-mbig-obj option does not seem to be supported by the assembler
        set(_orion_core_bigobj_flag )
    endif()
endif()

add_library(orion_core ${_orion_core_lib_type}
    ${CMAKE_CURRENT_BINARY_DIR}/webkit_bridge_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/webkit_bridge_v2_impl.cpp
    core/webextensions/internal_store.cpp
    core/webextensions/install_source.cpp
    core/webextensions/runtime.cpp
    core/webextensions/web_view.cpp
    core/webkit_bridge/host_attachment.cpp
    core/webkit_bridge/live_window_tab_model.cpp
    core/webkit_bridge/window_tab_repository.cpp
    core/webkit_bridge/profile_world.cpp
    core/webkit_bridge/profile_bridge.cpp
    core/webkit_bridge/webview_bridge.cpp
    core/webkit_bridge/context_menu_bridge.cpp
    core/webkit_bridge/clipboard_bridge.cpp
    core/webextensions/repository.cpp
    core/webextensions/extension.cpp
    core/webextensions/private/alarms.cpp
    core/webextensions/private/action_v2.cpp
    core/webextensions/private/extension_api.cpp
    core/webextensions/private/permissions.cpp
    core/webextensions/private/runtime.cpp
    core/webextensions/private/webrequest_v2.cpp
    core/webextensions/private/tabs_v2.cpp
    core/webextensions/private/storage.cpp
    core/webextensions/private/i18n.cpp
    core/webextensions/private/test.cpp
    core/webextensions/private/notifications.cpp
    core/webextensions/private/context_menus_v2.cpp
    core/webextensions/private/webnavigation_v2.cpp
    core/webextensions/private/windows.cpp
    core/webextensions/private/scripting_v2.cpp
    core/sync/sync_service.cpp
    core/db/db.cpp
    core/db/db_stmt.cpp
    core/db/db_tx.cpp
    core/db/db_variant.cpp
    core/db/db_migration.cpp
    core/profiles_db/profiles_db.cpp
    core/profiles_db/migrations.cpp
    core/core_api.cpp
    core/filter_list_metadata.cpp
    core/profiles_db/test.cpp
    core/profiles_db/profile.cpp
    core/profiles_db/bookmark.cpp
    core/profiles_db/bookmark_folder.cpp
    core/profiles_db/history_entry.cpp
    core/profiles_db/reading_list_entry.cpp
    core/profiles_db/window.cpp
    core/profiles_db/window_tab_checkpoint.cpp
    core/profiles_db/tab.cpp
    core/profiles_db/tab_back_forward_list_entry.cpp
    core/profiles_db/recently_closed_tab.cpp
    core/profiles_db/recently_closed_window.cpp
    core/profiles_db/setting.cpp
    core/profiles_db/extension.cpp
    core/profiles_db/snapshot.cpp
    core/profiles_db/snapshot_back_forward.cpp
    core/profiles_db/undo_redo_tab_step.cpp
    core/profiles_db/undo_redo_tab.cpp
    core/profiles_db/undo_redo_tab_back_forward_list_entry.cpp
    core/profiles_db/password_item.cpp
    core/profiles_db/extension_storage_area.cpp
    core/profiles_db/download.cpp
    core/profiles_db/search_engine.cpp
    core/profiles_db/filter_list.cpp
    core/profiles_db/overflow_menu_item.cpp
    core/profiles_db/toolbar_item.cpp
    core/profiles_db/programmable_button.cpp
    core/profiles_db/favicon.cpp
    core/profiles_db/favicon_cleanup_service.cpp
    core/profiles_db/download_cleanup_service.cpp
    core/profiles_db/filter_list_update_service.cpp
    core/scripts/edit_text_on_page_scripts.cpp
    core/scripts/allow_copy_and_paste_scripts.cpp
    core/form_analyzer/form.cpp
    core/form_analyzer/form_field.cpp
    core/settings_impl.cpp
    core/download_test.cpp
    core/profiles_db/history_cleanup_service.cpp
    core/bookmark_html_exporter.cpp
    core/bookmark_html_importer.cpp
    core/test/core_api_test.cpp
    core/toolbar_item_util.cpp
)

target_embed_file(orion_core
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/core/webextensions/extension_list.json"
    SYMBOL extension_list_json
)

target_embed_file(orion_core
    INPUT "${CMAKE_CURRENT_BINARY_DIR}/debug_extension_list.json"
    SYMBOL debug_extension_list_json
)

target_compile_options(orion_core
  INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:
      /MP
    >
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
      -Wall
      -Wextra
      -Wpedantic
      -Werror
      -Wno-unused-parameter
      -Wno-unused-function
      -Wno-unused-variable
      -Wno-maybe-uninitialized
      -fexceptions
      -fPIC
    >
)

# if(ORION_NET_USE_CURL AND ORION_NET_USE_SOUP)
#     message(FATAL_ERROR "Cannot use both libcurl and libsoup at the same time at the moment. Please choose one.")
# endif()

# if(NOT ORION_NET_USE_CURL AND NOT ORION_NET_USE_SOUP)
#     message(FATAL_ERROR "At least one of libcurl or libsoup must be used for network operations. Please choose one.")
# endif()

# if(ORION_NET_USE_CURL)
#     target_compile_definitions(orion_core PRIVATE
#         NET_LIB=1
#     )
# elseif(ORION_NET_USE_SOUP)
#     target_compile_definitions(orion_core PRIVATE
#         NET_LIB=0
#     )
# endif()

add_library(orion_core_interface INTERFACE)
add_dependencies(orion_core_interface orion_core_codegen)

set_property(TARGET orion_core PROPERTY CXX_STANDARD 20)

target_compile_options(orion_core_interface
  INTERFACE
    ${_orion_core_bigobj_flag}
    $<$<CXX_COMPILER_ID:MSVC>:
      /MP
    >
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
      -Wall
      -Wextra
      -Wpedantic
      -Werror
      -Wno-unused-parameter
      -Wno-unused-function
      -Wno-unused-variable
      -Wno-maybe-uninitialized
      -fexceptions
      -fPIC
    >
)
target_compile_definitions(orion_core_interface
    INTERFACE
        _HAS_EXCEPTIONS=0
        NOMINMAX=1
)

# uncomment below to enable optimizations to catch more errors/warnings (for exmaple, maybe-uninitialized):
#target_compile_options(${orion_core PRIVATE -O2)

# uncomment 2 lines below to enable address sanitizer (this cannot be used with debugger):
#target_compile_options(orion_core PRIVATE -fsanitize=address -fno-omit-frame-pointer)
#target_link_options(orion_core PRIVATE -fsanitize=address)

target_include_directories(orion_core_interface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(orion_core_interface
    INTERFACE sqlite3::sqlite3
    INTERFACE orion_common
    INTERFACE orion_sync
)

target_link_libraries(orion_core
    PUBLIC orion_core_interface
)

if(DEVELOPER_ID)
    list(APPEND MANIFEST_COMPILE_DEFINITIONS DEVELOPER_ID="${DEVELOPER_ID}")
endif()
if(DEVELOPER_NAME)
    list(APPEND MANIFEST_COMPILE_DEFINITIONS DEVELOPER_NAME="${DEVELOPER_NAME}")
endif()
if(APPLICATION_ID)
    list(APPEND MANIFEST_COMPILE_DEFINITIONS APPLICATION_ID="${APPLICATION_ID}")
endif()
if(APPLICATION_VERSION)
    list(APPEND MANIFEST_COMPILE_DEFINITIONS APPLICATION_VERSION="${APPLICATION_VERSION}")
endif()
if(APPLICATION_COMMAND)
    list(APPEND MANIFEST_COMPILE_DEFINITIONS APPLICATION_COMMAND="${APPLICATION_COMMAND}")
endif()
if(APPLICATION_NAME)
    list(APPEND MANIFEST_COMPILE_DEFINITIONS APPLICATION_NAME="${APPLICATION_NAME}")
endif()
if(APPLICATION_SUMMARY)
    list(APPEND MANIFEST_COMPILE_DEFINITIONS APPLICATION_SUMMARY="${APPLICATION_SUMMARY}")
endif()
if(APPLICATION_IS_BETA)
    list(APPEND MANIFEST_COMPILE_DEFINITIONS APPLICATION_IS_BETA=${APPLICATION_IS_BETA})
endif()
if(GRESOURCE_PREFIX)
    list(APPEND MANIFEST_COMPILE_DEFINITIONS GRESOURCE_PREFIX="${GRESOURCE_PREFIX}")
endif()

target_compile_definitions(orion_core_interface INTERFACE
	$<$<CONFIG:Debug>:_DEBUG>
	$<$<CONFIG:Release>:NDEBUG>
	$<$<CONFIG:Debug>:DEBUG_LOG>
	${MANIFEST_COMPILE_DEFINITIONS}
)

include(GNUInstallDirs)
install(TARGETS
    orion_core
    orion_core_interface
    EXPORT orion_coreConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY core/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/core
    FILES_MATCHING PATTERN "*.h"
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/core/webkit_bridge_api.h
    ${CMAKE_CURRENT_BINARY_DIR}/core/webkit_bridge_v2_api.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/core
)
install(EXPORT orion_coreConfig NAMESPACE orion_core:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/orion_core)
install(TARGETS orion_core)
