function(glob_glsl OUT_VAR SUB_PATH)
    file(GLOB_RECURSE FOUND_FILES
        CONFIGURE_DEPENDS
        "${SUB_PATH}/*.vert"
        "${SUB_PATH}/*.frag"
    )

    set(${OUT_VAR} ${${OUT_VAR}} ${FOUND_FILES} PARENT_SCOPE)
endfunction()