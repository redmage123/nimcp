// Standalone compilation test
#include <stdio.h>

extern "C" {
// Forward declarations to check if we can compile
typedef struct event_queue_struct* event_queue_t;
event_queue_t event_queue_create(const void* config);
void event_queue_destroy(event_queue_t queue);
}

int main() {
    printf("Test compilation successful\n");
    return 0;
}
