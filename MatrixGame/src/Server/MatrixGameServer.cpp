/**
 *  @file MatrixGameServer.cpp
 *
 *  @brief Contains entry point of matrix multiplayer lockstep relay server.
 *
 *  This file is only visible by MatrixGameServer CMake configuration.
 */

#include <iostream>

#include "MatrixGameServer.hpp"

#include <map>

#include "Network/Message.hpp"
#include "Network/Network.hpp"
#include "Types.hpp"
#include <enet/enet.h>

ENetHost* g_server_host;

std::map<enet_uint32, u8> peer_to_side{}; // TODO: does not work correctly
// MessageChecksumParams last_frame_checksums[4]{};
std::map<u32, u64> frame_checksums[4] {}; // physical frame to checksum for every side. Yellow=0

ServerState g_server_state = ServerState::BROADCASTING;

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

// Side_id: yellow here is 0, red is 1, blue is 2
void register_checksum(u8 side_id, MessageChecksumParams& checksum)
{
    frame_checksums[side_id].emplace(checksum.target_frame, checksum.checksum);
}

// Mutates the records by deleting them if they proven to be correct.
// Returns true if desync detected for requested frame.
bool check_checksums(u32 target_frame)
{
    std::cout << "Checking checksums for " << target_frame << std::endl;
    if (
        frame_checksums[0].find(target_frame)==frame_checksums[0].end() ||
        frame_checksums[1].find(target_frame)==frame_checksums[1].end()
    )
        return false;

    if (frame_checksums[0][target_frame] != frame_checksums[1][target_frame])
    {
        return true;
    }
    else
    {
        std::cout << "Releasing mem for " << target_frame << std::endl;
        // Release memory
        frame_checksums[0].erase(target_frame);
        frame_checksums[1].erase(target_frame);
        return false;
    }
}

void process_server_network_frame()
{
    ENetEvent event;
    while (enet_host_service (g_server_host, &event, 0) > 0)
    {
        std::cout << "\nNew event: " << event.type << "\n";
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            std::cout << "Connected: " << event.peer->connectID << "\n\n";

            std::cout << "The full list of connected peers: " << g_server_host->connectedPeers << "\n" ;

            for (int i = 0; i < g_server_host->connectedPeers; i++)
            {
                std::cout << g_server_host->peers[i].connectID << std::endl;
            }

            // Register the player
            peer_to_side.emplace(event.peer->connectID, g_server_host->connectedPeers - 1);

            // Initiate the starting of the game
            if (g_server_host->connectedPeers == 2)
            {
                Message msg { MessageType::START };

                BitWriter writer;
                msg.serialize_to_bitstream(writer);

                ENetPacket* packet = enet_packet_create(
                    writer.get_buffer(), writer.get_buffer_size(), ENET_PACKET_FLAG_RELIABLE
                );

                enet_host_broadcast(g_server_host, 0, packet);
                // enet_packet_destroy(packet);
            }

            break;
        case ENET_EVENT_TYPE_RECEIVE:
            std::cout << "Got new packet from " << event.peer->connectID << ":\n";
            if (g_server_host->connectedPeers < 2)
            {
                std::cerr << "Only one player...\n";
                std::terminate();
            }

            BitReader reader = BitReader(event.packet->data);
            Message m { Message::deserialize_from_bitstream(reader) };

            if (g_server_state == ServerState::BROADCASTING)
            {
                if (m.type == MessageType::COMMAND_BATCH)
                {
                    std::cout << "Got command batch for " << m.command_batch.target_frame << "\n";
                    // Find to which peer to retranslate the command_batch
                    ENetPeer* target = nullptr;
                    for (int i = 0; i < g_server_host->connectedPeers; i++)
                    {
                        if (g_server_host->peers[i].connectID != event.peer->connectID)
                        {
                            target = g_server_host->peers + i;
                            break;
                        }
                    }

                    if (target != nullptr)
                    {
                        std::cout << "Relaying to " << target->connectID << "\n\n";
                        ENetPacket* packet = enet_packet_create(nullptr, event.packet->dataLength, ENET_PACKET_FLAG_RELIABLE);
                        memcpy(packet->data, event.packet->data, event.packet->dataLength);
                        enet_peer_send(target, 0, packet);
                        enet_host_flush(g_server_host);
                        // enet_packet_destroy(packet);

                    }
                    else
                    {
                        std::cerr << "No target found.\n";
                    }
                }
                else if (m.type == MessageType::CHECKSUM)
                {
                    std::cout << "got checksum: " << m.checksum.checksum << ", for frame: " << m.checksum.target_frame << std::endl;
                    register_checksum(peer_to_side[event.peer->connectID], m.checksum);
                    bool is_desync = check_checksums(m.checksum.target_frame);

                    // Panic here
                    if (is_desync)
                    {
                        Message msg { MessageDesyncParams {m.checksum.target_frame} };
                        BitWriter writer;
                        msg.serialize_to_bitstream(writer);

                        ENetPacket* packet = enet_packet_create(
                            writer.get_buffer(), writer.get_buffer_size(), ENET_PACKET_FLAG_RELIABLE
                        );

                        // memcpy(packet->data, &msg, msg.get_serialized_size());
                        enet_host_broadcast(g_server_host, 0, packet);
                        enet_host_flush(g_server_host);
                        // enet_packet_destroy(packet);

                        std::cerr << "\nDesync detected at frame: " << m.checksum.target_frame << "\n";
                        // std::cerr << "Panicking!" << m.checksum.target_frame << "\n";
                        g_server_state = ServerState::DESYNC_HAPPENED;
                        std::cout << "\nDesync detected at frame: " << m.checksum.target_frame << "\n";
                        std::cout << "\nStopping broadcasting..." << "\n";
                        // std::terminate();
                    }
                }
            }
            else
            {


            }
                if (m.type == MessageType::STATE_REPORT)
                {
                    if (m.report.type == ReportType::DEFAULT)
                    {
                        std::string filename = "received_report_side" + std::to_string(m.report.player_side) + ".txt";
                        std::ofstream file(filename, std::ios::out | std::ios::trunc);
                        file << m.report.data;
                        std::cout << "Got world snapshot from side: " << std::to_string(m.report.player_side)
                                    << ", saving the data to the file: " << filename << "\n";
                    }
                    else
                    {
                        std::string filename = "code_trace_report_side" + std::to_string(m.report.player_side) + ".json";
                        std::ofstream file(filename, std::ios::out | std::ios::trunc);
                        file << m.report.data;
                        std::cout << "Got code trace report from side: " << std::to_string(m.report.player_side)
                                    << ", saving the data to the file: " << filename << "\n";
                    }
                }

            enet_packet_destroy(event.packet);
            break;
        }
    }

}

int main()
{
    std::cout << "Game server started!" << static_cast<u8>(SideID::RED) << std::endl;

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