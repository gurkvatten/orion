function(orion_package_webextensions)
    cmake_parse_arguments(ARG "" "SOURCE_DIR;OUTPUT_DIR" "" ${ARGN})

    if(NOT ARG_SOURCE_DIR)
        message(FATAL_ERROR "orion_package_webextensions requires SOURCE_DIR")
    endif()
    if(NOT ARG_OUTPUT_DIR)
        message(FATAL_ERROR "orion_package_webextensions requires OUTPUT_DIR")
    endif()

    if(NOT GO_EXECUTABLE)
        message(FATAL_ERROR "orion_package_webextensions requires GO_EXECUTABLE")
    endif()

    set(webextension_packager_source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/package_webextension.go"
    )
    if(CMAKE_HOST_WIN32)
        set(webextension_packager_suffix ".exe")
    else()
        set(webextension_packager_suffix "")
    endif()
    set(webextension_packager
        "${CMAKE_CURRENT_BINARY_DIR}/tools/package_webextension${webextension_packager_suffix}"
    )
    set(webextension_private_key_dir
        "${CMAKE_BINARY_DIR}/CMakeFiles/webextension-keys"
    )
    set(webextension_archives)

    add_custom_command(
        OUTPUT "${webextension_packager}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/tools"
        COMMAND "${GO_EXECUTABLE}" build -trimpath
            -o "${webextension_packager}"
            "${webextension_packager_source}"
        DEPENDS "${webextension_packager_source}"
        COMMENT "Building WebExtension packager"
        VERBATIM
    )

    foreach(webextension_platform chrome firefox)
        set(webextension_platform_source_dir
            "${ARG_SOURCE_DIR}/${webextension_platform}"
        )
        file(GLOB webextension_entries
            CONFIGURE_DEPENDS
            "${webextension_platform_source_dir}/*"
        )

        if(webextension_platform STREQUAL "chrome")
            set(webextension_archive_extension crx)
        else()
            set(webextension_archive_extension xpi)
        endif()

        foreach(webextension_source_dir IN LISTS webextension_entries)
            if(NOT IS_DIRECTORY "${webextension_source_dir}")
                continue()
            endif()

            cmake_path(GET webextension_source_dir FILENAME webextension_name)

            # A flavor directory may hold only its manifest plus a `.orion-overlay`
            # file naming (relative to it) a shared source directory that is
            # packaged underneath it; the flavor's own files win on conflicts.
            set(webextension_sources "${webextension_source_dir}")
            set(webextension_overlay_file "${webextension_source_dir}/.orion-overlay")
            if(EXISTS "${webextension_overlay_file}")
                file(STRINGS "${webextension_overlay_file}" webextension_overlay_relative LIMIT_COUNT 1)
                cmake_path(ABSOLUTE_PATH webextension_overlay_relative
                    BASE_DIRECTORY "${webextension_source_dir}" NORMALIZE
                    OUTPUT_VARIABLE webextension_overlay_dir)
                if(NOT IS_DIRECTORY "${webextension_overlay_dir}")
                    message(FATAL_ERROR
                        "${webextension_overlay_file} points at missing directory ${webextension_overlay_dir}")
                endif()
                set(webextension_sources "${webextension_overlay_dir}" "${webextension_source_dir}")
            endif()
            set(webextension_source_args)
            set(webextension_source_files)
            foreach(webextension_source IN LISTS webextension_sources)
                list(APPEND webextension_source_args --source "${webextension_source}")
                file(GLOB_RECURSE webextension_layer_files
                    CONFIGURE_DEPENDS
                    "${webextension_source}/*"
                )
                list(APPEND webextension_source_files ${webextension_layer_files})
            endforeach()

            set(webextension_archive
                "${ARG_OUTPUT_DIR}/${webextension_name}.${webextension_archive_extension}"
            )

            if(webextension_platform STREQUAL "chrome")
                set(webextension_private_key
                    "${webextension_private_key_dir}/${webextension_name}.pem"
                )
                add_custom_command(
                    OUTPUT "${webextension_archive}"
                    BYPRODUCTS "${webextension_private_key}"
                    COMMAND "${CMAKE_COMMAND}" -E make_directory
                        "${ARG_OUTPUT_DIR}"
                        "${webextension_private_key_dir}"
                    COMMAND "${webextension_packager}"
                        --format crx2
                        ${webextension_source_args}
                        --output "${webextension_archive}"
                        --key "${webextension_private_key}"
                    WORKING_DIRECTORY "${webextension_source_dir}"
                    DEPENDS
                        ${webextension_source_files}
                        "${webextension_packager}"
                    COMMENT "Packaging Chrome extension ${webextension_name}"
                    COMMAND_EXPAND_LISTS
                    VERBATIM
                )
            else()
                add_custom_command(
                    OUTPUT "${webextension_archive}"
                    COMMAND "${CMAKE_COMMAND}" -E make_directory
                        "${ARG_OUTPUT_DIR}"
                    COMMAND "${webextension_packager}"
                        --format zip
                        ${webextension_source_args}
                        --output "${webextension_archive}"
                    WORKING_DIRECTORY "${webextension_source_dir}"
                    DEPENDS
                        ${webextension_source_files}
                        "${webextension_packager}"
                    COMMENT "Packaging Firefox extension ${webextension_name}"
                    COMMAND_EXPAND_LISTS
                    VERBATIM
                )
            endif()

            list(APPEND webextension_archives "${webextension_archive}")
        endforeach()
    endforeach()

    if(NOT TARGET webextensions)
        add_custom_target(webextensions ALL DEPENDS ${webextension_archives})
    endif()
endfunction()
