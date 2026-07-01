#!/bin/bash

# Deadlock Visualizer Project Runner Script

# Exit immediately if a command exits with a non-zero status
set -e

echo "=================================================="
echo " Preparing Deadlock Visualizer..."
echo "=================================================="

# Check if gcc is available
if ! command -v gcc &> /dev/null; then
    echo "Error: gcc is not installed. Please install a C compiler."
    exit 1
fi

# Check if node is available
if ! command -v node &> /dev/null; then
    echo "Error: Node.js is not installed. Please install Node.js (v12+)."
    exit 1
fi

echo "1. Building C Simulation Engine..."
make clean
make

echo "2. Launching Node.js Backend Server..."
echo "Starting server at http://localhost:3000"
echo "Press Ctrl+C to terminate the simulation and server."
echo ""

node server.js
