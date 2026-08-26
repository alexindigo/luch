cmake_minimum_required(VERSION 3.21)

project(Luch VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick)
find_package(LayerShellQt REQUIRED)

qt_add_executable(luch
    src/main.cpp
    src/pickerwindow.cpp
    src/pickerwindow.h
)

qt_add_qml_module(luch
    URI Luch
    VERSION 1.0
    QML_FILES
        ui/Main.qml
)

target_include_directories(luch PRIVATE src)

target_link_libraries(luch PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    LayerShellQt::Interface
)

install(TARGETS luch RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
