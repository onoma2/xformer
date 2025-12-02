cmake_minimum_required(VERSION 3.16)
project(AlgoTester)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2
find_path(SDL2_INCLUDE_DIR
    NAMES SDL.h
    PATHS /opt/homebrew/Cellar/sdl2/2.32.10/include
    PATH_SUFFIXES SDL2)

find_library(SDL2_LIBRARY
    NAMES SDL2
    PATHS /opt/homebrew/Cellar/sdl2/2.32.10/lib)

# Find SDL2_ttf
find_path(SDL2_TTF_INCLUDE_DIR
    NAMES SDL_ttf.h
    PATHS /opt/homebrew/Cellar/sdl2_ttf/2.24.0/include
    PATH_SUFFIXES SDL2)

find_library(SDL2_TTF_LIBRARY
    NAMES SDL2_ttf
    PATHS /opt/homebrew/Cellar/sdl2_ttf/2.24.0/lib)

message(STATUS "SDL2_INCLUDE_DIR: ${SDL2_INCLUDE_DIR}")
message(STATUS "SDL2_LIBRARY: ${SDL2_LIBRARY}")
message(STATUS "SDL2_TTF_INCLUDE_DIR: ${SDL2_TTF_INCLUDE_DIR}")
message(STATUS "SDL2_TTF_LIBRARY: ${SDL2_TTF_LIBRARY}")

# Source files
set(SOURCES
    src/main.cpp
    src/AlgorithmProcessor.cpp
    src/AlgoVisualization.cpp
    src/AudioEngine.cpp
)

set(HEADERS
    include/AlgorithmProcessor.h
    include/AlgoVisualization.h
    include/AudioEngine.h
    include/dj_fft.h
)

# Create executable
add_executable(algo_tester ${SOURCES} ${HEADERS})

# Include directories
get_filename_component(SDL2_INCLUDE_PARENT_DIR ${SDL2_INCLUDE_DIR} DIRECTORY)
get_filename_component(SDL2_TTF_INCLUDE_PARENT_DIR ${SDL2_TTF_INCLUDE_DIR} DIRECTORY)

target_include_directories(algo_tester PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDL2_INCLUDE_PARENT_DIR}
    ${SDL2_TTF_INCLUDE_PARENT_DIR}
)

# Link libraries
if(SDL2_TTF_LIBRARY)
    target_link_libraries(algo_tester
        ${SDL2_LIBRARY}
        ${SDL2_TTF_LIBRARY}
        m
    )
    target_compile_definitions(algo_tester PRIVATE HAS_SDL2_TTF)
else()
    target_link_libraries(algo_tester
        ${SDL2_LIBRARY}
        m
    )
endif()

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(algo_tester PRIVATE -Wall -Wextra)
endif()

# Install target
install(TARGETS algo_tester DESTINATION bin)