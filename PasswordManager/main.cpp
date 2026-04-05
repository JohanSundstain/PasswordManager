#include <iostream>

#include "PasswordManager.h"

int main() 
{
    PasswordManager pm;

    if (!SetConsoleCtrlHandler(PasswordManager::ctrl_handler, TRUE))
    {
        return 1; 
    }
    try
    {
        pm.run();
    }
    catch (const std::runtime_error& re)
    {
        std::cerr << re.what() << std::endl;
        return 1;
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "cannot allocate memory" << std::endl;
        return 1;
    }

    return 0;
}