include_guard(GLOBAL)

# Generates a C++ header containing an embedded file and adds it to TARGET.
#
# target_embed_file(<target>
#     INPUT <source-file>
#     [OUTPUT <relative-generated-header>]
#     [SYMBOL <c++-identifier-prefix>]
#     [CHUNK_SIZE <bytes>]
#     [VISIBILITY PRIVATE|PUBLIC|INTERFACE])
#
# OUTPUT defaults to "embed/<input filename>.h" and is relative to the current
# binary directory. SYMBOL defaults to the input filename with non-identifier
# characters replaced by underscores. The generated header exports:
#
#   inline constexpr const std::uint8_t* <symbol>_data;
#   inline constexpr std::size_t <symbol>_length;
function(target_embed_file TARGET)
    if(NOT TARGET "${TARGET}")
        message(FATAL_ERROR "target_embed_file: target '${TARGET}' does not exist")
    endif()

    cmake_parse_arguments(
        EMBED
        ""
        "INPUT;OUTPUT;SYMBOL;CHUNK_SIZE;VISIBILITY"
        ""
        ${ARGN}
    )

    if(NOT EMBED_INPUT)
        message(FATAL_ERROR "target_embed_file: INPUT is required")
    endif()

    if(NOT IS_ABSOLUTE "${EMBED_INPUT}")
        set(EMBED_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${EMBED_INPUT}")
    endif()
    cmake_path(NORMAL_PATH EMBED_INPUT)

    if(NOT EXISTS "${EMBED_INPUT}")
        message(FATAL_ERROR "target_embed_file: input does not exist: ${EMBED_INPUT}")
    endif()

    cmake_path(GET EMBED_INPUT FILENAME EMBED_INPUT_FILENAME)

    if(NOT EMBED_OUTPUT)
        set(EMBED_OUTPUT "embed/${EMBED_INPUT_FILENAME}.h")
    endif()
    if(IS_ABSOLUTE "${EMBED_OUTPUT}")
        message(FATAL_ERROR "target_embed_file: OUTPUT must be relative to the current binary directory")
    endif()

    if(NOT EMBED_SYMBOL)
        string(MAKE_C_IDENTIFIER "${EMBED_INPUT_FILENAME}" EMBED_SYMBOL)
    endif()
    if(NOT EMBED_SYMBOL MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR "target_embed_file: SYMBOL must be a valid C++ identifier")
    endif()

    if(NOT EMBED_CHUNK_SIZE)
        set(EMBED_CHUNK_SIZE 4096)
    endif()
    if(NOT EMBED_CHUNK_SIZE MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "target_embed_file: CHUNK_SIZE must be a positive integer")
    endif()

    get_target_property(EMBED_TARGET_TYPE "${TARGET}" TYPE)
    if(NOT EMBED_VISIBILITY)
        if(EMBED_TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
            set(EMBED_VISIBILITY INTERFACE)
        else()
            set(EMBED_VISIBILITY PRIVATE)
        endif()
    endif()
    if(NOT EMBED_VISIBILITY MATCHES "^(PRIVATE|PUBLIC|INTERFACE)$")
        message(FATAL_ERROR "target_embed_file: VISIBILITY must be PRIVATE, PUBLIC, or INTERFACE")
    endif()

    set(EMBED_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${EMBED_OUTPUT}")
    set(EMBED_GENERATOR "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GenerateEmbeddedFile.cmake")

    add_custom_command(
        OUTPUT "${EMBED_OUTPUT_FILE}"
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT_FILE=${EMBED_INPUT}"
            "-DOUTPUT_FILE=${EMBED_OUTPUT_FILE}"
            "-DSYMBOL=${EMBED_SYMBOL}"
            "-DCHUNK_SIZE=${EMBED_CHUNK_SIZE}"
            -P "${EMBED_GENERATOR}"
        MAIN_DEPENDENCY "${EMBED_INPUT}"
        DEPENDS "${EMBED_GENERATOR}"
        COMMENT "Embedding ${EMBED_INPUT_FILENAME} as ${EMBED_SYMBOL}"
        VERBATIM
    )

    set_source_files_properties(
        "${EMBED_OUTPUT_FILE}"
        PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE
    )
    target_sources("${TARGET}" ${EMBED_VISIBILITY} "${EMBED_OUTPUT_FILE}")
    target_include_directories(
        "${TARGET}"
        ${EMBED_VISIBILITY} "${CMAKE_CURRENT_BINARY_DIR}"
    )
endfunction()
