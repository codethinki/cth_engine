# RGFW from the lib/rgfw submodule: the header with its platform libraries
# cgfw compiles the implementation (RGFW_IMPLEMENTATION) in one C source file
# platforms: Win32 and Wayland, no X11, no OpenGL and no EGL
if(NOT TARGET rgfw::rgfw)
    set(RGFW_DIR "${CMAKE_CURRENT_LIST_DIR}/../rgfw")
    if(NOT EXISTS "${RGFW_DIR}/RGFW.h")
        message(FATAL_ERROR "lib/rgfw is empty, run: git submodule update --init lib/rgfw")
    endif()

    add_library(rgfw INTERFACE)
    add_library(rgfw::rgfw ALIAS rgfw)

    # the implementation and the generated protocol code are C
    enable_language(C)

    target_include_directories(rgfw INTERFACE "${RGFW_DIR}")
    # RGFWDEF empty: the functions have external linkage, the default inline needs the definitions in every user
    target_compile_definitions(rgfw INTERFACE RGFW_VULKAN "RGFWDEF=")

    if(WIN32)
        # RGFW includes windows.h: no min and max macros
        target_compile_definitions(rgfw INTERFACE NOMINMAX)
        # RGFW loads winmm, dwmapi, shcore and opengl32 at run time
        target_link_libraries(rgfw INTERFACE gdi32 user32 shell32)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(rgfw INTERFACE RGFW_WAYLAND RGFW_NO_X11)

        # wayland and xkbcommon come from the system, the vcpkg ports of them are empty on linux
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(RGFW_WAYLAND REQUIRED IMPORTED_TARGET wayland-client wayland-cursor xkbcommon)
        find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

        # the protocol XML files of the vcpkg port wayland-protocols
        pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
        if(NOT WAYLAND_PROTOCOLS_DIR)
            message(FATAL_ERROR "wayland-protocols not found, vcpkg installs it on linux")
        endif()

        # the protocols that RGFW includes, see the xdg-shell.c rule in lib/rgfw/Makefile
        set(RGFW_WAYLAND_PROTOCOLS
            "stable/xdg-shell/xdg-shell.xml"
            "unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
            "staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml"
            "unstable/xdg-output/xdg-output-unstable-v1.xml"
            "unstable/relative-pointer/relative-pointer-unstable-v1.xml"
            "unstable/pointer-constraints/pointer-constraints-unstable-v1.xml"
        )

        set(RGFW_PROTOCOL_DIR "${CMAKE_BINARY_DIR}/lib/rgfw/wayland")
        file(MAKE_DIRECTORY "${RGFW_PROTOCOL_DIR}")

        set(RGFW_PROTOCOL_FILES "")
        foreach(protocol IN LISTS RGFW_WAYLAND_PROTOCOLS)
            get_filename_component(name "${protocol}" NAME_WE)
            set(xml "${WAYLAND_PROTOCOLS_DIR}/${protocol}")
            set(header "${RGFW_PROTOCOL_DIR}/${name}.h")
            set(code "${RGFW_PROTOCOL_DIR}/${name}.c")

            add_custom_command(
                    OUTPUT "${header}" "${code}"
                    COMMAND "${WAYLAND_SCANNER}" client-header "${xml}" "${header}"
                    COMMAND "${WAYLAND_SCANNER}" public-code "${xml}" "${code}"
                    DEPENDS "${xml}"
                    COMMENT "wayland-scanner ${name}"
                    VERBATIM
            )
            list(APPEND RGFW_PROTOCOL_FILES "${header}" "${code}")
        endforeach()

        add_library(rgfw_wayland_protocols STATIC ${RGFW_PROTOCOL_FILES})
        target_include_directories(rgfw_wayland_protocols PUBLIC "${RGFW_PROTOCOL_DIR}")
        target_link_libraries(rgfw_wayland_protocols PUBLIC PkgConfig::RGFW_WAYLAND)

        target_link_libraries(rgfw INTERFACE rgfw_wayland_protocols)

        #TODO libdecor for frames on GNOME, GNOME has no xdg-decoration (KDE and sway have it), vcpkg has no port
    else()
        message(FATAL_ERROR "RGFW: only Windows and Linux (Wayland) are supported, got ${CMAKE_SYSTEM_NAME}")
    endif()
endif()
