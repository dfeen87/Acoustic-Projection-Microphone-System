#!/bin/bash
# start-apm.sh - Unix/Linux/Mac launcher script
# Production-ready APM system startup

set -e  # Exit on error

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

echo ""
echo "========================================"
echo "APM System Launcher (Unix/Linux/Mac)"
echo "========================================"
echo ""

# Check Node.js
if ! command -v node &> /dev/null; then
    error "Node.js not found!"
    error "Install from https://nodejs.org/"
    exit 1
fi
info "Node.js: $(node --version)"

# Check CMake
if ! command -v cmake &> /dev/null; then
    error "CMake not found!"
    error "Install CMake: brew install cmake (Mac) or apt install cmake (Linux)"
    exit 1
fi
info "CMake: $(cmake --version | head -n1)"

# Check compiler
COMPILER=""
if command -v g++ &> /dev/null; then
    COMPILER="g++"
elif command -v clang++ &> /dev/null; then
    COMPILER="clang++"
else
    error "No C++ compiler found!"
    error "Install g++ or clang++: brew install gcc (Mac) or apt install g++ (Linux)"
    exit 1
fi
info "Compiler: $COMPILER ($(${COMPILER} --version | head -n1))"

# Install dependencies if needed
if [ ! -d "launcher/node_modules" ]; then
    info "Installing dependencies..."
    cd launcher
    npm install
    if [ $? -ne 0 ]; then
        cd ..
        error "Failed to install dependencies"
        exit 1
    fi
    cd ..
    success "Dependencies installed"
fi

# Check if backend is built
BACKEND_FOUND=false
for path in "apm_backend" "build/apm_backend" "build/Release/apm_backend" "build/Debug/apm_backend"; do
    if [ -f "$path" ]; then
        BACKEND_FOUND=true
        BACKEND_PATH="$path"
        break
    fi
done

if [ "$BACKEND_FOUND" = false ]; then
    info "Backend not found, building..."
    
    # Create build directory
    mkdir -p build
    
    # Configure
    info "Configuring CMake..."
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    if [ $? -ne 0 ]; then
        error "CMake configuration failed"
        exit 1
    fi
    
    # Build
    info "Building backend..."
    cmake --build build --config Release
    if [ $? -ne 0 ]; then
        error "Build failed"
        exit 1
    fi
    
    # Find the built binary
    for path in "build/apm_backend" "apm_backend"; do
        if [ -f "$path" ]; then
            BACKEND_PATH="$path"
            break
        fi
    done
    
    success "Backend built: $BACKEND_PATH"
fi

# Make backend executable (if not already)
if [ -f "$BACKEND_PATH" ]; then
    if [ ! -x "$BACKEND_PATH" ]; then
        info "Making backend executable..."
        chmod +x "$BACKEND_PATH"
    fi
fi

# Start the system
echo ""
info "Starting APM System..."
echo ""

cd launcher
npm start
EXIT_CODE=$?

# Return to original directory
cd ..

if [ $EXIT_CODE -ne 0 ]; then
    echo ""
    error "APM System exited with code $EXIT_CODE"
    exit $EXIT_CODE
fi

success "APM System shutdown complete"
exit 0
