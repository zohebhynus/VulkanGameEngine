

#include "Application.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main()
{
    Application App;

    try 
    {
        App.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}