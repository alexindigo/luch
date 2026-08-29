cmake_minimum_required(VERSION 3.21)

project(Luch VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core DBus Gui Quick WaylandClient
                                            WaylandScannerTools)
find_package(LayerShellQt REQUIRED)
find_package(WaylandScanner REQUIRED)
find_path(WAYLAND_PROTOCOLS_PATH
          staging/xdg-activation/xdg-activation-v1.xml
          PATHS /usr/share/wayland-protocols
          REQUIRED)
# XdgIcon QML module — resolves real browser icons in the picker.
find_package(xdgiconqml REQUIRED)
# xdgiconqml exports xdgiconqml::xdgiconqml; Qt's QML import machinery looks
# for the module's qmldir linktarget by name. Alias it so the `import XdgIcon`
# in ui/XdgIconLoader.qml resolves without a "link target does not exist" warning.
if(TARGET xdgiconqml::xdgiconqml AND NOT TARGET xdgiconqmlplugin)
    add_library(xdgiconqmlplugin ALIAS xdgiconqml::xdgiconqml)
endif()

qt_add_executable(luch
    src/browserregistry.cpp
    src/browserregistry.h
    src/chromiumsocket.cpp
    src/chromiumsocket.h
    src/config.cpp
    src/config.h
    src/dbustransport.cpp
    src/dbustransport.h
    src/launcher.cpp
    src/launcher.h
    src/main.cpp
    src/pickerwindow.cpp
    src/pickerwindow.h
    src/queue.cpp
    src/queue.h
    src/singleinstance.cpp
    src/singleinstance.h
    src/target.cpp
    src/target.h
    src/xdgactivation.cpp
    src/xdgactivation.h
)

qt6_generate_wayland_protocol_client_sources(luch
    FILES ${WAYLAND_PROTOCOLS_PATH}/staging/xdg-activation/xdg-activation-v1.xml
)

qt_add_qml_module(luch
    URI Luch
    VERSION 1.0
    QML_FILES
        ui/BrowserDelegate.qml
        ui/Main.qml
        ui/QueueChrome.qml
        ui/TargetFooter.qml
        ui/XdgIconLoader.qml
)

target_include_directories(luch PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
# qpa native-interface headers (for the wl_surface of the popup) live in
# the versioned private include tree; Arch's Qt targets don't export it.
target_include_directories(luch PRIVATE
    "${Qt6Gui_DIR}/../../../include/qt6/QtGui/${Qt6Gui_VERSION}")

target_link_libraries(luch PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Quick
    Qt6::WaylandClient
    LayerShellQt::Interface
)

install(TARGETS luch RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES luch.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES assets/luch.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
foreach(luch_icon_size 256 128 64 48 32)
    install(FILES "assets/luch-${luch_icon_size}.png"
            DESTINATION
                "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${luch_icon_size}x${luch_icon_size}/apps"
            RENAME luch.png)
endforeach()
