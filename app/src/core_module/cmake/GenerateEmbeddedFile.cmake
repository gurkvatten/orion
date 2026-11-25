foreach(REQUIRED_ARGUMENT INPUT_FILE OUTPUT_FILE SYMBOL CHUNK_SIZE)
    if(NOT DEFINED ${REQUIRED_ARGUMENT})
        message(FATAL_ERROR "GenerateEmbeddedFile.cmake: ${REQUIRED_ARGUMENT} is required")
    endif()
endforeach()

if(NOT SYMBOL MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
    message(FATAL_ERROR "GenerateEmbeddedFile.cmake: invalid symbol '${SYMBOL}'")
endif()
if(NOT CHUNK_SIZE MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "GenerateEmbeddedFile.cmake: CHUNK_SIZE must be a positive integer")
endif()

file(READ "${INPUT_FILE}" FILE_HEX HEX)
string(LENGTH "${FILE_HEX}" FILE_HEX_LENGTH)
math(EXPR FILE_LENGTH "${FILE_HEX_LENGTH} / 2")
math(EXPR CHUNK_HEX_SIZE "${CHUNK_SIZE} * 2")

get_filename_component(OUTPUT_DIRECTORY "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")
set(TEMPORARY_OUTPUT "${OUTPUT_FILE}.tmp")

file(WRITE "${TEMPORARY_OUTPUT}"
    "// Generated from ${INPUT_FILE}; do not edit.\n"
    "#pragma once\n\n"
    "#include <array>\n"
    "#include <cstddef>\n"
    "#include <cstdint>\n\n"
    "namespace ${SYMBOL}_embed_detail {\n"
)

set(CHUNK_INDEX 0)
set(HEX_OFFSET 0)
while(HEX_OFFSET LESS FILE_HEX_LENGTH)
    math(EXPR HEX_REMAINING "${FILE_HEX_LENGTH} - ${HEX_OFFSET}")
    if(HEX_REMAINING GREATER CHUNK_HEX_SIZE)
        set(CURRENT_CHUNK_HEX_SIZE "${CHUNK_HEX_SIZE}")
    else()
        set(CURRENT_CHUNK_HEX_SIZE "${HEX_REMAINING}")
    endif()

    string(SUBSTRING "${FILE_HEX}" ${HEX_OFFSET} ${CURRENT_CHUNK_HEX_SIZE} CHUNK_HEX)
    set(CHUNK_BYTES "")
    set(CHUNK_HEX_OFFSET 0)
    while(CHUNK_HEX_OFFSET LESS CURRENT_CHUNK_HEX_SIZE)
        math(EXPR CHUNK_HEX_REMAINING "${CURRENT_CHUNK_HEX_SIZE} - ${CHUNK_HEX_OFFSET}")
        if(CHUNK_HEX_REMAINING GREATER 32)
            set(ROW_HEX_SIZE 32)
        else()
            set(ROW_HEX_SIZE "${CHUNK_HEX_REMAINING}")
        endif()
        string(SUBSTRING "${CHUNK_HEX}" ${CHUNK_HEX_OFFSET} ${ROW_HEX_SIZE} ROW_HEX)
        string(REGEX REPLACE "([0-9A-Fa-f][0-9A-Fa-f])" "0x\\1, " ROW_BYTES "${ROW_HEX}")
        string(APPEND CHUNK_BYTES "    ${ROW_BYTES}\n")
        math(EXPR CHUNK_HEX_OFFSET "${CHUNK_HEX_OFFSET} + ${ROW_HEX_SIZE}")
    endwhile()

    file(APPEND "${TEMPORARY_OUTPUT}"
        "inline constexpr std::uint8_t chunk_${CHUNK_INDEX}[] = {\n"
        "${CHUNK_BYTES}"
        "};\n\n"
    )

    math(EXPR HEX_OFFSET "${HEX_OFFSET} + ${CURRENT_CHUNK_HEX_SIZE}")
    math(EXPR CHUNK_INDEX "${CHUNK_INDEX} + 1")
endwhile()

file(APPEND "${TEMPORARY_OUTPUT}"
    "constexpr std::array<std::uint8_t, ${FILE_LENGTH}> make_data() noexcept\n"
    "{\n"
    "    std::array<std::uint8_t, ${FILE_LENGTH}> result{};\n"
    "    std::size_t offset = 0;\n"
)

set(COPY_CHUNK_INDEX 0)
while(COPY_CHUNK_INDEX LESS CHUNK_INDEX)
    file(APPEND "${TEMPORARY_OUTPUT}"
        "    for (std::uint8_t byte : chunk_${COPY_CHUNK_INDEX}) {\n"
        "        result[offset++] = byte;\n"
        "    }\n"
    )
    math(EXPR COPY_CHUNK_INDEX "${COPY_CHUNK_INDEX} + 1")
endwhile()

file(APPEND "${TEMPORARY_OUTPUT}"
    "    return result;\n"
    "}\n\n"
    "inline constexpr auto storage = make_data();\n"
    "} // namespace ${SYMBOL}_embed_detail\n\n"
    "inline constexpr const std::uint8_t* ${SYMBOL}_data = ${SYMBOL}_embed_detail::storage.data();\n"
    "inline constexpr std::size_t ${SYMBOL}_length = ${SYMBOL}_embed_detail::storage.size();\n"
)

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${TEMPORARY_OUTPUT}" "${OUTPUT_FILE}"
    COMMAND_ERROR_IS_FATAL ANY
)
file(REMOVE "${TEMPORARY_OUTPUT}")
