# Configure Go toolchain for code generation

set(GO_TOOLCHAIN_VERSION "1.25.3")

set(_go_user_provided FALSE)
if(DEFINED GO_EXECUTABLE AND GO_EXECUTABLE)
    if(EXISTS ${GO_EXECUTABLE})
        set(_go_user_provided TRUE)
    else()
        message(FATAL_ERROR "GO_EXECUTABLE was provided but '${GO_EXECUTABLE}' does not exist.")
    endif()
endif()

if(NOT _go_user_provided)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        set(GO_PLATFORM "linux")
        set(GO_ARCHIVE_EXTENSION "tar.gz")
    endif()
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(GO_EXECUTABLE_SUFFIX ".exe")
        set(GO_PLATFORM "windows")
        set(GO_ARCHIVE_EXTENSION "zip")
    endif()
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(GO_PLATFORM "darwin")
        set(GO_ARCHIVE_EXTENSION "tar.gz")
    endif()

    if(NOT GO_PLATFORM) 
        message(FATAL_ERROR "Automatic Go download only implemented for Linux/Windows/Darwin hosts. Set GO_EXECUTABLE manually.")
    endif()

    if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        set(GO_PLATFORM_ARCH "amd64")
    elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(GO_PLATFORM_ARCH "arm64")
    else()
        message(FATAL_ERROR
            "Unsupported host architecture '${CMAKE_HOST_SYSTEM_PROCESSOR}' for automatic Go download. "
            "Please provide GO_EXECUTABLE manually.")
    endif()

    set(GO_TOOLCHAIN_DIRECTORY "${CMAKE_BINARY_DIR}/toolchains/go/${GO_TOOLCHAIN_VERSION}")
    set(GO_TOOLCHAIN_PREFIX "${GO_TOOLCHAIN_DIRECTORY}/go")
    set(GO_ARCHIVE_NAME "go${GO_TOOLCHAIN_VERSION}.${GO_PLATFORM}-${GO_PLATFORM_ARCH}.${GO_ARCHIVE_EXTENSION}")
    set(GO_ARCHIVE_PATH "${GO_TOOLCHAIN_DIRECTORY}/${GO_ARCHIVE_NAME}")
    set(GO_DOWNLOAD_URL "https://go.dev/dl/${GO_ARCHIVE_NAME}")

    if(NOT EXISTS "${GO_TOOLCHAIN_PREFIX}/bin/go${GO_EXECUTABLE_SUFFIX}")
        file(MAKE_DIRECTORY "${GO_TOOLCHAIN_DIRECTORY}")

        if(NOT EXISTS "${GO_ARCHIVE_PATH}")
            message(STATUS "Downloading Go ${GO_TOOLCHAIN_VERSION} (${GO_PLATFORM}-${GO_PLATFORM_ARCH})")
            file(DOWNLOAD
                "${GO_DOWNLOAD_URL}"
                "${GO_ARCHIVE_PATH}"
                SHOW_PROGRESS
            )
        endif()

        if(EXISTS "${GO_TOOLCHAIN_PREFIX}")
            file(REMOVE_RECURSE "${GO_TOOLCHAIN_PREFIX}")
        endif()

        message(STATUS "Extracting Go toolchain to ${GO_TOOLCHAIN_PREFIX}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${GO_ARCHIVE_PATH}"
            WORKING_DIRECTORY "${GO_TOOLCHAIN_DIRECTORY}"
            COMMAND_ERROR_IS_FATAL ANY
        )
    endif()

    set(GO_EXECUTABLE "${GO_TOOLCHAIN_PREFIX}/bin/go${GO_EXECUTABLE_SUFFIX}" CACHE FILEPATH "Go toolchain downloaded during configure" FORCE)
    set(GO_ROOT "${GO_TOOLCHAIN_PREFIX}" CACHE PATH "Go toolchain root extracted during configure" FORCE)
endif()

message(STATUS "Using Go executable: ${GO_EXECUTABLE}")