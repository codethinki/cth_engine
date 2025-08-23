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