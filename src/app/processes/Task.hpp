// processes/Task.hpp
#ifndef TASK_HPP
#define TASK_HPP

#include <string>
#include <memory> 

using namespace std;

// Define Task as an abstract base class
class Task {
public:
    virtual string toString() const = 0;
    virtual ~Task() = default;
};

// Concrete Task for file operations
class FileTask : public Task {
private:
    string filename;
    string action; // "ENCRYPT" or "DECRYPT"
    string secretKey; // optional; empty means use ENV key
public:
    FileTask(const string& name, const string& act)
        : filename(name), action(act), secretKey("") {}

    FileTask(const string& name, const string& act, const string& key)
        : filename(name), action(act), secretKey(key) {}
        
    // Method to serialize task into string for shared memory
    string toString() const override {
        // Format: ACTION,FILEPATH,KEY (KEY may be empty)
        return action + "," + filename + "," + secretKey;
    }
};

// Termination signal
class StopTask : public Task {
public:
    string toString() const override { return "STOP_WORKER_SIGNAL,"; }
};

#endif