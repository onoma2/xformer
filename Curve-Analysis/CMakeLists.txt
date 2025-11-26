cmake_minimum_required(VERSION 3.10)
project(CurveAnalysis)

set(CMAKE_CXX_STANDARD 17)

# Find required packages
find_package(OpenGL REQUIRED)

# Find SDL2
find_path(SDL2_INCLUDE_DIR 
    NAMES SDL.h
    PATHS /opt/homebrew/Cellar/sdl2/2.32.10/include
    PATH_SUFFIXES SDL2)
    
find_library(SDL2_LIBRARY 
    NAMES SDL2
    PATHS /opt/homebrew/Cellar/sdl2/2.32.10/lib)

message(STATUS "SDL2_INCLUDE_DIR: ${SDL2_INCLUDE_DIR}")
message(STATUS "SDL2_LIBRARY: ${SDL2_LIBRARY}")

# Find SDL2_ttf
find_path(SDL2_TTF_INCLUDE_DIR 
    NAMES SDL_ttf.h
    PATHS /opt/homebrew/Cellar/sdl2_ttf/2.24.0/include
    PATH_SUFFIXES SDL2)
    
find_library(SDL2_TTF_LIBRARY 
    NAMES SDL2_ttf
    PATHS /opt/homebrew/Cellar/sdl2_ttf/2.24.0/lib)

message(STATUS "SDL2_TTF_INCLUDE_DIR: ${SDL2_TTF_INCLUDE_DIR}")
message(STATUS "SDL2_TTF_LIBRARY: ${SDL2_TTF_LIBRARY}")

# Define the executable
add_executable(curve_analysis
    main.cpp
    CurveProcessor.cpp
    Curve.h
    CurveProcessor.h
)

# Include directories - need to use parent directory of SDL2_INCLUDE_DIR
get_filename_component(SDL2_INCLUDE_PARENT_DIR ${SDL2_INCLUDE_DIR} DIRECTORY)
get_filename_component(SDL2_TTF_INCLUDE_PARENT_DIR ${SDL2_TTF_INCLUDE_DIR} DIRECTORY)

target_include_directories(curve_analysis PRIVATE
    ${SDL2_INCLUDE_PARENT_DIR}
    ${SDL2_TTF_INCLUDE_PARENT_DIR}
    ${OPENGL_INCLUDE_DIRS}
)

# Link libraries
if(SDL2_TTF_LIBRARY)
    target_link_libraries(curve_analysis
        ${SDL2_LIBRARY}
        ${SDL2_TTF_LIBRARY}
        ${OPENGL_LIBRARIES}
        m
    )
    target_compile_definitions(curve_analysis PRIVATE HAS_SDL2_TTF)
else()
    target_link_libraries(curve_analysis
        ${SDL2_LIBRARY}
        ${OPENGL_LIBRARIES}
        m
    )
endif()

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(curve_analysis PRIVATE -Wall -Wextra)
endif()