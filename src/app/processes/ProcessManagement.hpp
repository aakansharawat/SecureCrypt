// processes/ProcessManagement.hpp (CORRECTED)
#ifndef PROCESS_MANAGEMENT_HPP
#define PROCESS_MANAGEMENT_HPP

#include "Task.hpp"
#include "SharedMemory.h" // Includes SharedMemory struct and SHM_NAME
#include <memory>
#include <semaphore.h>
#include <vector>
#include <unistd.h> // For pid_t

class ProcessManagement
{
private:
    sem_t* itemsSemaphore;
    sem_t* emptySlotsSemaphore;
    
    SharedMemory* sharedMem; // Uses the struct from SharedMemory.h
    int shmFd;
    
    std::vector<pid_t> workerPIDs;
    int numWorkers;

    // Internal function where the actual work and file IO happens
    void executeCryption(const char* taskStr); 

public:
    // CRITICAL FIX 1: Constructor takes the number of workers and forks them.
    ProcessManagement(int numWorkers);
    ~ProcessManagement();
    
    // The producer (main process) puts tasks here.
    bool submitToQueue(std::unique_ptr<Task> task);
    
    // CRITICAL FIX 2: This is the infinite loop run by the child workers.
    void executeTaskLoop();

    // Termination
    void terminateWorkers();
    void joinWorkers();
};

#endif