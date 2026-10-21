#include <iostream>
#include <stdexcept>
#include "src/engine/platform/window/Window.h"

int main()
{
    try{
        Window app;
        app.run();
    }
    catch (const std::exception& e){
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}