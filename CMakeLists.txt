cmake_minimum_required(VERSION 3.16)
project(err_ VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 COMPONENTS Widgets Svg REQUIRED)
add_executable(err_
    Resources/res.qrc
    err_.H err_.cxx
)
target_link_libraries(err_ PRIVATE Qt6::Widgets Qt6::Svg)

install(TARGETS err_
    RUNTIME DESTINATION bin
    COMPONENT runtime
)

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

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Resources/hicolor/scalable/err_.svg
    DESTINATION share/icons/hicolor/scalable/apps
    COMPONENT icons
)

set(CPACK_PACKAGE_NAME "err_")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "Zynomon Aelius")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Zynomon Aelius")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "error.os dashboard a system manager")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

set(CPACK_COMPONENTS_ALL runtime;desktop;icons)

include(CPack)
