function(glob_cpp OUT_VAR SUB_PATH)
    file(GLOB_RECURSE FOUND_FILES
        CONFIGURE_DEPENDS
        "${SUB_PATH}/*.cpp"
        "${SUB_PATH}/*.hpp"
        "${SUB_PATH}/*.inl"
    )

    set(${OUT_VAR} ${${OUT_VAR}} ${FOUND_FILES} PARENT_SCOPE)
endfunction()