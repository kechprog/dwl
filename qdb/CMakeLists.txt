cmake_minimum_required(VERSION 3.10)

project(qdb)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(LayerShellQt REQUIRED)

add_executable(qdb src/main.cpp)

target_link_libraries(qdb Qt6::Core Qt6::Gui Qt6::Widgets LayerShellQt::Interface)
