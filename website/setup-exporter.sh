#!/bin/bash
# Setup systemd service for metrics exporter
# Run as: sudo bash website/setup-exporter.sh

set -e

cat > /etc/systemd/system/nimcp-metrics.service << 'EOF'
[Unit]
Description=NIMCP Metrics Exporter — writes brain metrics to JSON for website
After=athena-brain.service
Wants=athena-brain.service

[Service]
Type=simple
User=bbrelin
Group=bbrelin
WorkingDirectory=/home/bbrelin/nimcp
ExecStart=/usr/bin/python3 -u /home/bbrelin/nimcp/website/metrics_exporter.py
Restart=always
RestartSec=5

Environment=PYTHONPATH=/home/bbrelin/nimcp/scripts
Environment=LD_LIBRARY_PATH=/home/bbrelin/nimcp/build/lib

StandardOutput=journal
StandardError=journal
SyslogIdentifier=nimcp-metrics

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable nimcp-metrics
systemctl start nimcp-metrics

echo "Metrics exporter service installed and started"
echo "Check status: systemctl status nimcp-metrics"
echo "View logs: journalctl -u nimcp-metrics -f"
