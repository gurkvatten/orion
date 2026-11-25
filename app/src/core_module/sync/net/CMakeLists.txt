find_package(CURL 8.0.0 REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

include(FetchContent)
include(ExternalProject)
include(GNUInstallDirs)

#if(NOT WIN32)
#    find_package(PkgConfig REQUIRED)
#    pkg_check_modules(LIBSOUP REQUIRED IMPORTED_TARGET libsoup-3.0>=3.6)
#endif()

IF (WIN32)
  # temporary workaround to export all symbols for Windwos build
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
ENDIF()

add_library(orion_net SHARED
    src/internal/curl_http_client.cpp
    src/internal/soup_http_client.cpp
    src/internal/http_client_intf.cpp
    src/internal/oauth2.cpp
    src/internal/hawk.cpp
    src/internal/jwt.cpp
    src/file_streamer.cpp
    src/net_http_client.cpp
    src/openid.cpp
    test/net_tests.cpp
    test/net_api_tests.cpp
    test/openid_api_tests.cpp
)

target_compile_options(orion_net
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
      -Wno-maybe-uninitialized
      -fexceptions
      -fPIC
    >
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET orion_net PROPERTY CXX_STANDARD 20)
endif()

message("net-include:${CMAKE_INSTALL_PREFIX}")
target_include_directories(orion_net PUBLIC
    "${CMAKE_INSTALL_PREFIX}/include"
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
target_include_directories(orion_net
    PRIVATE ${CURL_INCLUDE_DIRS}
)
#if(NOT WIN32)
#    target_include_directories(orion_net PRIVATE ${LIBSOUP_INCLUDE_DIRS})
#endif()

find_library(ORION_COMMON orion_common ${CMAKE_CURRENT_BINARY_DIR}/../common)

target_link_libraries(orion_net
	PRIVATE orion_common
    PRIVATE CURL::libcurl
    PRIVATE OpenSSL::Crypto
)
#if(NOT WIN32)
#    target_link_libraries(orion_net PRIVATE PkgConfig::LIBSOUP)
#endif()

# This is required for flatpak build
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

target_compile_definitions(orion_net INTERFACE
	$<$<CONFIG:Debug>:_DEBUG>
	$<$<CONFIG:Release>:NDEBUG>
	$<$<CONFIG:Debug>:DEBUG_LOG>
	DEVELOPER_ID="${DEVELOPER_ID}"
	DEVELOPER_NAME="${DEVELOPER_NAME}"
	APPLICATION_ID="${APPLICATION_ID}"
	APPLICATION_COMMAND="${APPLICATION_COMMAND}"
	APPLICATION_NAME="${APPLICATION_NAME}"
	APPLICATION_SUMMARY="${APPLICATION_SUMMARY}"
	GRESOURCE_PREFIX="${GRESOURCE_PREFIX}"
)

if(ORION_NET_USE_CURL AND ORION_NET_USE_SOUP)
    message(FATAL_ERROR "Cannot use both libcurl and libsoup at the same time at the moment. Please choose one.")
endif()

if(NOT ORION_NET_USE_CURL AND NOT ORION_NET_USE_SOUP)
    message(FATAL_ERROR "At least one of libcurl or libsoup must be used for network operations. Please choose one.")
endif()

if(ORION_NET_USE_CURL)
    target_compile_definitions(orion_net PUBLIC
        NET_LIB=1
    )
elseif(ORION_NET_USE_SOUP)
    target_compile_definitions(orion_net PUBLIC
        NET_LIB=0
    )
endif()

install(TARGETS
    orion_net
    EXPORT orion_netConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY net/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/net
    FILES_MATCHING PATTERN "*.h"
)
install(EXPORT orion_netConfig NAMESPACE orion_net:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/orion_net)

install(TARGETS orion_net)
