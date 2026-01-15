function(glob_cpp OUT_VAR SUB_PATH)
    file(GLOB_RECURSE FOUND_FILES
        CONFIGURE_DEPENDS
        "${SUB_PATH}/*.cpp"
        "${SUB_PATH}/*.hpp"
        "${SUB_PATH}/*.inl"
    )

    set(${OUT_VAR} ${${OUT_VAR}} ${FOUND_FILES} PARENT_SCOPE)
endfunction()


function(glob_cxx OUT_VAR SUB_PATH)
    file(GLOB_RECURSE FOUND_FILES
        CONFIGURE_DEPENDS
        "${SUB_PATH}/*.cxx"
    )

    set(${OUT_VAR} ${${OUT_VAR}} ${FOUND_FILES} PARENT_SCOPE)
endfunction()

function(add_resources TARGET_NAME RESOURCE_PATH)
    # Get the absolute path to the source resources
    get_filename_component(ABS_RESOURCE_PATH ${RESOURCE_PATH} ABSOLUTE)

    add_custom_command(
        TARGET ${TARGET_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ABS_RESOURCE_PATH}"
                # Use the original RESOURCE_PATH to preserve the structure
                "$<TARGET_FILE_DIR:${TARGET_NAME}>/${RESOURCE_PATH}"
        COMMENT "Copying resource directory: ${RESOURCE_PATH}"
        VERBATIM
    )
endfunction()