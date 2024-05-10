

#include "Application.h"
#include "Instrumentation.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main()
{
    Application App;

    try 
    {
        PROFILE_BEGIN("Runtime", "Runtime.json");
        App.Run();
        PROFILE_END();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}