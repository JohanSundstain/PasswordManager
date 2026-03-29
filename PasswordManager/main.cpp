#define SODIUM_STATIC

#include <sodium.h>
#include <iostream>
#include <string>
#include <vector>
#include <conio.h>
#include <deque>
#include <variant>

#include "PasswordManager.h"
#include "GuardAllocator.hpp"



int main()
{
    PasswordManager pm;

    pm.run();

    return 0;
}
