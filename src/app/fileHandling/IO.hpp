// IO.hpp
#ifndef IO_HPP
#define IO_HPP

#include <fstream>
#include <string>

class IO {
public:
    IO(const std::string& file_path);
    ~IO();
    
    // FIX: Return a reference to the stream, do NOT move it out.
    std::fstream& getFileStream();

private:
    // Disallow copying and moving to enforce single ownership
    IO(const IO&) = delete;
    IO& operator=(const IO&) = delete;
    
    std::fstream file_stream;
};

#endif