
include(FetchContent)
include(ExternalProject)
include(GNUInstallDirs)

add_subdirectory(net)

IF (WIN32)
  # temporary workaround to export all symbols for Windwos build
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
ENDIF()

option(ORION_SYNC_BUILD_SHARED "Build orion_sync as a shared library" ON)
if(ORION_SYNC_BUILD_SHARED)
  set(_orion_sync_lib_type SHARED)
else()
  set(_orion_sync_lib_type STATIC)
endif()

add_library(orion_sync ${_orion_sync_lib_type}
    src/internal/file_client.cpp
    src/internal/thread_holder.cpp
    src/internal/workers.cpp
    src/sync_api.cpp
    src/sync_payload.cpp
    src/sync_channel.cpp
    src/win32_socketpair.cpp
 	test/sync_tests.cpp
    test/sync_api_test.cpp
)

target_compile_options(orion_sync
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

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET orion_sync PROPERTY CXX_STANDARD 20)
endif()

message("sync-include:${CMAKE_INSTALL_PREFIX}")
target_include_directories(orion_sync PUBLIC
    "${CMAKE_INSTALL_PREFIX}/include"
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

find_library(ORION_COMMON orion_common ${CMAKE_CURRENT_BINARY_DIR}/../common)

find_library(ORION_NET orion_net ${CMAKE_CURRENT_BINARY_DIR}/net)

add_library(orion_sync_interface INTERFACE)

target_link_libraries(orion_sync_interface
	INTERFACE orion_net
)

target_link_libraries(orion_sync
	PRIVATE orion_common
    PUBLIC orion_sync_interface
)

# This is required for flatpak build
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

target_compile_definitions(orion_sync INTERFACE
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


install(TARGETS
    orion_sync
    orion_sync_interface
    EXPORT orion_syncConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY sync/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sync
    FILES_MATCHING PATTERN "*.h"
)
install(EXPORT orion_syncConfig NAMESPACE orion_sync:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/orion_sync)

install(TARGETS orion_sync)
