cmake_minimum_required(VERSION 3.21)

project(Luch VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick)
find_package(LayerShellQt REQUIRED)
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
    src/config.cpp
    src/config.h
    src/launcher.cpp
    src/launcher.h
    src/main.cpp
    src/pickerwindow.cpp
    src/pickerwindow.h
    src/target.cpp
    src/target.h
)

qt_add_qml_module(luch
    URI Luch
    VERSION 1.0
    QML_FILES
        ui/BrowserDelegate.qml
        ui/Main.qml
        ui/TargetFooter.qml
        ui/XdgIconLoader.qml
)

target_include_directories(luch PRIVATE src)

target_link_libraries(luch PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
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
