#include "Network.hpp"

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/memory.hpp>
#include <sstream>

#include "CException.hpp"
#include "MatrixGame.h"
#include "MatrixLogic.hpp"
#include "Message.hpp"
#include "stupid_logger.hpp"

#include <enet/enet.h>

// logger_type cli_lgr{"client.log"};

Network g_Network{};

// namespace network
// {

// ENetHost* g_client_host;
//
// std::list<network::CommandsFrameRecord> commands_journal;


void Network::send_message(Message &msg)
{
    BitWriter writer;
    msg.serialize_to_bitstream(writer);

    ENetPacket* packet = enet_packet_create(
        writer.get_buffer(), writer.get_buffer_size(), ENET_PACKET_FLAG_RELIABLE
    );

    enet_peer_send(client_host->peers, 0, packet);

    enet_host_flush (client_host);
    // enet_packet_destroy(packet);
}

void Network::approve_final_input(u32 target_frame)
{
    // get_frame_record(target_frame)->set_side_inputs(controllable_side_id, current_input);
    auto inputs = get_frame_record(target_frame)->get_side_inputs(controllable_side_id);
    if (inputs == nullptr)
    {
        get_frame_record(target_frame)->set_side_inputs(controllable_side_id);
        inputs = get_frame_record(target_frame)->get_side_inputs(controllable_side_id);
    }

    Message msg = MessageCommandBatchParams{target_frame, controllable_side_id};
    msg.command_batch.commands = *inputs;
    send_message(msg);
    // current_input.clear();
    // get_frame_record(target_frame)->set_side_inputs(controllable_side_id, current_input);
    // Message msg = MessageCommandBatchParams{target_frame, controllable_side_id};
    // msg.command_batch.commands = current_input;
    // send_message(msg);
    // current_input.clear();
}

void Network::process_incoming_message(const Message &msg)
{
    if (msg.type == MessageType::COMMAND_BATCH)
    {
        std::cout << "Got command batch for " << msg.command_batch.target_frame << std::endl;
        get_frame_record(msg.command_batch.target_frame)->set_side_inputs(msg.command_batch.target_side, msg.command_batch.commands);
    }
    else if (msg.type == MessageType::START)
    {
        std::cout << "Game started officially." << std::endl;
        game_ongoing = true;
    }
    else if (msg.type == MessageType::DESYNC)
    {
        game_ongoing = false;
        lgr.error("DESYNC detected at frame: {}")(msg.checksum.target_frame);
        std::cerr << "DESYNC detected at frame: " << msg.checksum.target_frame << std::endl;
        std::cerr << "Desync" << std::endl;
    }
}

void Network::process_network_frame([[maybe_unused]] u32 delta_ns)
{
    static std::chrono::time_point<std::chrono::steady_clock>   prev_time = std::chrono::steady_clock::now();

    std::chrono::time_point<std::chrono::steady_clock>          curr_time = std::chrono::steady_clock::now();
    f64 delta = std::chrono::duration<f64>(curr_time - prev_time).count();
    prev_time = curr_time;

    if (game_ongoing && (physics_frame + INPUT_BUFFER_SIZE) > input_frame)
    {
        time_to_next_input -= delta;
        if (time_to_next_input <= 0)
        {
            approve_final_input(input_frame);
            const float INPUT_FRAME_DURATION = 0.013;
            time_to_next_input = INPUT_FRAME_DURATION;
            input_frame += 1;
        }
    }

    ENetEvent event;
    while (enet_host_service (client_host, &event, 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            lgr.info("A new client connected from %x:%u.\n")
                (event.peer->address.host, event.peer->address.port);


            /* Store any relevant client information here. */
            // event.peer->data = "Client information";

            break;

        case ENET_EVENT_TYPE_RECEIVE:
            // printf ("A packet of length %u containing %s was received from %s on channel %u.\n",
            //         event.packet -> dataLength,
            //         reinterpret_cast<const char *>(event.packet->data),
            //         static_cast<const char *>(event.peer->data),
            //         event.channelID);

            {
                BitReader reader = BitReader(event.packet->data);
                Message msg{Message::deserialize_from_bitstream(reader)};
                process_incoming_message(msg);

                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy(event.packet);
                break;

            }

        case ENET_EVENT_TYPE_DISCONNECT:
            printf ("%s disconnected.\n", static_cast<const char *>(event.peer->data));

            /* Reset the peer's client information. */

            event.peer -> data = NULL;
            break;
        }
    }
    for (int i = 0; i < client_host->peerCount; i++)
    {
        g_MatrixMap->m_DI.T(utils::format(L"Connected to %d", client_host->peers[i].connectID).c_str(), L"");
    }
}

void Network::init_client_host()
{
    client_host = enet_host_create (NULL /* create a client host */,
                1 /* only allow 1 outgoing connection */,
                2 /* allow up 2 channels to be used, 0 and 1 */,
                0 /* assume any amount of incoming bandwidth */,
                0 /* assume any amount of outgoing bandwidth */);

    if (client_host == NULL)
    {
        fprintf (stderr, "An error occurred while trying to create an ENet client host.\n");
        debugbreak();
    }
}

// TODO: actually call this function
void Network::deinit_client_host()
{
    enet_host_destroy(client_host);
}

void Network::connect_to_server()
{
    ENetAddress address;
    ENetEvent event;
    ENetPeer *peer;

    /* Connect to some.server.net:1234. */
    enet_address_set_host (& address, "127.0.0.1");
    address.port = 1234;


    // Sleep(10000);
    while (true)
    {
        /* Initiate the connection, allocating the two channels 0 and 1. */
        peer = enet_host_connect (client_host, &address, 2, 0);
        if (peer == NULL)
        {
            fprintf (stderr, "No available peers for initiating an ENet connection.\n");
            exit (EXIT_FAILURE);
        }

        if (enet_host_service (client_host, &event, 3000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
        {
            // g_MatrixMap->m_DI.T(L"Connected!", L"");
            // puts ("Connection to some.server.net:1234 succeeded.");
            std::cout << "Connected to the server!\n";
            break;
        }
        else
        {
            std::cout << "Could not connect to the server.\n";
            std::cout << "Attempting to reconnect...\n";
            enet_peer_reset(peer);
            continue;
            // std::terminate();
            /* Either the 5 seconds are up or a disconnect event was */
            /* received. Reset the peer in the event the 5 seconds   */
            /* had run out without any significant event.            */
            // enet_peer_reset (peer);
            //
            // puts ("Connection to some.server.net:1234 failed.");
        }
    }
}
void Network::static_init_networking()
{
    controllable_side_id = static_cast<u8>(isClient2 ? SideID::BLUE : SideID::RED);
    // controllable_side_id = static_cast<u8>(isClient2 ? SideID::BLUE : SideID::RED);
    commands_journal.push_back(CommandsFrameRecord(0));

    // Init ENet
    if (enet_initialize () != 0)
    {
        fprintf (stderr, "An error occurred while initializing ENet.\n");
        debugbreak(); // Todo: improve handing
    }
    std::atexit(enet_deinitialize);

    // Init ENet client host
    init_client_host();

    connect_to_server();
}

void Network::consume_input_frame(const u32 frame)
{
    CommandsFrameRecord* record = get_frame_record(frame);

    for (u32 side_id = 1; side_id < 5; side_id++)
    {
        if (record->is_side_input_ready(side_id))
        {
            std::vector<Command>* commands = record->get_side_inputs(side_id);
            for (u32 i = 0; i < commands->size(); ++i)
            {
                (*commands)[i].execute_for_side(side_id);
            }
        }
    }
}

void Network::save_commands_journal_to_file()
{
    std::ofstream fs("Client" + std::to_string(controllable_side_id) + "_commands.json");
    cereal::JSONOutputArchive oarchive(fs);
    oarchive(cereal::make_nvp("commands_journal", commands_journal));
}

void Network::add_input_for_current_input_frame(const Command &command)
{
    // if there is no vector for inputs yet - create it
    if (!this->get_frame_record(this->input_frame)->is_side_input_ready(this->controllable_side_id))
    {
        this->get_frame_record(this->input_frame)->set_side_inputs(this->controllable_side_id);
    }

    // Add command to the vector
    this->get_frame_record(this->input_frame)->get_side_inputs(this->controllable_side_id)->push_back(command);
}

void NetOrderMoveTo(const std::vector<u32> &entities_nid, const D3DXVECTOR3& destination)
{
    Command command { CommandMoveParams{entities_nid, destination} };
    g_Network.add_input_for_current_input_frame(command);
}

void NetOrderCapture(const std::vector<u32> &entities_nid, u32 target_nid)
{
    Command command { CommandCaptureParams{entities_nid, target_nid} };
    g_Network.add_input_for_current_input_frame(command);
}

void NetOrderAttack(const std::vector<u32> &entities_nid, u32 target_nid)
{
    Command command { CommandAttackParams{entities_nid, target_nid} };
    g_Network.add_input_for_current_input_frame(command);
}


// }

