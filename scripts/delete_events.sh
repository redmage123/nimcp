#!/bin/bash
set -x
rm -fv /home/bbrelin/nimcp/test/unit/middleware/events/test_event_types.cpp
rm -fv /home/bbrelin/nimcp/test/unit/middleware/events/test_event_queue.cpp
rm -fv /home/bbrelin/nimcp/test/unit/middleware/events/test_event_subscriber.cpp
rm -fv /home/bbrelin/nimcp/test/unit/middleware/events/test_event_bus.cpp
rm -fv /home/bbrelin/nimcp/test/unit/middleware/events/CMakeLists.txt
rm -fv /home/bbrelin/nimcp/test/regression/middleware/events/test_event_types_regression.cpp
rm -fv /home/bbrelin/nimcp/test/regression/middleware/events/test_event_queue_regression.cpp
rm -fv /home/bbrelin/nimcp/test/regression/middleware/events/test_event_subscriber_regression.cpp
rm -fv /home/bbrelin/nimcp/test/regression/middleware/events/CMakeLists.txt
rm -rfv /home/bbrelin/nimcp/test/unit/middleware/events
rm -rfv /home/bbrelin/nimcp/test/regression/middleware/events
echo "Deletion complete"
