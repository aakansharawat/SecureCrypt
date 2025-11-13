#pragma once

#include <pthread.h> // For pthread_mutex_t
#include <unistd.h>  // For basic definitions (optional but harmless)

// Configuration Constants
#define SHM_NAME "/my_queue" // Must match the name used in ProcessManagement.cpp
#define MAX_TASKS 1000
#define TASK_STR_LEN 256 // Must match the size used in ProcessManagement.cpp

struct SharedMemory {
    // 1. The Circular Buffer (Task Queue)
    // char tasks[1000][256]
    char tasks[MAX_TASKS][TASK_STR_LEN]; 
    
    // 2. Queue Management Indices
    int front; // Read index (where the consumer takes from)
    int rear;  // Write index (where the producer puts to)

    // NOTE: The size counter (std::atomic<int> size) is removed 
    // to rely purely on semaphores and mutex for synchronization,
    // which is the safer and more standard pattern for IPC queue management.

    // 3. CRITICAL Synchronization Primitive
    // Used for mutual exclusion (locking the queue during read/write)
    pthread_mutex_t queue_mutex;
};