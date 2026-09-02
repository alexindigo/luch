cmake_minimum_required(VERSION 3.21)

project(Luch VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core DBus Gui Quick Widgets
                                            WaylandClient
                                            WaylandScannerTools)
find_package(LayerShellQt REQUIRED)
find_package(WaylandScanner REQUIRED)
# seccomp network sandbox for the offline plugin worker.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SECCOMP REQUIRED IMPORTED_TARGET libseccomp)
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
    src/augmentationpipeline.cpp
    src/augmentationpipeline.h
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
    src/settings.cpp
    src/settings.h
    src/singleinstance.cpp
    src/singleinstance.h
    src/target.cpp
    src/target.h
    src/urltools.cpp
    src/urltools.h
    src/worker/inetworker.cpp
    src/worker/inetworker.h
    src/worker/offlineworker.cpp
    src/worker/offlineworker.h
    src/worker/workerrunner.h
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
        ui/DissectionPanel.qml
        ui/LightsStrip.qml
        ui/Main.qml
        ui/QueueChrome.qml
        ui/SettingsWindow.qml
        ui/TargetFooter.qml
        ui/VariantPills.qml
        ui/XdgIconLoader.qml
        ui/decompose.js
)

target_include_directories(luch PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(luch PRIVATE
    LUCH_INSTALLED_PLUGINS_DIR="${CMAKE_INSTALL_FULL_LIBDIR}/luch/plugins"
    LUCH_INSTALLED_LISTS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/luch/lists")
# qpa native-interface headers (for the wl_surface of the popup) live in
# the versioned private include tree; Arch's Qt targets don't export it.
target_include_directories(luch PRIVATE
    "${Qt6Gui_DIR}/../../../include/qt6/QtGui/${Qt6Gui_VERSION}")

target_link_libraries(luch PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Quick
    Qt6::Widgets
    Qt6::WaylandClient
    LayerShellQt::Interface
    PkgConfig::SECCOMP
    luch-urlclean-engine
)

# URL-cleaning engine (QtCore-only): shared by the urlclean plugin and
# linkable by future plugins; PIC because plugins are MODULE libraries.
add_library(luch-urlclean-engine STATIC
    src/effectivetld.cpp
    src/effectivetld.h
    src/urlcleaner.cpp
    src/urlcleaner.h
)
set_target_properties(luch-urlclean-engine PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
target_include_directories(luch-urlclean-engine PUBLIC src)
target_link_libraries(luch-urlclean-engine PUBLIC Qt6::Core)

# Stage plugins over the URL-cleaning engine: each calls one stage of
# luch-urlclean-engine and ships as a runtime-loadable .so (+ sidecar
# manifest <id>.json installed next to it).
add_library(luch-plugin-debounce MODULE
    src/plugins/debounce/debounceplugin.cpp
    src/plugins/debounce/debounceplugin.h
)
set_target_properties(luch-plugin-debounce PROPERTIES
    PREFIX "" OUTPUT_NAME "debounce")
target_include_directories(luch-plugin-debounce PRIVATE src/pluginapi)
target_compile_definitions(luch-plugin-debounce PRIVATE
    LUCH_INSTALLED_LISTS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/luch/lists")
target_link_libraries(luch-plugin-debounce PRIVATE
    luch-urlclean-engine Qt6::Core)

add_library(luch-plugin-brave-clean-url MODULE
    src/plugins/brave-clean-url/bravecleanurlplugin.cpp
    src/plugins/brave-clean-url/bravecleanurlplugin.h
)
set_target_properties(luch-plugin-brave-clean-url PROPERTIES
    PREFIX "" OUTPUT_NAME "brave-clean-url")
target_include_directories(luch-plugin-brave-clean-url PRIVATE
    src/pluginapi)
target_compile_definitions(luch-plugin-brave-clean-url PRIVATE
    LUCH_INSTALLED_LISTS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/luch/lists")
target_link_libraries(luch-plugin-brave-clean-url PRIVATE
    luch-urlclean-engine Qt6::Core)

install(TARGETS luch RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY assets/lists/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/luch/lists")
install(FILES src/pluginapi/luchaugmenter.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/luch")
install(TARGETS luch-plugin-debounce luch-plugin-brave-clean-url
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/luch/plugins")
install(FILES src/plugins/debounce/debounceplugin.json
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/luch/plugins"
        RENAME debounce.json)
install(FILES src/plugins/brave-clean-url/bravecleanurlplugin.json
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/luch/plugins"
        RENAME brave-clean-url.json)
install(FILES luch.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES assets/luch.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
foreach(luch_icon_size 256 128 64 48 32)
    install(FILES "assets/luch-${luch_icon_size}.png"
            DESTINATION
                "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${luch_icon_size}x${luch_icon_size}/apps"
            RENAME luch.png)
endforeach()

# Verification plugins — built, never installed.
add_subdirectory(tests)
