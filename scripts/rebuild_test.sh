#!/bin/bash
cd /home/bbrelin/nimcp/build
cmake ..
make unit_middleware_events_event_queue -j$(nproc)
