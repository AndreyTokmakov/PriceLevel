/**============================================================================
Name        : main.cpp
Created on  : 
Author      : Andrei Tokmakov
Version     : 1.0
Copyright   : Your copyright notice
Description :
============================================================================**/

#include <iostream>
#include "Pool.hpp"


int main([[maybe_unused]] const int argc,
         [[maybe_unused]] char** argv)
{
    pools::pool_one::TestAll();

    return EXIT_SUCCESS;
}

