#!/bin/bash
set -x
cd /home/bbrelin/nimcp
g++ -c test/unit/middleware/events/test_event_queue.cpp \
    -I include \
    -I /usr/include/gtest \
    -std=c++17 \
    -o /tmp/test_event_queue.o 2>&1
