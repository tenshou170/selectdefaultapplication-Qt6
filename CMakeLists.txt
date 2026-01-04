cmake_minimum_required(VERSION 3.16)

project(selectdefaultapplication-Qt6 VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    selectdefaultapplication.cpp
    selectdefaultapplication.h
    xdgmimeapps.cpp
    xdgmimeapps.h
)

add_executable(sda-qt6
    ${PROJECT_SOURCES}
)

target_link_libraries(sda-qt6 PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

# Install target
install(TARGETS sda-qt6
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
