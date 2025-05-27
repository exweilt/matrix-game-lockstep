/**
 *  @file MatrixGameServer.cpp
 *
 *  @brief Contains entry point of matrix multiplayer lockstep relay server.
 *
 *  This file is only visible by MatrixGameServer CMake configuration.
 */

#include <iostream>

#include "MatrixGameServer.hpp"

#include "Types.hpp"
#include "Network/Network.hpp"
// #include <enet/enet.h>

int main()
{
    std::cout << "Game server started!" << static_cast<u8>(network::SideID::RED) << std::endl;
    // enet_initialize();
    return 0;
}