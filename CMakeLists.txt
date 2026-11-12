cmake_minimum_required(VERSION 3.16)

project(VisionFlow
    VERSION 0.1.0
    DESCRIPTION "Real-time distributed computer vision platform"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(VISIONFLOW_BUILD_TESTS "Build VisionFlow tests" ON)

find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio highgui)

function(visionflow_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    endif()
endfunction()

# Core library: everything except the application entry point, so later
# phases (and tests) can link against it without pulling in main().
add_library(visionflow_core
    src/video_source.cpp
)
target_include_directories(visionflow_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(visionflow_core
    PUBLIC
        ${OpenCV_LIBS}
)
target_include_directories(visionflow_core SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})
visionflow_enable_warnings(visionflow_core)

add_executable(visionflow
    src/main.cpp
)
target_link_libraries(visionflow
    PRIVATE
        visionflow_core
)
visionflow_enable_warnings(visionflow)

if(VISIONFLOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
