cmake_minimum_required(VERSION 3.16)
project(err_ VERSION 1.0 LANGUAGES CXX)

# --- Compiler / CMake behavior ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Find Qt (Widgets + Svg for .svg/.svgz) ---
find_package(Qt6 COMPONENTS Widgets Svg REQUIRED)

# --- Sources ---
set(SRC
    main.cpp
)

# --- Resources (embed res.qrc into binary) ---
set(RESOURCE_QRC resources/res.qrc)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${RESOURCE_QRC}")
    qt_add_resources(RESOURCE_FILES "${RESOURCE_QRC}")
    list(APPEND SRC ${RESOURCE_FILES})
endif()

# --- Target ---
add_executable(err_ ${SRC}
    res.qrc
    icons/16x16/apps/err_.png icons/24x24/apps/err_.png icons/32x32/apps/err.png icons/48x48/apps/err.png icons/512x512/apps/err_.svg)
target_link_libraries(err_ PRIVATE Qt6::Widgets Qt6::Svg)

if(NOT MSVC)
    target_compile_options(err_ PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
    find_package(Threads QUIET)
    if(Threads_FOUND)
        target_link_libraries(err_ PRIVATE Threads::Threads)
    else()
        target_link_libraries(err_ PRIVATE pthread)
    endif()
endif()

# --- Install binary ---
install(TARGETS err_
        RUNTIME DESTINATION bin
        COMPONENT runtime)

# --- Desktop entry generation and install ---
set(EXECUTABLE_NAME "err_")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/err_.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/err_.desktop
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/err_.desktop
        DESTINATION share/applications
        COMPONENT desktop)

# --- Icon install (from /resources/icons) ---
foreach(SIZE IN ITEMS 16x16 24x24 32x32 48x48 512x512)
    file(GLOB ICON_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/${SIZE}/apps/*.{png,svg,svgz})
    if(ICON_FILES)
        install(FILES ${ICON_FILES}
                DESTINATION share/icons/hicolor/${SIZE}/apps
                COMPONENT icons)
    endif()
endforeach()

# --- IDE visibility (so all files show up in Qt Creator) ---
set(IDE_FILES
    ${RESOURCE_QRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/err_.desktop.in
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/16x16/apps/err_.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/24x24/apps/err_.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/32x32/apps/err.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/48x48/apps/err.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/512x512/apps/err.svg
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/512x512/apps/err.svgz
)
source_group("Resources" FILES ${IDE_FILES})

# --- CPack (DEB) configuration ---
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "err_")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "Zynomon Aelius")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Zynomon Aelius")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "error.os dashboard — system manager")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

set(CPACK_COMPONENTS_ALL runtime;desktop;icons)

include(CPack)
