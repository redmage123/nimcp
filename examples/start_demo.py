#!/usr/bin/env python3
"""
NIMCP Web Demo Launcher
Starts both backend (Flask API) and frontend (React/Vite) with a single command
Supports HTTP and HTTPS with auto-generated certificates
"""

import os
import sys
import time
import subprocess
import signal
import atexit
import argparse

# Process tracking
frontend_process = None
backend_process = None

def cleanup():
    """Cleanup function to terminate child processes"""
    global frontend_process, backend_process

    print("\n\nShutting down...")

    if frontend_process:
        print("Stopping frontend...")
        try:
            frontend_process.terminate()
            frontend_process.wait(timeout=5)
        except:
            frontend_process.kill()

    if backend_process:
        print("Stopping backend...")
        try:
            backend_process.terminate()
            backend_process.wait(timeout=5)
        except:
            backend_process.kill()

    print("Cleanup complete")

def signal_handler(signum, frame):
    """Handle Ctrl+C gracefully"""
    cleanup()
    sys.exit(0)

def generate_ssl_cert():
    """Generate self-signed SSL certificate for HTTPS"""
    cert_dir = os.path.join(os.path.dirname(__file__), 'web_demo', 'certs')
    cert_file = os.path.join(cert_dir, 'cert.pem')
    key_file = os.path.join(cert_dir, 'key.pem')

    # Check if certificates already exist
    if os.path.exists(cert_file) and os.path.exists(key_file):
        print(f"✓ SSL certificates found")
        return cert_file, key_file

    # Create certs directory
    os.makedirs(cert_dir, exist_ok=True)

    print("Generating self-signed SSL certificate...")

    # Generate certificate using openssl
    cmd = [
        'openssl', 'req', '-x509', '-newkey', 'rsa:4096',
        '-keyout', key_file,
        '-out', cert_file,
        '-days', '365',
        '-nodes',
        '-subj', '/CN=localhost'
    ]

    try:
        subprocess.run(cmd, check=True, capture_output=True)
        print(f"✓ SSL certificate generated: {cert_file}")
        return cert_file, key_file
    except subprocess.CalledProcessError as e:
        print(f"Warning: Could not generate SSL certificate: {e}")
        print("Falling back to HTTP mode")
        return None, None
    except FileNotFoundError:
        print("Warning: openssl not found. Falling back to HTTP mode")
        print("  Install openssl: sudo apt-get install openssl")
        return None, None

def start_frontend():
    """Start the React frontend with Vite"""
    global frontend_process

    frontend_dir = os.path.join(os.path.dirname(__file__), 'web_demo', 'frontend')

    if not os.path.exists(frontend_dir):
        print(f"Error: Frontend directory not found: {frontend_dir}")
        return False

    # Check if node_modules exists
    if not os.path.exists(os.path.join(frontend_dir, 'node_modules')):
        print("Installing frontend dependencies...")
        npm_install = subprocess.run(
            ['npm', 'install'],
            cwd=frontend_dir,
            capture_output=True,
            text=True
        )
        if npm_install.returncode != 0:
            print(f"Error installing dependencies: {npm_install.stderr}")
            return False

    print("Starting React frontend (Vite)...")

    try:
        frontend_process = subprocess.Popen(
            ['npm', 'run', 'dev'],
            cwd=frontend_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        # Wait a bit and check if it started successfully
        time.sleep(2)
        if frontend_process.poll() is not None:
            print("Error: Frontend failed to start")
            return False

        print("✓ Frontend started on http://localhost:5000")
        return True

    except FileNotFoundError:
        print("Error: npm not found. Please install Node.js")
        return False
    except Exception as e:
        print(f"Error starting frontend: {e}")
        return False

def start_backend(use_https=False):
    """Start the Flask backend"""
    global backend_process

    backend_script = os.path.join(os.path.dirname(__file__), 'web_demo', 'app.py')

    if not os.path.exists(backend_script):
        print(f"Error: Backend script not found: {backend_script}")
        return False

    print("Starting Flask backend...")

    # Set environment variable for HTTPS mode
    env = os.environ.copy()
    if use_https:
        cert_file, key_file = generate_ssl_cert()
        if cert_file and key_file:
            env['USE_HTTPS'] = '1'
            env['SSL_CERT'] = cert_file
            env['SSL_KEY'] = key_file

    try:
        backend_process = subprocess.Popen(
            [sys.executable, backend_script],
            cwd=os.path.dirname(backend_script),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            env=env
        )

        # Stream backend output
        print("=" * 70)
        for line in iter(backend_process.stdout.readline, ''):
            if line:
                print(line.rstrip())
            if "Press CTRL+C to quit" in line:
                break

        return True

    except Exception as e:
        print(f"Error starting backend: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Start NIMCP Web Demo')
    parser.add_argument('--https', action='store_true',
                       help='Enable HTTPS with self-signed certificate')
    parser.add_argument('--no-frontend', action='store_true',
                       help='Start backend only (useful if frontend is already running)')

    args = parser.parse_args()

    # Register cleanup handlers
    atexit.register(cleanup)
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    print("=" * 70)
    print("NIMCP Web Demo Launcher")
    print("=" * 70)

    # Start frontend first (unless disabled)
    if not args.no_frontend:
        if not start_frontend():
            print("\nFrontend failed to start. Continue anyway? (y/n): ", end='')
            if input().lower() != 'y':
                return 1
            print()

    # Start backend
    if not start_backend(use_https=args.https):
        cleanup()
        return 1

    # Keep the script running
    try:
        if backend_process:
            backend_process.wait()
    except KeyboardInterrupt:
        pass
    finally:
        cleanup()

    return 0

if __name__ == '__main__':
    sys.exit(main())
