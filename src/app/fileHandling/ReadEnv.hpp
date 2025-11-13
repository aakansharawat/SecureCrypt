// ReadEnv.hpp (NEW FILE)
#ifndef READENV_HPP
#define READENV_HPP

#include <string>

class ReadEnv {
public:
    // We make this function static since it doesn't need class state.
    static std::string getenv(); 
};

#endif