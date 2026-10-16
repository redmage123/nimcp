#!/bin/bash
set -e

cd /home/bbrelin/nimcp

echo "Compiling nimcp_event_types.c..."
gcc -c src/middleware/events/nimcp_event_types.c \
    -I include \
    -I src \
    -std=c11 \
    -o /tmp/nimcp_event_types.o

echo "Compiling test_event_types.cpp..."
g++ -c test/unit/middleware/events/test_event_types.cpp \
    -I include \
    -I /usr/include \
    -std=c++11 \
    -o /tmp/test_event_types.o

echo "Linking..."
g++ /tmp/test_event_types.o /tmp/nimcp_event_types.o \
    -L build/src/middleware \
    -L build/src \
    -lnimcp_middleware \
    -lnimcp \
    -lgtest \
    -lgtest_main \
    -lpthread \
    -o /tmp/unit_middleware_events_event_types

echo "Running tests..."
/tmp/unit_middleware_events_event_types

echo "Done!"
