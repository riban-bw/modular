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
file(GLOB PLUGIN_SOURCES "plugins/src/*.cpp")
add_executable(rmcore
    src/main.cpp
    src/usart.cpp
    src/module.cpp
    src/moduleManager.cpp
    src/util.cpp
    ${PLUGIN_SOURCES}
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
target_link_libraries(rmcore jack)
