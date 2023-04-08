#include "coroutine/coroutine.h"

#include <iostream>

int main(int argc, char** argv) {
    coroutine_async [](){
        std::cout << "here" << std::endl;
    };

    coroutine_start;
    return 0;
}