#!/bin/bash
# ===============================================
# Unix Run Script (Mac & Ubuntu)
# ===============================================

echo ""
echo "==============================================="
echo "  Running Application - Unix"
echo "==============================================="
echo ""

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "ERROR: Build directory not found!"
    echo "Please run ./build_and_run.sh first to build the project."
    exit 1
fi

# Check if executable exists
if [ ! -f "build/Sandbox/MyGame" ]; then
    echo "ERROR: Executable not found!"
    echo "Please run ./build_and_run.sh first to build the project."
    exit 1
fi

echo "Running the application..."
echo ""
cd build/Sandbox
./MyGame
if [ $? -ne 0 ]; then
    echo ""
    echo "Application exited with error code: $?"
else
    echo ""
    echo "Application completed successfully!"
fi

echo ""
echo "Script completed!"
