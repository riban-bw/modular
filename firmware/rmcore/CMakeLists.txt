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
    src/rmcore.cpp
    src/usart.cpp
    src/moduleManager.cpp
    src/util.cpp
)

# Ensure rmcore builds after wavetable.h is generated
add_dependencies(rmcore generate_wavetable bogaudiodsp)

# Include paths
target_include_directories(rmcore PUBLIC
    ./include
    ../include
)

# Link with JACK
target_link_libraries(rmcore jack readline)

# Configure plugins

file(GLOB PLUGIN_SOURCES "plugins/src/*.cpp")

# Build Bogaudio DSP shared lib
file(GLOB BOG_SOURCES "plugins/BogaudioModules/src/dsp/*.cpp")
add_library("bogaudiodsp" SHARED ${BOG_SOURCES})
target_include_directories("bogaudiodsp" PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/src/dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/lib
)

set(MATCH_PATTERN "^bog.*")
foreach(plugin_src ${PLUGIN_SOURCES})
    get_filename_component(plugin_name ${plugin_src} NAME_WE)

    add_library(${plugin_name} SHARED ${plugin_src} src/util.cpp)

    target_include_directories(${plugin_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include # rmcore includes, e.g. module.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../include # global includes, e.g. global.h
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/include # plugin includes
    )

    set_target_properties(${plugin_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/plugins
    )

    string(REGEX MATCH ${MATCH_PATTERN} match ${plugin_name})
    if(match)
        target_include_directories(${plugin_name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/src
            ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/src/dsp
        )
        target_link_libraries(${plugin_name} PRIVATE bogaudiodsp)

    endif()
endforeach()

# Source for each Bogaudio plugin files
target_sources(bogslew PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/src/slew_common.cpp)

target_sources(bogvcf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/src/dsp/filters/multimode.cpp)

target_sources(bogvco PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/src/dsp/filters/experiments.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/src/dsp/filters/resample.cpp
)
