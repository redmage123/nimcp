#!/bin/bash
################################################################################
# NIMCP Installation Script v2.7.0
################################################################################
#
# WHAT: Complete installation script for NIMCP on fresh Linux system
# WHY:  Automate setup of NIMCP library, bindings, and web demo
# HOW:  Install dependencies, build library, configure environment
#
# Usage:
#   chmod +x install.sh
#   ./install.sh
#
# Tested on: Ubuntu 20.04+, Debian 11+, Amazon Linux 2
################################################################################

set -e  # Exit on error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Log functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Detect OS
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
        OS_VERSION=$VERSION_ID
    else
        log_error "Cannot detect OS. /etc/os-release not found."
        exit 1
    fi

    log_info "Detected OS: $OS $OS_VERSION"
}

# Check if running as root
check_root() {
    if [ "$EUID" -eq 0 ]; then
        log_warning "Running as root. Will install system packages."
        SUDO=""
    else
        log_info "Running as non-root user. Will use sudo for system packages."
        SUDO="sudo"
    fi
}

# Install system dependencies
install_system_deps() {
    log_info "Installing system dependencies..."

    case $OS in
        ubuntu|debian)
            $SUDO apt-get update
            $SUDO apt-get install -y \
                build-essential \
                cmake \
                git \
                python3 \
                python3-pip \
                python3-dev \
                libjansson-dev \
                liblz4-dev \
                pkg-config \
                curl \
                wget
            ;;
        centos|rhel|fedora|amzn)
            $SUDO yum install -y \
                gcc \
                gcc-c++ \
                make \
                cmake \
                git \
                python3 \
                python3-pip \
                python3-devel \
                jansson-devel \
                lz4-devel \
                pkgconfig \
                curl \
                wget
            ;;
        *)
            log_error "Unsupported OS: $OS"
            log_info "Please install these packages manually:"
            log_info "  - build-essential/gcc/g++"
            log_info "  - cmake"
            log_info "  - python3 and python3-dev"
            log_info "  - libjansson-dev"
            log_info "  - liblz4-dev"
            exit 1
            ;;
    esac

    log_success "System dependencies installed"
}

# Install Node.js and npm
install_nodejs() {
    log_info "Checking Node.js installation..."

    if command -v node &> /dev/null; then
        NODE_VERSION=$(node -v)
        log_info "Node.js already installed: $NODE_VERSION"
    else
        log_info "Installing Node.js..."

        # Install Node.js 18.x LTS
        curl -fsSL https://deb.nodesource.com/setup_18.x | $SUDO -E bash -
        $SUDO apt-get install -y nodejs

        log_success "Node.js installed: $(node -v)"
    fi

    log_success "npm version: $(npm -v)"
}

# Get repository root
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
log_info "Repository root: $REPO_ROOT"

# Build NIMCP core library
build_nimcp() {
    log_info "Building NIMCP core library..."

    cd "$REPO_ROOT"

    # Create build directory
    mkdir -p build
    cd build

    # Run CMake
    log_info "Running CMake..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local

    # Build
    log_info "Compiling NIMCP (this may take a few minutes)..."
    make -j$(nproc)

    # Check if library was built
    if [ -f "$REPO_ROOT/bin/libnimcp.so.2.5.0" ]; then
        log_success "NIMCP library built successfully"
    else
        log_error "NIMCP library build failed"
        exit 1
    fi
}

# Install Python bindings
install_python_bindings() {
    log_info "Installing NIMCP Python bindings..."

    cd "$REPO_ROOT/src/bindings/python"

    # Install Python package
    python3 setup.py install --user

    # Verify installation
    if python3 -c "import nimcp; print(f'NIMCP Python version: {nimcp.__version__}')" 2>/dev/null; then
        log_success "NIMCP Python bindings installed"
    else
        log_error "NIMCP Python bindings installation failed"
        exit 1
    fi
}

# Install web demo dependencies
install_web_demo() {
    log_info "Installing web demo dependencies..."

    # Backend dependencies
    log_info "Installing Flask backend dependencies..."
    cd "$REPO_ROOT/src/bindings/web-demo/backend"
    pip3 install --user -r requirements.txt

    # Frontend dependencies
    log_info "Installing React frontend dependencies..."
    cd "$REPO_ROOT/src/bindings/web-demo/frontend"
    npm install --legacy-peer-deps

    log_success "Web demo dependencies installed"
}

# Create environment setup script
create_env_setup() {
    log_info "Creating environment setup script..."

    cat > "$REPO_ROOT/setup_env.sh" << 'EOF'
#!/bin/bash
################################################################################
# NIMCP Environment Setup
################################################################################
#
# WHAT: Set up environment variables for NIMCP
# WHY:  Required for running NIMCP applications
# HOW:  Source this file before running NIMCP programs
#
# Usage:
#   source setup_env.sh
#   # OR
#   . setup_env.sh
################################################################################

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Set library path
export LD_LIBRARY_PATH="${SCRIPT_DIR}/bin:$LD_LIBRARY_PATH"

# Set Python path (if needed)
export PYTHONPATH="${SCRIPT_DIR}/src/bindings/python:$PYTHONPATH"

# Add Flask to PATH
export PATH="$HOME/.local/bin:$PATH"

# Verify setup
if [ -f "${SCRIPT_DIR}/bin/libnimcp.so" ]; then
    echo "✓ NIMCP environment configured"
    echo "  Library path: ${SCRIPT_DIR}/bin"
else
    echo "✗ Warning: NIMCP library not found at ${SCRIPT_DIR}/bin/libnimcp.so"
fi

# Python module check
if python3 -c "import nimcp" 2>/dev/null; then
    echo "✓ NIMCP Python module available"
else
    echo "✗ Warning: NIMCP Python module not found"
fi
EOF

    chmod +x "$REPO_ROOT/setup_env.sh"
    log_success "Environment setup script created: setup_env.sh"
}

# Create startup scripts for web demo
create_startup_scripts() {
    log_info "Creating web demo startup scripts..."

    # Backend startup script
    cat > "$REPO_ROOT/src/bindings/web-demo/start_backend.sh" << 'EOF'
#!/bin/bash
################################################################################
# Start NIMCP Web Demo Backend
################################################################################

# Get repo root (3 levels up from this script)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Source environment
source "$REPO_ROOT/setup_env.sh"

# Start backend
cd "$SCRIPT_DIR/backend"
echo "Starting NIMCP Web Demo Backend..."
echo "Access API at: http://localhost:5000"
python3 app.py
EOF

    chmod +x "$REPO_ROOT/src/bindings/web-demo/start_backend.sh"

    # Frontend startup script
    cat > "$REPO_ROOT/src/bindings/web-demo/start_frontend.sh" << 'EOF'
#!/bin/bash
################################################################################
# Start NIMCP Web Demo Frontend
################################################################################

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$SCRIPT_DIR/frontend"
echo "Starting NIMCP Web Demo Frontend..."
echo "Access UI at: http://localhost:3000"
HOST=localhost npm start
EOF

    chmod +x "$REPO_ROOT/src/bindings/web-demo/start_frontend.sh"

    # Combined startup script
    cat > "$REPO_ROOT/src/bindings/web-demo/start_demo.sh" << 'EOF'
#!/bin/bash
################################################################################
# Start NIMCP Web Demo (Backend + Frontend)
################################################################################

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=========================================="
echo "Starting NIMCP Web Demo"
echo "=========================================="
echo ""

# Start backend in background
echo "Starting backend..."
"$SCRIPT_DIR/start_backend.sh" &
BACKEND_PID=$!

# Wait for backend to start
sleep 5

# Start frontend
echo "Starting frontend..."
"$SCRIPT_DIR/start_frontend.sh" &
FRONTEND_PID=$!

echo ""
echo "=========================================="
echo "NIMCP Web Demo Started"
echo "=========================================="
echo "Backend:  http://localhost:5000"
echo "Frontend: http://localhost:3000"
echo ""
echo "Press Ctrl+C to stop both servers"
echo ""

# Wait for user interrupt
trap "kill $BACKEND_PID $FRONTEND_PID 2>/dev/null; exit" INT TERM

wait
EOF

    chmod +x "$REPO_ROOT/src/bindings/web-demo/start_demo.sh"

    log_success "Startup scripts created"
}

# Create systemd service files (optional)
create_systemd_services() {
    log_info "Creating systemd service files..."

    cat > "$REPO_ROOT/systemd/nimcp-backend.service" << EOF
[Unit]
Description=NIMCP Web Demo Backend
After=network.target

[Service]
Type=simple
User=$USER
WorkingDirectory=$REPO_ROOT/src/bindings/web-demo/backend
Environment="LD_LIBRARY_PATH=$REPO_ROOT/bin"
Environment="PATH=/usr/local/bin:/usr/bin:/bin:$HOME/.local/bin"
ExecStart=/usr/bin/python3 app.py
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOF

    cat > "$REPO_ROOT/systemd/nimcp-frontend.service" << EOF
[Unit]
Description=NIMCP Web Demo Frontend
After=network.target nimcp-backend.service

[Service]
Type=simple
User=$USER
WorkingDirectory=$REPO_ROOT/src/bindings/web-demo/frontend
Environment="HOST=localhost"
ExecStart=/usr/bin/npm start
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOF

    mkdir -p "$REPO_ROOT/systemd"

    log_success "Systemd service files created in systemd/"
    log_info "To install services:"
    log_info "  sudo cp systemd/*.service /etc/systemd/system/"
    log_info "  sudo systemctl daemon-reload"
    log_info "  sudo systemctl enable nimcp-backend nimcp-frontend"
    log_info "  sudo systemctl start nimcp-backend nimcp-frontend"
}

# Create README for deployment
create_deployment_readme() {
    log_info "Creating deployment README..."

    cat > "$REPO_ROOT/DEPLOYMENT.md" << 'EOF'
# NIMCP Deployment Guide

## Quick Start

After cloning this repository, run the installation script:

```bash
chmod +x install.sh
./install.sh
```

This will:
- Install all system dependencies
- Build NIMCP core library
- Install Python bindings
- Set up web demo dependencies
- Create startup scripts

## Environment Setup

Before running NIMCP applications, source the environment setup script:

```bash
source setup_env.sh
```

Or add to your `~/.bashrc` for automatic setup:

```bash
echo "source $PWD/setup_env.sh" >> ~/.bashrc
source ~/.bashrc
```

## Running the Web Demo

### Method 1: Combined Script (Easiest)

```bash
cd src/bindings/web-demo
./start_demo.sh
```

This starts both backend and frontend. Access at:
- Frontend: http://localhost:3000
- Backend API: http://localhost:5000

### Method 2: Separate Terminals

**Terminal 1 - Backend:**
```bash
cd src/bindings/web-demo
./start_backend.sh
```

**Terminal 2 - Frontend:**
```bash
cd src/bindings/web-demo
./start_frontend.sh
```

### Method 3: Production with Systemd

```bash
# Install services
sudo cp systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload

# Enable and start
sudo systemctl enable nimcp-backend nimcp-frontend
sudo systemctl start nimcp-backend nimcp-frontend

# Check status
sudo systemctl status nimcp-backend
sudo systemctl status nimcp-frontend

# View logs
sudo journalctl -u nimcp-backend -f
sudo journalctl -u nimcp-frontend -f
```

## Production Deployment with Nginx

### Install Nginx

```bash
sudo apt-get install nginx
```

### Configure Nginx

Create `/etc/nginx/sites-available/nimcp`:

```nginx
server {
    listen 80;
    server_name your-domain.com;

    # Frontend (React build)
    location / {
        root /path/to/nimcp/src/bindings/web-demo/frontend/build;
        try_files $uri $uri/ /index.html;
    }

    # Backend API
    location /api {
        proxy_pass http://localhost:5000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_cache_bypass $http_upgrade;
    }
}
```

Enable site:

```bash
sudo ln -s /etc/nginx/sites-available/nimcp /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx
```

### Build React for Production

```bash
cd src/bindings/web-demo/frontend
npm run build
```

## Cloud Server Setup

### AWS EC2

1. Launch Ubuntu 22.04 instance
2. Open ports 80, 443, 3000, 5000 in security group
3. SSH into instance
4. Clone repository
5. Run `./install.sh`
6. Follow production deployment steps

### Google Cloud Platform

1. Create Compute Engine VM (Ubuntu 22.04)
2. Add firewall rules for ports 80, 443, 3000, 5000
3. SSH into VM
4. Clone repository
5. Run `./install.sh`
6. Follow production deployment steps

### DigitalOcean

1. Create Droplet (Ubuntu 22.04)
2. Configure firewall
3. SSH into droplet
4. Clone repository
5. Run `./install.sh`
6. Follow production deployment steps

## Troubleshooting

### Library not found error

```bash
source setup_env.sh
echo $LD_LIBRARY_PATH  # Should include /path/to/nimcp/bin
```

### Python module not found

```bash
cd src/bindings/python
python3 setup.py install --user
```

### Frontend won't start

```bash
cd src/bindings/web-demo/frontend
rm -rf node_modules package-lock.json
npm install --legacy-peer-deps --force
```

### Backend 500 errors

Check backend logs and ensure NIMCP Python module is installed correctly:

```bash
python3 -c "import nimcp; print(nimcp.__version__)"
```

## Environment Variables

### Backend

- `FLASK_ENV=production` - Set for production mode
- `LD_LIBRARY_PATH` - Must include NIMCP bin directory

### Frontend

- `REACT_APP_API_URL` - Backend API URL (default: http://localhost:5000)
- `HOST=localhost` - Bind host for development server

## Security Notes

- Change Flask secret key for production
- Use HTTPS in production (Let's Encrypt)
- Configure firewall rules
- Use environment variables for sensitive data
- Run as non-root user

## Support

- Documentation: docs/
- Issues: GitHub issue tracker
- Web Demo README: src/bindings/web-demo/README.md
EOF

    log_success "Deployment README created: DEPLOYMENT.md"
}

# Main installation flow
main() {
    echo "=========================================="
    echo "NIMCP Installation Script v2.7.0"
    echo "=========================================="
    echo ""

    detect_os
    check_root

    log_info "Starting installation..."
    echo ""

    # Install dependencies
    install_system_deps
    install_nodejs

    # Build NIMCP
    build_nimcp

    # Install bindings
    install_python_bindings

    # Install web demo
    install_web_demo

    # Create helper scripts
    create_env_setup
    create_startup_scripts
    create_systemd_services
    create_deployment_readme

    echo ""
    echo "=========================================="
    log_success "NIMCP Installation Complete!"
    echo "=========================================="
    echo ""
    log_info "Next steps:"
    echo ""
    echo "1. Set up environment:"
    echo "   source setup_env.sh"
    echo ""
    echo "2. Start web demo:"
    echo "   cd src/bindings/web-demo"
    echo "   ./start_demo.sh"
    echo ""
    echo "3. Access web UI:"
    echo "   http://localhost:3000"
    echo ""
    echo "For deployment instructions, see: DEPLOYMENT.md"
    echo ""
}

# Run main installation
main "$@"
