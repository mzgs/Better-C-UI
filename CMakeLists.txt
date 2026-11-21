cmake_minimum_required(VERSION 3.16)

project(cui
    VERSION 1.3.1
    DESCRIPTION "Modern native UI components for C"
    LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CUI_IS_TOP_LEVEL ON)
else()
    set(CUI_IS_TOP_LEVEL OFF)
endif()

option(CUI_BUILD_EXAMPLES "Build the examples" ${CUI_IS_TOP_LEVEL})
option(CUI_BUILD_TESTS "Build the tests" ${CUI_IS_TOP_LEVEL})
option(CUI_TREE_SITTER "Build the optional Tree-sitter highlighting add-on" OFF)
option(CUI_TREE_SITTER_DEMO "Download the pinned GPUI Kit grammars and build the Tree-sitter demo" OFF)
option(CUI_TEXT_SHAPING "Shape static Arabic text with OpenType" OFF)
option(CUI_BUILD_SHARED "Build a shared library" OFF)
option(CUI_WITH_APP "Include the app layer (windows, events)" ON)
option(CUI_WEBVIEW "Build the optional native WebView add-on" OFF)
option(CUI_VIDEO "Build the optional libmpv video playback add-on" OFF)
option(CUI_DEFAULT_FONTS "Embed Inter, Geist and Lucide" ON)
option(CUI_STB_IMAGE "Image loading with stb_image" ON)
option(CUI_IMAGE_ALL_FORMATS "Also decode BMP, GIF and TGA (PNG and JPEG are always on)" OFF)
if(CUI_TREE_SITTER_DEMO AND (NOT CUI_TREE_SITTER OR NOT CUI_BUILD_EXAMPLES OR NOT CUI_WITH_APP))
    message(FATAL_ERROR "CUI_TREE_SITTER_DEMO requires CUI_TREE_SITTER, CUI_BUILD_EXAMPLES and CUI_WITH_APP")
endif()
set(CUI_PLATFORM "auto" CACHE STRING "Window backend: auto, win32, cocoa, x11, sdl2, glfw")
set_property(CACHE CUI_PLATFORM PROPERTY STRINGS auto win32 cocoa x11 sdl2 glfw)

if(NOT CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

set(CUI_CORE_SOURCES
    src/cui_core.c
    src/cui_theme.c
    src/cui_draw.c
    src/cui_text.c
    src/cui_layout.c
    src/cui_widgets.c
    src/cui_controls.c
    src/cui_textedit.c
    src/cui_overlay.c
    src/cui_data.c
    src/cui_charts.c
    src/cui_extras.c
    src/cui_actions.c
    src/cui_pickers.c
    src/cui_disclosure.c
    src/cui_grid.c
    src/cui_treetable.c
    src/cui_code.c
    src/cui_markdown.c
    src/cui_editor.c
    src/cui_diff.c
    src/cui_tools.c
    src/cui_hex.c
    src/cui_social.c
    src/cui_media.c
    src/cui_planning.c
    src/cui_chrome.c
    src/cui_dock.c
    src/cui_viz.c
    src/cui_motion.c
    src/cui_ai.c
    src/cui_image.c
    src/cui_nodes.c
    src/cui_flame.c
    src/cui_curves.c
    src/cui_timeline.c
    src/cui_dials.c
    src/cui_month.c
    src/cui_strings.c
    src/cui_devtools.c
    src/cui_graph.c
    src/cui_devpanels.c
    src/cui_regex.c
    src/cui_devviews.c
    src/cui_instrument.c
    src/cui_tour.c
    src/cui_photo.c
    src/cui_files.c
    src/cui_terminal.c
    src/cui_emoji.c
    src/render/cui_render_sw.c
    src/render/cui_render_gl.c)

if(CUI_DEFAULT_FONTS)
    list(APPEND CUI_CORE_SOURCES src/cui_font_data.c src/cui_inflate.c)
endif()

# Pick the window backend
set(CUI_BACKEND "")
if(CUI_WITH_APP)
    if(CUI_PLATFORM STREQUAL "auto")
        if(WIN32)
            set(CUI_BACKEND win32)
        elseif(APPLE)
            set(CUI_BACKEND cocoa)
        else()
            set(CUI_BACKEND x11)
        endif()
    else()
        set(CUI_BACKEND ${CUI_PLATFORM})
    endif()
    if(CUI_BACKEND STREQUAL "cocoa")
        enable_language(OBJC)
    endif()
    list(APPEND CUI_CORE_SOURCES src/platform/cui_app.c src/platform/cui_dialogs.c)
    if(CUI_BACKEND STREQUAL "win32")
        list(APPEND CUI_CORE_SOURCES src/platform/cui_app_win32.c)
    elseif(CUI_BACKEND STREQUAL "cocoa")
        list(APPEND CUI_CORE_SOURCES src/platform/cui_app_cocoa.m)
    elseif(CUI_BACKEND STREQUAL "x11")
        list(APPEND CUI_CORE_SOURCES src/platform/cui_app_x11.c)
    elseif(CUI_BACKEND STREQUAL "sdl2")
        list(APPEND CUI_CORE_SOURCES src/platform/cui_app_sdl2.c)
    elseif(CUI_BACKEND STREQUAL "glfw")
        list(APPEND CUI_CORE_SOURCES src/platform/cui_app_glfw.c)
    else()
        message(FATAL_ERROR "Unknown CUI_PLATFORM '${CUI_BACKEND}'")
    endif()
endif()

if(CUI_BUILD_SHARED)
    add_library(cui SHARED ${CUI_CORE_SOURCES})
    target_compile_definitions(cui PUBLIC CUI_SHARED PRIVATE CUI_BUILDING)
    set_target_properties(cui PROPERTIES C_VISIBILITY_PRESET hidden OBJC_VISIBILITY_PRESET hidden)
else()
    add_library(cui STATIC ${CUI_CORE_SOURCES})
endif()
add_library(cui::cui ALIAS cui)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/CuiApp.cmake)   # cui_app_target(), for apps that link CUI

target_include_directories(cui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

include(GNUInstallDirs)
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(CUI_PC_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(CUI_PC_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
    file(RELATIVE_PATH CUI_PC_PREFIX_REL "/${CMAKE_INSTALL_LIBDIR}/pkgconfig" "/")
    string(REGEX REPLACE "/$" "" CUI_PC_PREFIX_REL "${CUI_PC_PREFIX_REL}")
    set(CUI_PC_PREFIX "\${pcfiledir}/${CUI_PC_PREFIX_REL}")
    set(CUI_PC_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(CUI_PC_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(CUI_PC_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

if(CUI_WEBVIEW)
    if(NOT CUI_WITH_APP)
        message(FATAL_ERROR "CUI_WEBVIEW requires CUI_WITH_APP")
    endif()
    add_subdirectory(addons/webview)
endif()

if(CUI_TREE_SITTER)
    add_subdirectory(addons/tree_sitter)
endif()
if(CUI_VIDEO)
    if(NOT CUI_WITH_APP)
        message(FATAL_ERROR "CUI_VIDEO requires CUI_WITH_APP")
    endif()
    add_subdirectory(addons/video)
endif()

if(CUI_TEXT_SHAPING)
    target_sources(cui PRIVATE src/cui_bidi.c src/cui_shape.c)
    target_compile_definitions(cui PRIVATE CUI_TEXT_SHAPING)
endif()

if(NOT CUI_DEFAULT_FONTS)
    target_compile_definitions(cui PRIVATE CUI_NO_DEFAULT_FONTS)
endif()
if(NOT CUI_STB_IMAGE)
    target_compile_definitions(cui PRIVATE CUI_NO_STB_IMAGE)
elseif(CUI_IMAGE_ALL_FORMATS)
    target_compile_definitions(cui PRIVATE CUI_IMAGE_ALL_FORMATS)
endif()

# One section per function and object, so linkers can drop the parts an app
# never calls (-Wl,--gc-sections, -dead_strip, /OPT:REF).
if(MSVC)
    target_compile_options(cui PRIVATE /Gy /Gw)
else()
    target_compile_options(cui PRIVATE -ffunction-sections -fdata-sections)
endif()

# No unwind tables in release builds on ELF systems: CUI is C and throws nothing, and they
# were 6 to 7% of an app. Debug builds keep them, and Windows and macOS need theirs. Keep
# them for backtraces from release builds, or C++ exceptions thrown through CUI callbacks.
option(CUI_KEEP_UNWIND_TABLES "Keep unwind tables in release builds on Linux and BSD" OFF)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32 AND NOT APPLE AND NOT EMSCRIPTEN AND NOT CUI_KEEP_UNWIND_TABLES)
    target_compile_options(cui PRIVATE $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:-fno-asynchronous-unwind-tables -fno-unwind-tables>)
endif()

# MinGW's linker cannot drop unused functions (each has an unwind record it keeps), so a
# whole file came along for any function used. Release builds also carry link-time
# optimization data ("fat" objects: apps that do not use LTO link as before), and an app
# linked with -flto keeps only what it calls: assistant.exe 740 KB to 417 KB.
if(MINGW AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(cui PRIVATE $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:-flto -ffat-lto-objects>)
endif()

if(MSVC)
    target_compile_options(cui PRIVATE /W4 /wd4100 /wd4201 /wd4204 /wd4221 /wd4244 /wd4267 /wd4305 /wd4996 /utf-8)
    target_compile_definitions(cui PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(cui PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# System libraries
find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(cui PRIVATE Threads::Threads)
endif()
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(cui PUBLIC m)
endif()

if(CUI_BACKEND STREQUAL "win32")
    target_link_libraries(cui PUBLIC user32 gdi32 opengl32 shell32 imm32 dwmapi comdlg32 ole32)
elseif(CUI_BACKEND STREQUAL "cocoa")
    target_link_libraries(cui PUBLIC "-framework Cocoa" "-framework QuartzCore" "-framework OpenGL")
    target_compile_definitions(cui PRIVATE GL_SILENCE_DEPRECATION)
    set_source_files_properties(src/platform/cui_app_cocoa.m PROPERTIES COMPILE_FLAGS "-fobjc-arc")
elseif(CUI_BACKEND STREQUAL "x11")
    find_package(X11 REQUIRED)
    target_include_directories(cui PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(cui PUBLIC ${X11_LIBRARIES} ${CMAKE_DL_LIBS})
elseif(CUI_BACKEND STREQUAL "sdl2")
    find_package(SDL2 REQUIRED)
    target_link_libraries(cui PUBLIC SDL2::SDL2)
    if(WIN32)
        target_link_libraries(cui PUBLIC user32 shell32 comdlg32 ole32)
    endif()
elseif(CUI_BACKEND STREQUAL "glfw")
    find_package(glfw3 REQUIRED)
    target_link_libraries(cui PUBLIC glfw)
    if(WIN32)
        target_link_libraries(cui PUBLIC user32 shell32 comdlg32 ole32)
    endif()
endif()

set_target_properties(cui PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

if(CUI_BUILD_EXAMPLES AND CUI_WITH_APP)
    add_subdirectory(examples)
endif()

if(CUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install
install(TARGETS cui EXPORT cuiTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/cui DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT cuiTargets NAMESPACE cui:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cui)
include(CMakePackageConfigHelpers)
# the packages cui::cui's link line names, found again where the installed package is used
set(CUI_CONFIG_DEPS "")
if(Threads_FOUND)
    string(APPEND CUI_CONFIG_DEPS "find_dependency(Threads)\n")
endif()
if(CUI_BACKEND STREQUAL "sdl2")
    string(APPEND CUI_CONFIG_DEPS "find_dependency(SDL2)\n")
elseif(CUI_BACKEND STREQUAL "glfw")
    string(APPEND CUI_CONFIG_DEPS "find_dependency(glfw3)\n")
endif()
configure_file(cmake/cuiConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/cuiConfig.cmake @ONLY)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/cuiConfigVersion.cmake COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cuiConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/cuiConfigVersion.cmake cmake/CuiApp.cmake cmake/FindTreeSitter.cmake cmake/FindMPV.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cui)

# pkg-config
set(CUI_PC_LIBS "-lcui")
set(CUI_PC_REQUIRES "")
set(CUI_PC_CFLAGS "-I\${includedir}")
if(CUI_BUILD_SHARED)
    set(CUI_PC_CFLAGS "${CUI_PC_CFLAGS} -DCUI_SHARED")
endif()
if(CUI_BACKEND STREQUAL "x11")
    set(CUI_PC_LIBS "${CUI_PC_LIBS} -lX11")
    if(CMAKE_DL_LIBS)
        set(CUI_PC_LIBS "${CUI_PC_LIBS} -l${CMAKE_DL_LIBS}")
    endif()
elseif(CUI_BACKEND STREQUAL "cocoa")
    set(CUI_PC_LIBS "${CUI_PC_LIBS} -framework Cocoa -framework QuartzCore -framework OpenGL")
elseif(CUI_BACKEND STREQUAL "win32")
    set(CUI_PC_LIBS "${CUI_PC_LIBS} -luser32 -lgdi32 -lopengl32 -lshell32 -limm32 -ldwmapi -lcomdlg32 -lole32")
elseif(CUI_BACKEND STREQUAL "sdl2")
    set(CUI_PC_REQUIRES "sdl2")
    if(WIN32)
        set(CUI_PC_LIBS "${CUI_PC_LIBS} -luser32 -lshell32 -lcomdlg32 -lole32")
    endif()
elseif(CUI_BACKEND STREQUAL "glfw")
    set(CUI_PC_REQUIRES "glfw3")
    if(WIN32)
        set(CUI_PC_LIBS "${CUI_PC_LIBS} -luser32 -lshell32 -lcomdlg32 -lole32")
    endif()
endif()
if(Threads_FOUND AND NOT WIN32 AND NOT APPLE)
    set(CUI_PC_LIBS "${CUI_PC_LIBS} -lpthread")
endif()
if(NOT WIN32 AND NOT MSVC AND NOT APPLE)
    set(CUI_PC_LIBS "${CUI_PC_LIBS} -lm")
endif()
set(CUI_PC_REQUIRES_LINE "")
if(CUI_PC_REQUIRES)
    set(CUI_PC_REQUIRES_LINE "Requires: ${CUI_PC_REQUIRES}\n")
endif()
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cui.pc
"prefix=${CUI_PC_PREFIX}
libdir=${CUI_PC_LIBDIR}
includedir=${CUI_PC_INCLUDEDIR}

Name: cui
Description: ${PROJECT_DESCRIPTION}
Version: ${PROJECT_VERSION}
${CUI_PC_REQUIRES_LINE}Cflags: ${CUI_PC_CFLAGS}
Libs: -L\${libdir} ${CUI_PC_LIBS}
")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cui.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
