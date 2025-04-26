cmake_minimum_required(VERSION 3.11)
project("riban modular" VERSION 0.0.2)
string(TIMESTAMP TODAY "%Y-%m-%d")
string(TIMESTAMP THIS_YEAR "%Y")
set (PROJECT_BUILD_DATE ${TODAY})
set (PROJECT_BUILD_YEAR ${THIS_YEAR})
configure_file (version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h @ONLY)

file(GLOB PLUGIN_SOURCES "plugins/src/*.cpp")
add_executable(rmcore
    src/main.cpp
    src/usart.cpp
    src/node.cpp
    src/moduleManager.cpp
    ${PLUGIN_SOURCES}
    )
target_include_directories(rmcore PUBLIC ./include ../include ./plugins/include)
target_link_libraries(rmcore jack)

add_executable(buildWavetable src/buildWavetable.cpp)
