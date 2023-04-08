#include "system/application.h"

int main(int argc, char** argv) {
    nemo::Application::GetInstance().run(argc, argv);
    return 0;
}