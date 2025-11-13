// IO.cpp
#include <iostream>
#include "IO.hpp"

IO::IO(const std::string& file_path) {
    // Open in/out/binary mode for in-place encryption/decryption
    file_stream.open(file_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_stream.is_open()) {
        // NOTE: In a real application, you should throw an exception here
        std::cerr << "Unable to open file: " << file_path << std::endl;
    }
}

IO::~IO() {
    // The destructor ensures the file is closed when the IO object goes out of scope.
    if (file_stream.is_open()) {
        file_stream.close();
    }
}

// FIX: Return a reference. This allows external functions to use the stream 
// while the IO object still owns it.
std::fstream& IO::getFileStream() {
    return file_stream;
}