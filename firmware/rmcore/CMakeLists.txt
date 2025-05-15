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
add_dependencies(rmcore generate_wavetable)

# Include paths
target_include_directories(rmcore PUBLIC
    ./include
    ../include
)

# Link with JACK
target_link_libraries(rmcore jack readline)

# Configure plugins

file(GLOB PLUGIN_SOURCES "plugins/src/*.cpp")

add_library(bogaudiodsp SHARED IMPORTED)
set_target_properties(bogaudiodsp PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/plugins/libbogaudiodsp.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/plugins/BogaudioModules/src/dsp"
)

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
endforeach()

target_sources(slew PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/src/slew_common.cpp)
target_link_libraries(slew PRIVATE bogaudiodsp)
target_include_directories(slew 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/src
)

target_sources(bogvcf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/src/dsp/filters/multimode.cpp)
target_link_libraries(bogvcf PRIVATE bogaudiodsp)
target_include_directories(bogvcf 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BogaudioModules/src
)