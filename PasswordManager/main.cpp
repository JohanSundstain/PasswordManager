#define SODIUM_STATIC

#include <sodium.h>
#include <iostream>
#include <string>
#include <vector>
#include <conio.h>
#include <deque>
#include <variant>

#include "PasswordManager.h"
#include "BIP-39.h"
#include "uint264.h"

#include "GuardAllocator.hpp"


int main()
{
    std::vector<int, GuardAllocator<int>> v{1,2,3,4,5};

    for (int i = 0; i < v.size(); i++)
    {
        std::cout << v[i] << " ";
    }

    v.push_back(7);
    v.push_back(7);
    v.push_back(7);
    v.push_back(7);

    for (int i = 0; i < v.size(); i++)
    {
        std::cout << v[i] << " ";
    }



    return 0;
}
