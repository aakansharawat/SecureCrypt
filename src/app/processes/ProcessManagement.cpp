#include <iostream>
#include <cstring>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "ProcessManagement.hpp"
#include "SharedMemory.h" 
#include "../fileHandling/IO.hpp"
#include "../encryptDecrypt/Cryption.hpp" 

using namespace std;

extern int processCryptionTask(const string& taskData); 

// --- Implementation of the internal work function ---
void ProcessManagement::executeCryption(const char* taskStr) {
    // Delegate the string parsing and file IO to the Cryption module.
    processCryptionTask(taskStr);
}
// ----------------------------------------------------


ProcessManagement::ProcessManagement(int numWorkers) : numWorkers(numWorkers) {
    sem_unlink("/items_semaphore");
    sem_unlink("/empty_slots_semaphore");
    itemsSemaphore      = sem_open("/items_semaphore", O_CREAT, 0666, 0); 
    emptySlotsSemaphore = sem_open("/empty_slots_semaphore", O_CREAT, 0666, MAX_TASKS); 

    if (itemsSemaphore == SEM_FAILED || emptySlotsSemaphore == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }
    shmFd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shmFd < 0) { perror("shm_open failed"); exit(EXIT_FAILURE); }
    if (ftruncate(shmFd, sizeof(SharedMemory)) < 0) { perror("ftruncate failed"); exit(EXIT_FAILURE); }
    
    sharedMem = static_cast<SharedMemory *>(mmap(nullptr, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0));
    if (sharedMem == MAP_FAILED) { perror("mmap failed"); exit(EXIT_FAILURE); }

    sharedMem->front = 0;
    sharedMem->rear = 0;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&sharedMem->queue_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    
    cout << "Starting " << numWorkers << " worker processes..." << endl;
    for (int i = 0; i < numWorkers; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
        } else if (pid == 0) {
            executeTaskLoop(); 
            _exit(0); 
        } else {



            workerPIDs.push_back(pid);
        }
    }
}

ProcessManagement::~ProcessManagement() {

    if (sharedMem != MAP_FAILED) {
        pthread_mutex_destroy(&sharedMem->queue_mutex);
        munmap(sharedMem, sizeof(SharedMemory));
    }
    if (shmFd >= 0) {
        close(shmFd);
        shm_unlink(SHM_NAME);
    }
    if (itemsSemaphore != SEM_FAILED) sem_close(itemsSemaphore);
    if (emptySlotsSemaphore != SEM_FAILED) sem_close(emptySlotsSemaphore);
    sem_unlink("/items_semaphore");
    sem_unlink("/empty_slots_semaphore");
}

bool ProcessManagement::submitToQueue(unique_ptr<Task> task) {
    if (sem_wait(emptySlotsSemaphore) != 0) {
        if (errno != EINTR) perror("sem_wait emptySlotsSemaphore failed");
        return false;
    }

    pthread_mutex_lock(&sharedMem->queue_mutex);
    
    const string taskStr = task->toString();
    strncpy(sharedMem->tasks[sharedMem->rear], taskStr.c_str(), TASK_STR_LEN - 1);
    sharedMem->tasks[sharedMem->rear][TASK_STR_LEN - 1] = '\0'; 
    sharedMem->rear = (sharedMem->rear + 1) % MAX_TASKS;

    pthread_mutex_unlock(&sharedMem->queue_mutex);

    // Signal that an item is available
    if (sem_post(itemsSemaphore) != 0) {
        perror("sem_post itemsSemaphore failed");
        sem_post(emptySlotsSemaphore); // Restore the slot
        return false;
    }
    
    return true;
}

// --- Consumer Loop (Worker's main function) ---
void ProcessManagement::executeTaskLoop() {
    while (true) {
        if (sem_wait(itemsSemaphore) != 0) {
            if (errno == EINTR) continue; 
            perror("sem_wait itemsSemaphore failed");
            break; 
        }

        // CRITICAL SECTION: Read from shared queue
        pthread_mutex_lock(&sharedMem->queue_mutex);
        
        char taskStr[TASK_STR_LEN];
        strcpy(taskStr, sharedMem->tasks[sharedMem->front]);
        sharedMem->front = (sharedMem->front + 1) % MAX_TASKS;

        pthread_mutex_unlock(&sharedMem->queue_mutex);

        // Signal that an empty slot is available
        sem_post(emptySlotsSemaphore); 

        // Check for termination signal
        if (strncmp(taskStr, "STOP_WORKER_SIGNAL", strlen("STOP_WORKER_SIGNAL")) == 0) {
            // Re-post the item to allow the next worker to get it and terminate
            sem_post(itemsSemaphore); 
            break;
        }

        // Execute the task
        executeCryption(taskStr);
    }
}

void ProcessManagement::terminateWorkers() {
    cout << "Sending termination signals to " << numWorkers << " workers..." << endl;
    for (int i = 0; i < numWorkers; ++i) {
        submitToQueue(make_unique<StopTask>());
    }
}

void ProcessManagement::joinWorkers() {
    for (pid_t pid : workerPIDs) {
        waitpid(pid, nullptr, 0);
    }
    cout << "All worker processes joined." << endl;
}