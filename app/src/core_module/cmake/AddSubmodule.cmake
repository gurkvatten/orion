function(add_submodule SUBMODULE SUBMODULE_PATH)
    if(ORION_USE_SYSTEM_DEPS)
        return()
    endif()

    set(_submodule "${SUBMODULE}")
    set(_submodule_path "${SUBMODULE_PATH}")
    set(_submodule_compile_definitions ${ARGN})

    ExternalProject_Add(${_submodule}_external
        SOURCE_DIR ${PROJECT_SOURCE_DIR}/${_submodule_path}
        # These options are crucial for flatpak build (because cmake version is older in flatpak)
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
    )
    ExternalProject_Get_property(${_submodule}_external SOURCE_DIR)
    ExternalProject_Get_property(${_submodule}_external BINARY_DIR)

    file(GLOB _entries LIST_DIRECTORIES true "${SOURCE_DIR}/*")

    if(NOT _entries)
        message(STATUS "Directory is empty (${SOURCE_DIR})")
        message(FATAL_ERROR "Missing ${_submodule} submodule, to fix run:\n$ git submodule update --init ${_submodule_path}")
    endif()

    unset(CMAKE_CONFIGURE_ARGS)
    set(VCPKG_ROOT $ENV{VCPKG_ROOT})
    if(VCPKG_ROOT)
        list(APPEND CMAKE_CONFIGURE_ARGS -E env "VCPKG_ROOT=${VCPKG_ROOT}" ${CMAKE_COMMAND})
    endif()
    if(CMAKE_GENERATOR)
        list(APPEND CMAKE_CONFIGURE_ARGS -G "${CMAKE_GENERATOR}")
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND CMAKE_CONFIGURE_ARGS -T "${CMAKE_GENERATOR_TOOLSET}")
    endif()
    if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND CMAKE_CONFIGURE_ARGS -A "${CMAKE_GENERATOR_PLATFORM}")
    endif()
    list(APPEND CMAKE_CONFIGURE_ARGS -S "${SOURCE_DIR}")
    list(APPEND CMAKE_CONFIGURE_ARGS -B "${BINARY_DIR}")
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND CMAKE_CONFIGURE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
    endif()
    if(CMAKE_BUILD_TYPE)
        list(APPEND CMAKE_CONFIGURE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
    endif()
    if(CMAKE_MSVC_RUNTIME_LIBRARY)
        list(APPEND CMAKE_CONFIGURE_ARGS -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY})
    endif()
    if(CMAKE_INSTALL_PREFIX)
        list(APPEND CMAKE_CONFIGURE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX})
    endif()
    list(APPEND CMAKE_CONFIGURE_ARGS -DCMAKE_DEBUG_POSTFIX=d)
    list(APPEND CMAKE_CONFIGURE_ARGS -DCMAKE_POSITION_INDEPENDENT_CODE=1)

    message("${_submodule}: cmake")
    execute_process(
        COMMAND ${CMAKE_COMMAND} ${CMAKE_CONFIGURE_ARGS} ${_submodule_compile_definitions}
        WORKING_DIRECTORY ${BINARY_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )

    message("${_submodule}: ${CMAKE_COMMAND} --build . --config Debug")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build . --config Debug
        WORKING_DIRECTORY ${BINARY_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )

    message("${_submodule}: ${CMAKE_COMMAND} --install . --config Debug")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --install . --config Debug
        WORKING_DIRECTORY ${BINARY_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )

    message("${_submodule}: ${CMAKE_COMMAND} --build . --config Release")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build . --config Release
        WORKING_DIRECTORY ${BINARY_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )

    message("${_submodule}: ${CMAKE_COMMAND} --install . --config Release")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --install . --config Release
        WORKING_DIRECTORY ${BINARY_DIR}
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )
endfunction()
