#!/bin/bash
# APM System Setup Script
# Automates dependency installation and project configuration

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Banner
echo "============================================"
echo "  APM System Setup Script"
echo "  Version 1.0.0"
echo "============================================"
echo ""

# Detect OS
log_info "Detecting operating system..."
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    if [ -f /etc/debian_version ]; then
        DISTRO="debian"
    elif [ -f /etc/redhat-release ]; then
        DISTRO="redhat"
    fi
    log_success "Detected Linux ($DISTRO)"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    log_success "Detected macOS"
else
    log_error "Unsupported OS: $OSTYPE"
    exit 1
fi

# Check for required commands
check_command() {
    if command -v $1 &> /dev/null; then
        log_success "$1 is installed"
        return 0
    else
        log_warning "$1 is not installed"
        return 1
    fi
}

log_info "Checking for required tools..."
HAS_CMAKE=$(check_command cmake && echo "yes" || echo "no")
HAS_GCC=$(check_command g++ && echo "yes" || echo "no")
HAS_GIT=$(check_command git && echo "yes" || echo "no")

# Install dependencies
install_deps() {
    log_info "Installing dependencies..."
    
    if [ "$OS" == "linux" ]; then
        if [ "$DISTRO" == "debian" ]; then
            sudo apt-get update
            sudo apt-get install -y \
                build-essential \
                cmake \
                git \
                libfftw3-dev \
                libfftw3-single3 \
                pkg-config
        elif [ "$DISTRO" == "redhat" ]; then
            sudo yum install -y \
                gcc-c++ \
                cmake \
                git \
                fftw-devel
        fi
    elif [ "$OS" == "macos" ]; then
        if ! check_command brew; then
            log_error "Homebrew not found. Install from https://brew.sh"
            exit 1
        fi
        brew install cmake fftw
    fi
    
    log_success "Dependencies installed"
}

# Optional: Install TensorFlow Lite
install_tflite() {
    log_info "Installing TensorFlow Lite (optional)..."
    
    if check_command python3; then
        pip3 install --user tensorflow 2>/dev/null || {
            log_warning "Failed to install TensorFlow. Translation features will use mock engine."
            return 1
        }
        log_success "TensorFlow Lite support installed"
    else
        log_warning "Python3 not found. Skipping TensorFlow Lite."
        return 1
    fi
}

# Configure project
configure_project() {
    log_info "Configuring project..."
    
    BUILD_TYPE="${BUILD_TYPE:-Release}"
    BUILD_TESTS="${BUILD_TESTS:-ON}"
    BUILD_BENCHMARKS="${BUILD_BENCHMARKS:-OFF}"
    
    mkdir -p build
    cd build
    
    cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DBUILD_TESTS=$BUILD_TESTS \
        -DBUILD_BENCHMARKS=$BUILD_BENCHMARKS \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    
    cd ..
    log_success "Project configured (build type: $BUILD_TYPE)"
}

# Build project
build_project() {
    log_info "Building project..."
    
    cd build
    cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    cd ..
    
    log_success "Build complete"
}

# Run tests
run_tests() {
    log_info "Running tests..."
    
    cd build
    if ctest --output-on-failure; then
        log_success "All tests passed"
    else
        log_error "Some tests failed"
        return 1
    fi
    cd ..
}

# Create example config
create_config() {
    log_info "Creating example configuration..."
    
    cat > config.json << 'EOF'
{
    "microphones": {
        "count": 4,
        "spacing_m": 0.012,
        "sample_rate": 48000
    },
    "speakers": {
        "count": 3,
        "spacing_m": 0.015
    },
    "translation": {
        "source_language": "en-US",
        "target_language": "es-ES"
    },
    "processing": {
        "beamforming": "delay_and_sum",
        "noise_suppression": true,
        "echo_cancellation": true,
        "vad_enabled": true
    }
}
EOF
    
    log_success "Configuration file created: config.json"
}

# Main menu
show_menu() {
    echo ""
    echo "Select an option:"
    echo "1) Install dependencies"
    echo "2) Install TensorFlow Lite (optional)"
    echo "3) Configure project"
    echo "4) Build project"
    echo "5) Run tests"
    echo "6) Full setup (1-5)"
    echo "7) Create example config"
    echo "8) Clean build directory"
    echo "9) Exit"
    echo ""
    read -p "Enter choice [1-9]: " choice
    
    case $choice in
        1) install_deps ;;
        2) install_tflite ;;
        3) configure_project ;;
        4) build_project ;;
        5) run_tests ;;
        6) 
            install_deps
            install_tflite
            configure_project
            build_project
            run_tests
            create_config
            log_success "Full setup complete!"
            ;;
        7) create_config ;;
        8)
            log_info "Cleaning build directory..."
            rm -rf build
            log_success "Build directory cleaned"
            ;;
        9)
            log_info "Goodbye!"
            exit 0
            ;;
        *)
            log_error "Invalid option"
            ;;
    esac
    
    show_menu
}

# Parse command line arguments
INTERACTIVE=true
while [[ $# -gt 0 ]]; do
    case $1 in
        --full)
            INTERACTIVE=false
            install_deps
            install_tflite || true
            configure_project
            build_project
            run_tests
            create_config
            log_success "Full automated setup complete!"
            exit 0
            ;;
        --deps-only)
            INTERACTIVE=false
            install_deps
            exit 0
            ;;
        --build-only)
            INTERACTIVE=false
            configure_project
            build_project
            exit 0
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --full        Run full automated setup"
            echo "  --deps-only   Install dependencies only"
            echo "  --build-only  Configure and build only"
            echo "  --help        Show this help message"
            echo ""
            echo "Environment variables:"
            echo "  BUILD_TYPE         Build type (Debug/Release, default: Release)"
            echo "  BUILD_TESTS        Build tests (ON/OFF, default: ON)"
            echo "  BUILD_BENCHMARKS   Build benchmarks (ON/OFF, default: OFF)"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
    shift
done

# Run interactive menu if no arguments provided
if [ "$INTERACTIVE" = true ]; then
    show_menu
fi
