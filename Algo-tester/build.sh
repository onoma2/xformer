#!/bin/bash

# Build script for PEW|FORMER Algorithm Tester

echo "Building PEW|FORMER Algorithm Tester..."

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Run CMake
echo "Configuring with CMake..."
cmake ..

# Build the project
echo "Building project..."
make

echo "Build complete!"
echo "Run with: ./algo_tester"