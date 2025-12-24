cmake_minimum_required(VERSION 3.16)
project(err_ VERSION 2.1.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 COMPONENTS Widgets Svg REQUIRED)

# ----------------- Sources ---------------------
add_executable(err_
    main.cpp
    Resources/res.qrc
)

target_link_libraries(err_ PRIVATE Qt6::Widgets Qt6::Svg)

# ----------------- Install binary ---------------------
install(TARGETS err_
    RUNTIME DESTINATION bin
    COMPONENT runtime
)

# ----------------- Desktop entry ---------------------
set(EXECUTABLE_NAME "err_")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/err_.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/err_.desktop
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/err_.desktop
    DESTINATION share/applications
    COMPONENT desktop
)

# ----------------- Icons ---------------------
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Resources/hicolor/scalable/err_.svg
    DESTINATION share/icons/hicolor/scalable/apps
    COMPONENT icons
)

# ----------------- Build type ---------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type" FORCE)
endif()

# ----------------- CPack (DEB) ---------------------
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "err_")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "Zynomon Aelius")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Zynomon Aelius")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "error.os dashboard — a system manager")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

set(CPACK_COMPONENTS_ALL runtime;desktop;icons)

include(CPack)
