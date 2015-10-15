cmake_minimum_required(VERSION 2.8)
project(Athol)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(EGL REQUIRED)
find_package(GLIB REQUIRED)
find_package(Libinput REQUIRED)
find_package(Libudev REQUIRED)
find_package(Wayland 1.5.0 REQUIRED)

add_custom_command(
    OUTPUT wayland-bcmrpi-dispmanx-protocol.c
    DEPENDS wayland-bcmrpi-dispmanx.xml
    COMMAND ${WAYLAND_SCANNER} server-header < wayland-bcmrpi-dispmanx.xml > wayland-bcmrpi-dispmanx-server-protocol.h
    COMMAND ${WAYLAND_SCANNER} client-header < wayland-bcmrpi-dispmanx.xml > wayland-bcmrpi-dispmanx-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} code < wayland-bcmrpi-dispmanx.xml > wayland-bcmrpi-dispmanx-protocol.c
)

configure_file(athol.pc.in ${CMAKE_BINARY_DIR}/athol.pc @ONLY)

add_executable(athol
    Athol.cpp
    Input.cpp
    Main.cpp
    ShellLoader.cpp
    Surface.cpp
    Pointer.cpp
    PointerData.cpp
    wayland-bcmrpi-dispmanx-protocol.c
)

target_include_directories(athol PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${EGL_INCLUDE_DIRS}
    ${LIBINPUT_INCLUDE_DIRS}
    ${LIBUDEV_INCLUDE_DIRS}
    ${WAYLAND_INCLUDE_DIRS}
)
target_link_libraries(athol
    ${EGL_LIBRARIES}
    ${LIBINPUT_LIBRARIES}
    ${LIBUDEV_LIBRARIES}
    ${WAYLAND_LIBRARIES}
    dl
)
install(TARGETS athol DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

set(Athol_INSTALLED_HEADERS
    API/Interfaces.h
)
install(FILES ${Athol_INSTALLED_HEADERS} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/Athol")
install(FILES ${CMAKE_BINARY_DIR}/athol.pc DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
