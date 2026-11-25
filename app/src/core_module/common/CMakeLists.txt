cmake_minimum_required (VERSION 3.24)

include(FetchContent)
include(ExternalProject)
include(GNUInstallDirs)
include(${PROJECT_SOURCE_DIR}/cmake/AddSubmodule.cmake)


IF (WIN32)
  # temporary workaround to export all symbols for Windwos build
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
ENDIF()



add_submodule(zlib third_party/zlib -DZLIB_BUILD_TESTING=0 -DZLIB_BUILD_SHARED=0)
find_package(ZLIB CONFIG REQUIRED)
set(_zlib_tgt "")
if(TARGET ZLIB::ZLIBSTATIC)
  set(_zlib_tgt ZLIB::ZLIBSTATIC)
elseif(TARGET ZLIB::ZLIB)
  set(_zlib_tgt ZLIB::ZLIB)
else()
  message(FATAL_ERROR "ZLIB config found, but no ZLIB::ZLIB or ZLIB::ZLIBSTATIC target exists.")
endif()

get_target_property(ZLIB_INCLUDE_DIR ${_zlib_tgt} INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(ZLIB_LIBRARY_RELEASE ${_zlib_tgt} IMPORTED_LOCATION_RELEASE)
get_target_property(ZLIB_LIBRARY_DEBUG ${_zlib_tgt} IMPORTED_LOCATION_DEBUG)
get_target_property(ZLIB_LIBRARY ${_zlib_tgt} IMPORTED_LOCATION)

add_submodule(libuv third_party/libuv/libuv -DBUILD_TESTING=0 -DLIBUV_BUILD_SHARED=0)
find_package(libuv CONFIG REQUIRED)
if(TARGET libuv::uv_a)
  set(_libuv_tgt libuv::uv_a)
elseif(TARGET libuv::uv)
  set(_libuv_tgt libuv::uv)
else()
  message(FATAL_ERROR "libuv config found, but no libuv::uv_a or libuv::uv target exists.")
endif()

add_submodule(libzip third_party/libzip/libzip
    -DZLIB_FOUND=1
    -DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIR}
    -DZLIB_LIBRARY=${ZLIB_LIBRARY}
    -DZLIB_LIBRARY_DEBUG=${ZLIB_LIBRARY_DEBUG}
    -DZLIB_LIBRARY_RELEASE=${ZLIB_LIBRARY_RELEASE}
    # This is required for flatpak build
    -DBUILD_SHARED_LIBS=0

    # Optional compression backends are not needed and may be picked up from vcpkg
    -DENABLE_BZIP2=0
    -DENABLE_LZMA=0
    -DENABLE_ZSTD=0

    # Disable crypto
    -DENABLE_COMMONCRYPTO=0
    -DENABLE_GNUTLS=0
    -DENABLE_MBEDTLS=0
    -DENABLE_OPENSSL=0
    -DENABLE_WINDOWS_CRYPTO=0

    # Disable other stuff
    -DBUILD_TOOLS=0
    -DBUILD_REGRESS=0
    -DBUILD_OSSFUZZ=0
    -DBUILD_EXAMPLES=0
    -DBUILD_DOC=0
)
find_package(libzip CONFIG REQUIRED)

add_submodule(pugixml third_party/pugixml)

add_submodule(reflectcpp third_party/reflectcpp
    -DCMAKE_CXX_STANDARD=20
    -DREFLECTCPP_JSON=ON
    -DREFLECTCPP_UBJSON=OFF
    -DREFLECTCPP_XML=ON
)
find_package(reflectcpp CONFIG REQUIRED)

option(ORION_COMMON_BUILD_SHARED "Build orion_common as a shared library" ON)
if(ORION_COMMON_BUILD_SHARED)
  set(_orion_common_lib_type SHARED)
else()
  set(_orion_common_lib_type STATIC)
endif()

add_library(orion_common ${_orion_common_lib_type}
	src/conversion_util.cpp
    src/enum_reflect_test.cpp
	src/observable.cpp
	src/time_util.cpp
	src/timer.cpp
    src/thread_util.cpp
    src/type_info.cpp
    src/uri.cpp
    src/error.cpp
    src/log.cpp
    src/string_util.cpp
    src/csv_util.cpp
    src/json_util.cpp
    src/file_system.cpp
    src/dispatch.cpp
    src/run_loop.cpp
    src/dispatch_test.cpp
    src/uuid.cpp
    src/zip_test.cpp
    src/zipper.cpp
    src/zlib_util.cpp
    src/crypto.cpp
    src/crypto_test.cpp
    src/dto/totp.cpp
    src/punycode.cpp
    src/punycode_test.cpp
    src/filter_list.cpp
    src/format_util.cpp
)

target_compile_options(orion_common
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
       -fPIC
    >
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET orion_common PROPERTY CXX_STANDARD 20)
endif()

message("common-include:${CMAKE_INSTALL_PREFIX}")
target_include_directories(orion_common PUBLIC
    "${CMAKE_INSTALL_PREFIX}/include"
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries(orion_common
    PUBLIC reflectcpp::reflectcpp
)

target_link_libraries(orion_common
    PUBLIC ${_libuv_tgt}
    PRIVATE ${_zlib_tgt}
    PRIVATE libzip::zip
#    INTERFACE libzip::zip the libzip cannot be apparently exposed outside of this lib as it complains that
#							zip_error_strerror is hidden and referenced by this so. It's the only symbol that does it
#							all zip exported symbols have visibility default and zip_error_strerror is nowhere else used
)

if(ORION_COMMON_USE_GLIB)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0>=2.60)

    target_link_libraries(orion_common
        PRIVATE PkgConfig::GLIB
    )
    target_compile_definitions(orion_common PRIVATE
        ORION_COMMON_USE_GLIB=1
    )
endif()

# This is required for flatpak build
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

target_compile_definitions(orion_common INTERFACE
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
    orion_common
    EXPORT orion_commonConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY common/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/common
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT orion_commonConfig NAMESPACE orion_common:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/orion_common)
install(TARGETS orion_common)
