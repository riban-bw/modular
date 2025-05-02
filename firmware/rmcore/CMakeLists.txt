cmake_minimum_required(VERSION 3.11)
project("riban modular" VERSION 0.0.2)

# Date Variables
string(TIMESTAMP TODAY "%Y-%m-%d")
string(TIMESTAMP THIS_YEAR "%Y")
set(PROJECT_BUILD_DATE ${TODAY})
set(PROJECT_BUILD_YEAR ${THIS_YEAR})

# Configure version.h
configure_file(version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h @ONLY)

# Add the buildWavetable executable
add_executable(buildWavetable src/buildWavetable.cpp)

# Define path to the actual header that buildWavetable generates
set(WAVETABLE_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/wavetable.h)

# Add a custom command that generates wavetable.h
add_custom_command(
    OUTPUT ${WAVETABLE_HEADER}
    COMMAND buildWavetable
    DEPENDS buildWavetable
    COMMENT "Generating real wavetable.h"
    VERBATIM
)

# Create a target that represents the generated header
add_custom_target(generate_wavetable DEPENDS ${WAVETABLE_HEADER})

# Add rmcore sources
add_executable(rmcore
    src/main.cpp
    src/usart.cpp
    src/moduleManager.cpp
    src/util.cpp
)

# Ensure rmcore builds after wavetable.h is generated
add_dependencies(rmcore generate_wavetable)

# Include paths
target_include_directories(rmcore PUBLIC
    ./include
    ../include
    ./plugins/include
)

# Link with JACK
target_link_libraries(rmcore jack readline)

# Configure plugins

file(GLOB PLUGIN_SOURCES "plugins/src/*.cpp")

foreach(plugin_src ${PLUGIN_SOURCES})
    get_filename_component(plugin_name ${plugin_src} NAME_WE)

    add_library(${plugin_name} SHARED ${plugin_src} src/util.cpp)

    target_include_directories(${plugin_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/include
    )

    # If your plugins need to link to anything, do it here:
    # target_link_libraries(${plugin_name} ...)

    set_target_properties(${plugin_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/plugins
    )
endforeach()
