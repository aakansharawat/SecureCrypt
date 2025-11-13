// ReadEnv.cpp (CORRECTED)
#include "ReadEnv.hpp" // Include its own header
#include "IO.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

// We implement the static method
std::string ReadEnv::getenv() {
    std::string env_path = ".env";
    
    // 1. Create IO object
    IO io(env_path);
    
    // FIX: Get a reference to the stream
    std::fstream& f_stream = io.getFileStream();

    if (!f_stream.is_open()) {
        std::cerr << "Error: Could not open .env file." << std::endl;
        return ""; // Return empty string on failure
    }

    // 2. Read content into stringstream
    std::stringstream buffer;
    buffer << f_stream.rdbuf(); // This reads the entire file content

    // NOTE: The IO destructor will handle closing f_stream when 'io' goes out of scope.
    
    return buffer.str();
}