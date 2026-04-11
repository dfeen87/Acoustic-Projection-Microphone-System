#!/bin/bash
#
# APM Headphone Translator - End User Setup Script
# One-click installation for casual users
#
#

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'
BOLD='\033[1m'

# Spinner animation
spinner() {
    local pid=$1
    local delay=0.1
    local spinstr='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
    while [ "$(ps a | awk '{print $1}' | grep $pid)" ]; do
        local temp=${spinstr#?}
        printf " [%c]  " "$spinstr"
        local spinstr=$temp${spinstr%"$temp"}
        sleep $delay
        printf "\b\b\b\b\b\b"
    done
    printf "    \b\b\b\b"
}

# Banner
clear
cat << "EOF"
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║         🎧  APM HEADPHONE TRANSLATOR  🌍                  ║
║                                                           ║
║         Transform Your Headphones Into a                  ║
║         Real-Time Translation Device!                     ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo ""
echo -e "${BLUE}Version 7.0.0 | Copyright © 2025 Don Michael Feeney Jr.${NC}"
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then 
    echo -e "${RED}❌ Please do not run this script as root/sudo${NC}"
    echo "   Run as your normal user. Sudo will be requested when needed."
    exit 1
fi

# Detect OS
echo -e "${BOLD}Detecting your system...${NC}"
OS="unknown"
ARCH=$(uname -m)

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
    fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
else
    echo -e "${RED}❌ Unsupported operating system: $OSTYPE${NC}"
    echo "   Currently supports: Linux and macOS"
    exit 1
fi

echo -e "${GREEN}✓${NC} Detected: $OS ($ARCH)"
echo ""

# Check disk space
REQUIRED_SPACE=500000 # 500MB in KB
AVAILABLE_SPACE=$(df . | tail -1 | awk '{print $4}')

if [ $AVAILABLE_SPACE -lt $REQUIRED_SPACE ]; then
    echo -e "${RED}❌ Insufficient disk space${NC}"
    echo "   Required: 500MB | Available: $((AVAILABLE_SPACE/1024))MB"
    exit 1
fi

echo -e "${GREEN}✓${NC} Sufficient disk space available"
echo ""

# Installation options
echo -e "${BOLD}Installation Options:${NC}"
echo ""
echo "  1) ${BOLD}Quick Install${NC} (Recommended)"
echo "     → Install with default settings"
echo "     → Download offline language packs"
echo "     → Create desktop shortcut"
echo ""
echo "  2) ${BOLD}Custom Install${NC}"
echo "     → Choose installation directory"
echo "     → Select specific languages"
echo "     → Advanced configuration"
echo ""
echo "  3) ${BOLD}Minimal Install${NC}"
echo "     → Online-only mode (no offline packs)"
echo "     → Smallest download (~50MB)"
echo ""
read -p "Enter choice [1-3]: " INSTALL_TYPE

INSTALL_DIR="$HOME/.local/share/apm-headphone"
DOWNLOAD_PACKS=true
CREATE_SHORTCUTS=true

case $INSTALL_TYPE in
    2)
        echo ""
        read -p "Installation directory [$INSTALL_DIR]: " CUSTOM_DIR
        if [ ! -z "$CUSTOM_DIR" ]; then
            INSTALL_DIR="$CUSTOM_DIR"
        fi
        
        echo ""
        echo "Download offline language packs?"
        echo "  (Required for offline translation)"
        read -p "Download packs? [Y/n]: " DOWNLOAD_CHOICE
        if [[ $DOWNLOAD_CHOICE =~ ^[Nn]$ ]]; then
            DOWNLOAD_PACKS=false
        fi
        ;;
    3)
        DOWNLOAD_PACKS=false
        echo ""
        echo -e "${YELLOW}⚠ Minimal install: Internet required for translation${NC}"
        ;;
esac

echo ""
echo -e "${BOLD}Installation Summary:${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Install to: $INSTALL_DIR"
echo "  Offline packs: $([ "$DOWNLOAD_PACKS" = true ] && echo "Yes" || echo "No")"
echo "  Desktop shortcut: $([ "$CREATE_SHORTCUTS" = true ] && echo "Yes" || echo "No")"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
read -p "Continue with installation? [Y/n]: " CONFIRM

if [[ $CONFIRM =~ ^[Nn]$ ]]; then
    echo "Installation cancelled."
    exit 0
fi

# Create directories
echo ""
echo -e "${BOLD}Setting up directories...${NC}"
mkdir -p "$INSTALL_DIR"/{bin,lib,share,config}
echo -e "${GREEN}✓${NC} Directories created"

# Download application
echo ""
echo -e "${BOLD}Downloading APM Headphone Translator...${NC}"

RELEASE_URL="https://github.com/yourrepo/apm/releases/latest/download"
if [ "$OS" = "linux" ]; then
    BINARY_URL="$RELEASE_URL/apm-headphone-linux-$ARCH.tar.gz"
elif [ "$OS" = "macos" ]; then
    BINARY_URL="$RELEASE_URL/apm-headphone-macos-$ARCH.tar.gz"
fi

# Download with progress
curl -L --progress-bar "$BINARY_URL" -o /tmp/apm-headphone.tar.gz || {
    echo -e "${RED}❌ Download failed${NC}"
    echo "   Please check your internet connection"
    exit 1
}

echo -e "${GREEN}✓${NC} Download complete"

# Extract
echo ""
echo -e "${BOLD}Installing application...${NC}"
tar -xzf /tmp/apm-headphone.tar.gz -C "$INSTALL_DIR"
chmod +x "$INSTALL_DIR/bin/apm-headphone"
rm /tmp/apm-headphone.tar.gz
echo -e "${GREEN}✓${NC} Application installed"

# Download language packs
if [ "$DOWNLOAD_PACKS" = true ]; then
    echo ""
    echo -e "${BOLD}Downloading offline language packs...${NC}"
    echo "  This may take a few minutes..."
    
    mkdir -p "$INSTALL_DIR/share/models"
    
    PACKS=("en-es" "en-ja" "en-fr" "en-de" "en-zh")
    PACK_NAMES=("English-Spanish" "English-Japanese" "English-French" "English-German" "English-Chinese")
    
    for i in "${!PACKS[@]}"; do
        PACK="${PACKS[$i]}"
        NAME="${PACK_NAMES[$i]}"
        
        echo -n "  📦 $NAME... "
        curl -sSL "$RELEASE_URL/models-$PACK.tar.gz" -o /tmp/model-$PACK.tar.gz 2>&1 | \
            grep -o '[0-9]\+%' | tail -1 &
        PID=$!
        spinner $PID
        
        if [ -f /tmp/model-$PACK.tar.gz ]; then
            tar -xzf /tmp/model-$PACK.tar.gz -C "$INSTALL_DIR/share/models"
            rm /tmp/model-$PACK.tar.gz
            echo -e "${GREEN}✓${NC}"
        else
            echo -e "${YELLOW}⚠ skipped${NC}"
        fi
    done
    
    echo -e "${GREEN}✓${NC} Language packs installed"
fi

# Install system dependencies
echo ""
echo -e "${BOLD}Installing system dependencies...${NC}"

install_deps_linux() {
    if command -v apt-get &> /dev/null; then
        sudo apt-get update -qq
        sudo apt-get install -y -qq libfftw3-3 libasound2 > /dev/null 2>&1
    elif command -v dnf &> /dev/null; then
        sudo dnf install -y -q fftw-libs alsa-lib > /dev/null 2>&1
    elif command -v pacman &> /dev/null; then
        sudo pacman -S --noconfirm fftw alsa-lib > /dev/null 2>&1
    fi
}

install_deps_macos() {
    if ! command -v brew &> /dev/null; then
        echo -e "${YELLOW}⚠ Homebrew not found${NC}"
        echo "   Install from: https://brew.sh"
        read -p "Continue without dependencies? [y/N]: " SKIP_DEPS
        if [[ ! $SKIP_DEPS =~ ^[Yy]$ ]]; then
            exit 1
        fi
        return
    fi
    brew install fftw portaudio > /dev/null 2>&1
}

if [ "$OS" = "linux" ]; then
    install_deps_linux &
    PID=$!
    spinner $PID
elif [ "$OS" = "macos" ]; then
    install_deps_macos &
    PID=$!
    spinner $PID
fi

echo -e "${GREEN}✓${NC} Dependencies installed"

# Create launcher script
echo ""
echo -e "${BOLD}Creating launcher...${NC}"

cat > "$INSTALL_DIR/bin/apm-launcher" << 'LAUNCHER_EOF'
#!/bin/bash
# APM Headphone Launcher

INSTALL_DIR="$(dirname "$(dirname "$(readlink -f "$0")")")"
export LD_LIBRARY_PATH="$INSTALL_DIR/lib:$LD_LIBRARY_PATH"
export APM_DATA_DIR="$INSTALL_DIR/share"
export APM_CONFIG_DIR="$INSTALL_DIR/config"

cd "$INSTALL_DIR"
exec "$INSTALL_DIR/bin/apm-headphone" "$@"
LAUNCHER_EOF

chmod +x "$INSTALL_DIR/bin/apm-launcher"

# Add to PATH
if [ "$OS" = "linux" ]; then
    BIN_LINK="$HOME/.local/bin/apm-headphone"
    mkdir -p "$HOME/.local/bin"
    ln -sf "$INSTALL_DIR/bin/apm-launcher" "$BIN_LINK"
    
    # Add to .bashrc if not already there
    if ! grep -q "$HOME/.local/bin" "$HOME/.bashrc" 2>/dev/null; then
        echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$HOME/.bashrc"
    fi
fi

echo -e "${GREEN}✓${NC} Launcher created"

# Create desktop entry (Linux)
if [ "$OS" = "linux" ] && [ "$CREATE_SHORTCUTS" = true ]; then
    echo ""
    echo -e "${BOLD}Creating desktop shortcut...${NC}"
    
    DESKTOP_FILE="$HOME/.local/share/applications/apm-headphone.desktop"
    mkdir -p "$HOME/.local/share/applications"
    
    cat > "$DESKTOP_FILE" << DESKTOP_EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=APM Headphone Translator
Comment=Real-time language translation for headphones
Exec=$INSTALL_DIR/bin/apm-launcher
Icon=$INSTALL_DIR/share/icons/apm-headphone.png
Terminal=false
Categories=Audio;AudioVideo;Utility;
Keywords=translation;language;headphone;audio;
StartupNotify=true
DESKTOP_EOF

    chmod +x "$DESKTOP_FILE"
    
    # Update desktop database
    if command -v update-desktop-database &> /dev/null; then
        update-desktop-database "$HOME/.local/share/applications" 2>/dev/null
    fi
    
    echo -e "${GREEN}✓${NC} Desktop shortcut created"
fi

# Create default config
echo ""
echo -e "${BOLD}Creating default configuration...${NC}"

cat > "$INSTALL_DIR/config/settings.json" << 'CONFIG_EOF'
{
    "version": "7.0.0",
    "audio": {
        "sample_rate": 48000,
        "buffer_size": 960,
        "input_device": "default",
        "output_device": "default"
    },
    "translation": {
        "source_language": "en-US",
        "target_language": "es-ES",
        "mode": "online",
        "quality": "balanced"
    },
    "processing": {
        "noise_cancellation": 70,
        "echo_cancellation": 100,
        "voice_activity_detection": true
    },
    "ui": {
        "show_transcripts": true,
        "compact_mode": false,
        "notifications": true
    }
}
CONFIG_EOF

echo -e "${GREEN}✓${NC} Configuration created"

# Installation complete!
echo ""
echo -e "${GREEN}${BOLD}╔═══════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}${BOLD}║                                                       ║${NC}"
echo -e "${GREEN}${BOLD}║          ✨  INSTALLATION COMPLETE! ✨               ║${NC}"
echo -e "${GREEN}${BOLD}║                                                       ║${NC}"
echo -e "${GREEN}${BOLD}╚═══════════════════════════════════════════════════════╝${NC}"
echo ""

echo -e "${BOLD}🎧 Quick Start Guide:${NC}"
echo ""
echo "  1. Connect your headphones/headset"
echo "  2. Launch the application:"

if [ "$OS" = "linux" ]; then
    echo "     • Search for 'APM Headphone Translator' in your apps menu"
    echo "     • Or run: ${BLUE}apm-headphone${NC}"
elif [ "$OS" = "macos" ]; then
    echo "     • Open from Applications folder"
    echo "     • Or run: ${BLUE}$INSTALL_DIR/bin/apm-launcher${NC}"
fi

echo "  3. Select your microphone and speakers"
echo "  4. Choose your languages"
echo "  5. Start translating!"
echo ""

echo -e "${BOLD}📚 Documentation:${NC}"
echo "  User Guide: $INSTALL_DIR/share/docs/INSTALL_USER.md"
echo "  Online Help: https://docs.apm-system.com"
echo ""

echo -e "${BOLD}💡 Tips:${NC}"
echo "  • Test your audio setup before first use"
echo "  • Speak clearly and at a normal pace"
echo "  • Use 'Balanced' quality for best results"
if [ "$DOWNLOAD_PACKS" = false ]; then
    echo "  • ${YELLOW}Internet required (no offline packs installed)${NC}"
fi
echo ""

# Ask to launch now
read -p "Launch APM Headphone Translator now? [Y/n]: " LAUNCH_NOW

if [[ ! $LAUNCH_NOW =~ ^[Nn]$ ]]; then
    echo ""
    echo "Starting application..."
    
    if [ "$OS" = "linux" ]; then
        nohup "$INSTALL_DIR/bin/apm-launcher" > /dev/null 2>&1 &
    elif [ "$OS" = "macos" ]; then
        open "$INSTALL_DIR/bin/apm-launcher"
    fi
    
    sleep 2
    echo -e "${GREEN}✓${NC} Application launched!"
fi

echo ""
echo "Thank you for installing APM Headphone Translator!"
echo ""
echo -e "${BLUE}Having issues? Email: support@apm-system.com${NC}"
echo ""

# Save install info
cat > "$INSTALL_DIR/install-info.txt" << INFO_EOF
APM Headphone Translator
Installation Date: $(date)
Version: 7.0.0
Install Directory: $INSTALL_DIR
OS: $OS ($ARCH)
Offline Packs: $DOWNLOAD_PACKS
INFO_EOF

exit 0
