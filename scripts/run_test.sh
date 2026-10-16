#!/bin/bash
set -e
cd /home/bbrelin/nimcp
export LD_LIBRARY_PATH=/home/bbrelin/nimcp/build/src:/home/bbrelin/nimcp/build/lib:$LD_LIBRARY_PATH
./test/unit_middleware_events_test_event_queue
