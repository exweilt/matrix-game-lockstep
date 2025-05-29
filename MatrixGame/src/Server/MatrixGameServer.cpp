/**
 *  @file MatrixGameServer.cpp
 *
 *  @brief Contains entry point of matrix multiplayer lockstep relay server.
 *
 *  This file is only visible by MatrixGameServer CMake configuration.
 */

#include <iostream>

#include "MatrixGameServer.hpp"

#include "Network/Message.hpp"
#include "Network/Network.hpp"
#include "Types.hpp"
#include <enet/enet.h>

ENetHost* g_server_host;

// void init_enet()
// {
//     if (enet_initialize () != 0)
//     {
//         fprintf (stderr, "An error occurred while initializing ENet.\n");
//         std::terminate();
//     }
// }

void init_server_host()
{
    ENetAddress address;
    /* enet_address_set_host (& address, "x.x.x.x"); */

    address.host = ENET_HOST_ANY;
    address.port = 1234;

    g_server_host = enet_host_create (& address /* the address to bind the server host to */,
                                 32      /* allow up to 32 clients and/or outgoing connections */,
                                  2      /* allow up to 2 channels to be used, 0 and 1 */,
                                  0      /* assume any amount of incoming bandwidth */,
                                  0      /* assume any amount of outgoing bandwidth */);
    if (g_server_host == nullptr)
    {
        fprintf (stderr, "An error occurred while trying to create an ENet server host.\n");
        std::terminate();
    }
}

void process_server_network_frame()
{
    ENetEvent event;
    while (enet_host_service (g_server_host, &event, 0) > 0)
    {
        std::cout << "New event: " << event.type << "\n";
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            std::cout << "Connected: " << event.peer->connectID << "\n\n";

            std::cout << "The full list of connected peers: " << g_server_host->connectedPeers << "\n" ;

            for (int i = 0; i < g_server_host->connectedPeers; i++)
            {
                std::cout << g_server_host->peers[i].connectID << std::endl;
            }

            if (g_server_host->connectedPeers == 2)
            {
                nw::Message msg { nw::MessageType::START };
                ENetPacket* packet = enet_packet_create(nullptr, msg.get_serialized_size(), ENET_PACKET_FLAG_RELIABLE);
                memcpy(packet->data, &msg, msg.get_serialized_size());
                enet_host_broadcast(g_server_host, 0, packet);
            }

            break;
        case ENET_EVENT_TYPE_RECEIVE:
            std::cout << "\nGot new packet from " << event.peer->connectID << ":\n";
            if (g_server_host->connectedPeers < 2)
            {
                std::cerr << "Only one player...\n";
                std::terminate();
            }

            ENetPeer* target;

            for (int i = 0; i < g_server_host->connectedPeers; i++)
            {
                if (g_server_host->peers[i].connectID != event.peer->connectID)
                {
                    target = g_server_host->peers + i;
                    break;
                }
            }

            network::Message m { nw::Message::deserialize_from_buffer(event.packet->data) };

            std::cout << "Relaying to " << target->connectID << "\n\n";
            enet_peer_send(target, 0, event.packet);

            enet_packet_destroy(event.packet);
            break;
        }
    }

}

int main()
{
    std::cout << "Game server started!" << static_cast<u8>(network::SideID::RED) << std::endl;

    // Init ENet
    if (enet_initialize () != 0)
    {
        fprintf (stderr, "An error occurred while initializing ENet.\n");
        std::terminate();
    }
    std::atexit(enet_deinitialize);

    // Init server host
    init_server_host();

    while (true)
    {
        process_server_network_frame();
    }

    // Clean up
    // TODO: make sure it is called even when terminating
    enet_host_destroy(g_server_host);

    return 0;
}