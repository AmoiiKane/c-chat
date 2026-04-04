#!/usr/bin/env bash
set -e

# ── Colors ──────────────────────────────────────────────────────
GRN='\033[0;32m'; YLW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
ok()   { echo -e "     ${GRN}OK${NC}"; }
info() { echo -e "  ${YLW}[!]${NC} $*"; }
die()  { echo -e "  ${RED}[✗]${NC} $*"; exit 1; }

echo ""
echo " ============================================="
echo "  c-chat Setup"
echo " ============================================="
echo ""

# ── Detect OS ──────────────────────────────────────────────────
OS="$(uname -s)"
DISTRO=""
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO="$ID"
fi

# ── Fix project structure ──────────────────────────────────────
echo " [1/4] Checking project structure..."
mkdir -p src bin

for f in client.c server.c common.h gui_client.c; do
    [ -f "$f" ] && mv "$f" src/ && echo "      Moved $f → src/"
done

[ -f src/server.c ] || die "src/server.c not found. Clone the full repo first."
ok

# ── Install dependencies ───────────────────────────────────────
echo " [2/4] Checking dependencies..."

install_deps() {
    case "$DISTRO" in
        arch|manjaro)
            sudo pacman -S --needed --noconfirm gcc make gtk3 webkit2gtk-4.1 ;;
        ubuntu|debian|linuxmint|pop)
            sudo apt-get update -qq
            sudo apt-get install -y gcc make libgtk-3-dev libwebkit2gtk-4.1-dev ;;
        fedora)
            sudo dnf install -y gcc make gtk3-devel webkit2gtk4.1-devel ;;
        opensuse*|sles)
            sudo zypper install -y gcc make gtk3-devel webkit2gtk3-soup2-devel ;;
        *)
            if [ "$OS" = "Darwin" ]; then
                command -v brew >/dev/null 2>&1 || \
                    die "Homebrew not found. Install it from https://brew.sh then re-run."
                brew install gcc make gtk+3 webkit2gtk
            else
                die "Unsupported distro: $DISTRO. Install gcc, make, gtk3, and webkit2gtk-4.1 manually."
            fi ;;
    esac
}

# Check what's missing
MISSING=""
command -v gcc  >/dev/null 2>&1 || MISSING="$MISSING gcc"
command -v make >/dev/null 2>&1 || MISSING="$MISSING make"
pkg-config --exists gtk+-3.0        2>/dev/null || MISSING="$MISSING gtk3"
pkg-config --exists webkit2gtk-4.1  2>/dev/null || MISSING="$MISSING webkit2gtk-4.1"

if [ -n "$MISSING" ]; then
    info "Missing:$MISSING — installing..."
    install_deps
else
    ok
fi

# ── Build ──────────────────────────────────────────────────────
echo " [3/4] Building c-chat..."
make re 2>&1 || die "Build failed. Check the errors above."
ok

# ── Done ───────────────────────────────────────────────────────
echo " [4/4] Done!"
echo ""
echo " ============================================="
echo -e "  ${GRN}Build complete!${NC}"
echo " ============================================="
echo ""
echo "  Binaries in bin/:"
[ -f bin/server ]     && echo "    bin/server         — run this on the host machine"
[ -f bin/client ]     && echo "    bin/client         — terminal client"
[ -f bin/gui_client ] && echo "    bin/gui_client     — graphical client (no terminal needed)"
echo ""
echo "  Quick start:"
echo "    1. On the server machine:  ./bin/server"
echo "    2. On client machines:     ./bin/gui_client"
echo "                            or ./bin/client <server-ip>"
echo ""
