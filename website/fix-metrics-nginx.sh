#!/bin/bash
# Fix: serve metrics.json directly from host nginx, bypassing Docker
# Run as: sudo bash website/fix-metrics-nginx.sh

set -e

# Update the nimcp nginx config to serve metrics.json from the host filesystem
cat > /etc/nginx/sites-available/nimcp << 'NGINX'
server {
    listen 80;
    listen [::]:80;
    server_name nimcp.ai-elevate.ai;

    # Metrics JSON served directly from host — bypasses Docker volume mount caching
    location = /metrics.json {
        alias /home/bbrelin/nimcp/website/metrics.json;
        add_header Cache-Control "no-cache, no-store, must-revalidate";
        add_header Pragma "no-cache";
        add_header Expires "0";
        add_header Access-Control-Allow-Origin "*";
    }

    # Everything else proxied to Docker container
    location / {
        proxy_pass http://127.0.0.1:8090;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
NGINX

nginx -t && systemctl reload nginx
echo "Done — metrics.json now served directly from host"
echo "Test: curl https://nimcp.ai-elevate.ai/metrics.json"
