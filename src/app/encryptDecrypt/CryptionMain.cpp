#include <iostream>
#include "Cryption.hpp" 

using namespace std;

int main(int argc, char* argv[]) {

    if (argc != 2) {
        cerr << "Usage: ./cryption <task_data>" << endl;
        return 1;
    }
    
    processCryptionTask(argv[1]); 
    
    return 0;
}