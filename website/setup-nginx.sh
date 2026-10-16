#!/bin/bash
# Setup script for nimcp.ai-elevate.ai
# Run as: sudo bash website/setup-nginx.sh

set -e

echo "=== NIMCP Website — Nginx Setup ==="

# 1. Create nginx config
cat > /etc/nginx/sites-available/nimcp << 'NGINX'
server {
    listen 80;
    listen [::]:80;
    server_name nimcp.ai-elevate.ai;

    location / {
        proxy_pass http://127.0.0.1:8090;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
NGINX
echo "[1/5] Nginx config created"

# 2. Enable the site (skip if already linked)
if [ ! -L /etc/nginx/sites-enabled/nimcp ]; then
    ln -s /etc/nginx/sites-available/nimcp /etc/nginx/sites-enabled/nimcp
    echo "[2/5] Site enabled"
else
    echo "[2/5] Site already enabled"
fi

# 3. Test config
echo "[3/5] Testing nginx config..."
nginx -t

# 4. Reload nginx
systemctl reload nginx
echo "[4/5] Nginx reloaded"

# 5. Get SSL cert
echo "[5/5] Requesting SSL certificate..."
certbot --nginx -d nimcp.ai-elevate.ai --non-interactive --agree-tos --email braun.brelin@ai-elevate.ai

echo ""
echo "=== Done ==="
echo "Site live at: https://nimcp.ai-elevate.ai"
echo ""
echo "NOTE: Make sure you have a DNS A or CNAME record:"
echo "  nimcp.ai-elevate.ai -> 176.9.99.103"
