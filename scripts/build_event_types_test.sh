#!/bin/bash
cd /home/bbrelin/nimcp/build
cmake ..
cmake --build . --target unit_middleware_events_event_types -j4
