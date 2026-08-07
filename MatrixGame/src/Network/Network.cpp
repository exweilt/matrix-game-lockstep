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
#include <algorithm>
#include <map>
#include "Interface/CConstructor.h"
// #include "SyncDebugger.hpp"
// logger_type cli_lgr{"client.log"};

Network g_Network{};

// namespace network
// {

// ENetHost* g_client_host;
//
// std::list<network::CommandsFrameRecord> commands_journal;

// void Network::send_message(Message &msg)
//
// {
//
//     BitWriter writer;
//
//     msg.serialize_to_bitstream(writer);
//
//
//     ENetPacket* packet = enet_packet_create(
//
//     writer.get_buffer(), writer.get_buffer_size(), ENET_PACKET_FLAG_RELIABLE
//
//     );
//
//
//     enet_peer_send(host->peers, 0, packet);
//
//
//     enet_host_flush (host);
//
//     // enet_packet_destroy(packet);
//
// }

void Network::send_message(Message &msg)
{
    BitWriter writer;
    msg.serialize_to_bitstream(writer);

    // Create the packet
    ENetPacket* packet = enet_packet_create(
        writer.get_buffer(), writer.get_buffer_size(), ENET_PACKET_FLAG_RELIABLE
    );

    // Broadcast to all connected peers on Channel 0
    enet_host_broadcast(host, 0, packet);

    // Send the data immediately
    enet_host_flush(host);
}

void Network::approve_final_input(u32 target_frame)
{
    // // get_frame_record(target_frame)->set_side_inputs(controllable_side_id, current_input);
    // auto inputs = get_frame_record(target_frame)->get_side_inputs(controllable_side_id);
    // if (inputs == nullptr)
    // {
    //     get_frame_record(target_frame)->set_side_inputs(controllable_side_id);
    //     inputs = get_frame_record(target_frame)->get_side_inputs(controllable_side_id);
    // }
    //
    // Message msg = MessageCommandBatchParams{target_frame, controllable_side_id};
    // msg.command_batch.commands = *inputs;
    // send_message(msg);
    // // current_input.clear();
    // // get_frame_record(target_frame)->set_side_inputs(controllable_side_id, current_input);
    // // Message msg = MessageCommandBatchParams{target_frame, controllable_side_id};
    // // msg.command_batch.commands = current_input;
    // // send_message(msg);
    // // current_input.clear();
}

void Network::handle_new_world_snapshot(WorldSnapshot ws)
{
    if (ws.frame <= interpolation_buffer.back().frame)
    {
        std::cout << "Snapshot for frame: " << ws.frame << " is outdated. Dropping it." << std::endl;
        return;
    }
    interpolation_buffer.push(ws);
}

void Network::process_incoming_message(const Message &msg)
{
    if (msg.type == MessageType::WORLD_SNAPSHOT)
    {
        handle_new_world_snapshot(msg.world_snapshot.ws);
        // std::cout << "Got Captured world snapshot: " << msg.world_snapshot.ws.to_json_string() << std::endl;
        // std::cout << "Got command batch for " << msg.command_batch.target_frame << std::endl;
        // get_frame_record(msg.command_batch.target_frame)->set_side_inputs(msg.command_batch.target_side, msg.command_batch.commands);
    }
    else if (msg.type == MessageType::START)
    {
        std::cout << "Game started officially." << std::endl;
        game_ongoing = true;
    }
    else if (msg.type == MessageType::DESYNC)
    {
        lgr.error("DESYNC detected at frame: {}")(msg.desync.target_frame);
        std::cerr << "DESYNC detected at frame: " << msg.desync.target_frame << std::endl;

        game_ongoing = false; // pause the game
        g_Network.desync_happened_at_frame = msg.desync.target_frame;  // log this
        g_Network.desync_happened = true;
    }
    else if (msg.type == MessageType::EVENTS)
    {
        if (msg.events.events.size() == 0) return;

        // std::cout << "Processing events: " << msg.events.events.size() << std::endl;

        std::set<std::pair<u32, u8>> robot_fired{};

        for (EventFire event_fire : msg.events.events)
        {
            if (robot_fired.contains(std::pair(event_fire.nid, event_fire.weapons)))
                continue;

            if (robots.contains(event_fire.nid))
            {
                robot_fired.insert(std::pair(event_fire.nid, event_fire.weapons));
                CMatrixRobotAI *robot = robots.at(event_fire.nid);


                // std::cout << "Trigger fire for: " << robot->m_NID << " at " << event_fire.frame << std::endl;
                // Trigger all rocket launchers
                for (int i = 0; i < robot->m_WeaponsCnt; i++)
                {
                    // static bool shot = false;
                    if (robot->m_Weapons[i].GetWeaponType() == event_fire.weapons && robot->m_Weapons[i].m_Weapon)
                    {
                        // shot = true;

                        D3DXMATRIX m = (*robot->m_Weapons[i].m_Unit->m_Graph->GetMatrixById(1)) * robot->m_Weapons[i].m_Unit->m_Matrix;
                        D3DXVECTOR3 vPos;
                        D3DXVec3TransformCoord(&vPos, &vPos, &m);
                        robot->m_Weapons[i].m_Weapon->m_Pos = vPos;

                        if (robot->m_Weapons[i].GetWeaponType() == WEAPON_BOMB)
                        {
                            robot->m_Weapons[i].m_Weapon->m_Dir = event_fire.target_pos;
                        }
                        else
                        {
                            D3DXVECTOR3 dir = event_fire.target_pos - robot->m_Weapons[i].m_Weapon->m_Pos;
                            D3DXVec3Normalize(&dir, &dir);
                            robot->m_Weapons[i].m_Weapon->m_Dir = dir;
                        }


                        robot->m_Weapons[i].m_Weapon->m_Skip = robot;
                        robot->m_Weapons[i].m_Weapon->Fire();
                        // std::cout << "Rocket launched. " << std::endl;
                    }
                }
            }
        }


        // for (int i = 0; i < msg.events.events.size(); i++)
        // {
        //     play_fire_event(msg.events.events[i]);
        // }
    }
}

void Network::broadcast_world_snapshot()
{
    WorldSnapshot ws = capture_world_snapshot();
    // std::cout << "Captured world snapshot: " << ws.to_json_string() << std::endl;

    Message msg
    {
        MessageWorldSnapshotParams {ws }
    };
    g_Network.send_message(msg);
}

void Network::broadcast_events()
{
    Message msg
    {
        MessageEventsParams { this_tick_event_pool }
    };
    std::cout << "Broadcasting events: " << msg.events.events.size() << std::endl;
    g_Network.send_message(msg);

    // if (msg.events.events.size() > 0)
    // {
    //     BitWriter writer;
    //     msg.serialize_to_bitstream(writer);
    //
    //     BitReader reader{writer.get_buffer()};
    //     Message readed = Message::deserialize_from_bitstream(reader);
    //     std::cout << "!!!:"  << std::endl;
    // }

    clear_events_for_current_tick();
}

void Network::delete_robot(CMatrixRobotAI *robot)
{
    robot->Damage(WEAPON_INSTANT_DEATH, D3DXVECTOR3(0, 0, 0), D3DXVECTOR3(0, 0, 0), 0, NULL);
    // robots.erase(robot->m_NID);
}

float lerp_angle(float t, float a, float b)
{
    constexpr float TWO_PI = 2.0f * M_PI;

    float delta = std::fmod(b - a, TWO_PI);

    if (delta > M_PI)
        delta -= TWO_PI;
    else if (delta < -M_PI)
        delta += TWO_PI;

    return a + delta * t;
}

void Network::process_playback([[maybe_unused]]int ms)
{
    // // static std::chrono::time_point<std::chrono::steady_clock>   tick_start_time = std::chrono::steady_clock::now();
    // std::chrono::time_point<std::chrono::steady_clock>          curr_time = std::chrono::steady_clock::now();
    // // static std::chrono::time_point<std::chrono::steady_clock>   prev_time = curr_time;
    //
    constexpr auto dt = std::chrono::milliseconds(102);
    // static auto next_tick = std::chrono::steady_clock::now() + dt;

    static std::chrono::steady_clock::time_point playback_time = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point last_update_time = playback_time;



    {
        if (g_Network.interpolation_buffer.size() < 2)
        {
            g_Network.playback_speed = 0.95f;
        }
        else if (g_Network.interpolation_buffer.size() >= 3)
        {
            g_Network.playback_speed = 1.4f;
        } else
            g_Network.playback_speed = 1.0f;
    }

    auto now = std::chrono::steady_clock::now();

    auto real_delta = now - last_update_time;
    last_update_time = now;
    auto modified_delta = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(real_delta) * playback_speed
    );

    playback_time += modified_delta;

    static u32 previous_frame = 0;
    static std::chrono::steady_clock::time_point previous_snapshot_time = std::chrono::steady_clock::now();



    // f64 tick_delta = std::chrono::duration<f64>(curr_time - tick_start_time).count() * g_Network.playback_speed;

    WorldSnapshot ws_from, ws_to;
    if (g_Network.interpolation_buffer.get_lerp_targets(ws_from, ws_to))
    {

        if (previous_frame != ws_from.frame)
        {
            previous_frame = ws_from.frame;
            previous_snapshot_time = playback_time;
        }

        f32 k = std::clamp(
        std::chrono::duration<float>(playback_time - previous_snapshot_time).count() / std::chrono::duration<float>(dt).count(),
            0.0f,
            1.0f
        );

        // std::cout << "Interpolating frames: " << ws_from.frame << " -> " << ws_to.frame << " k = " << k << std::endl;


        // const f32 k = 1.0f;
        // f32 k = std::min(1.0f, static_cast<float>(tick_delta) / 15.0f);
        u32 event_treshold_frame = ws_from.frame + std::ceil((ws_to.frame - ws_from.frame) * k);
        g_MatrixMap->m_DI.T(L"Interpolating:", utils::format(L"%d -> %d [%d] (%.2f)", ws_from.frame, ws_to.frame, event_treshold_frame, k).c_str(), 1000);
        g_MatrixMap->m_DI.T(L"Inter. Buffer:", utils::format(L"%2d/%d (speed %.2f)", g_Network.interpolation_buffer.size(), g_Network.interpolation_buffer.capacity(), g_Network.playback_speed).c_str(), 1000);


        for (auto& [id, side_ss] : ws_to.sides)
        {
            g_MatrixMap->GetSideById(id)->SetResourceAmount(TITAN, side_ss.titanium);
            g_MatrixMap->GetSideById(id)->SetResourceAmount(ELECTRONICS, side_ss.electronics);
            g_MatrixMap->GetSideById(id)->SetResourceAmount(ENERGY, side_ss.energy);
            g_MatrixMap->GetSideById(id)->SetResourceAmount(PLASMA, side_ss.plasma);
        }


        for (auto& [id, rs] : ws_from.robots) {
            if (!g_Network.robots.contains(id))
            {
                g_Network.populate_robot(rs);
            }
            robots.at(id)->is_processed_by_network = true;

            if (ws_to.robots.contains(id))
            {
                CMatrixRobotAI *r = g_Network.robots.at(id);
                RobotSnapshot rs_from = ws_from.robots.at(id);
                RobotSnapshot rs_to = ws_to.robots.at(id);
                r->m_PosX = LERPFLOAT(k, rs_from.x, rs_to.x);
                r->m_PosY = LERPFLOAT(k, rs_from.y, rs_to.y);
                r->SetRotationZ(lerp_angle(k, rs_from.rotation, rs_to.rotation));
                r->SetHullRotationZ(lerp_angle(k, rs_from.hull_rotation, rs_to.hull_rotation));
                r->InitMaxHitpoint(rs.maxhealth * 10);
                r->SetHitPoint(rs.health * 10);

                D3DXVECTOR2 shift = D3DXVECTOR2(rs_to.x - rs_from.x, rs_to.y - rs_from.y);
                r->m_Speed = D3DXVec2Length(&shift);

                if (r->GetAnimation() != static_cast<EAnimation>(rs.animation))
                    r->SwitchAnimation(static_cast<EAnimation>(rs.animation));

                // TODO: optimize this?
                r->RChange(MR_Matrix | MR_ShadowProjGeom | MR_ShadowProjTex | MR_ShadowStencil);
                r->RNeed(MR_Matrix);
                r->JoinToGroup();
            }
        }

        // kill unneeded
        // for (auto& [id, robot] : robots)
        // {
        //     if (!robot->is_processed_by_network)
        //     {
        //         std::cout << "Robot " << id << " is missing from the snapshot, removing it." << std::endl;
        //         std::cout << "snapshot: " << ws_from.to_json_string() << std::endl;
        //         delete_robot(robot);
        //     }
        //     else
        //     {
        //         robot->is_processed_by_network = false;
        //     }
        // }

        // int frame_delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - tick_start_time).count() * g_Network.playback_speed;
        // int delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>((curr_time - prev_time) * g_Network.playback_speed).count();
        std::erase_if(robots, [this, modified_delta](auto& pair) {
            auto id = pair.first;
            auto robot = pair.second;

            if (!robot->is_processed_by_network)
            {
                std::cout << "Robot " << id << " is missing from the snapshot, removing it.\n";
                delete_robot(robot);
            }
            robot->is_processed_by_network = false;


            return static_cast<CMatrixMapStatic *>(robot)->StaticTakt(
                std::chrono::duration_cast<std::chrono::milliseconds>(modified_delta).count()
            );
        });

        if (k >= 1.0f)
        {
            g_Network.interpolation_buffer.pop_front();
            // tick_start_time = std::chrono::steady_clock::now();
            // next_tick += dt;
        }
    }
    else
    {
        // tick_start_time = std::chrono::steady_clock::now();
    }
    // auto delta = (std::chrono::steady_clock::now() - curr_time) * g_Network.playback_speed;
    // curr_time += delta;

    // auto delta = std::chrono::steady_clock::now() - curr_time;

    // auto scaled_delta = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
    //     std::chrono::duration<double>(delta) * g_Network.playback_speed * 20.0f
    // );

    // curr_time += scaled_delta;

    // prev_time = curr_time;
}

void Network::populate_robot(RobotSnapshot& rs)
{
    // g_Network.
    // D3DXVECTOR3 pos = g_MatrixMap->m_TraceStopPos;

    SSpecialBot bot{};

    bot.m_Chassis.m_nKind = static_cast<ERobotUnitKind>(rs.chassis);
    bot.m_Armor.m_Unit.m_nKind = static_cast<ERobotUnitKind>(rs.hull);
    bot.m_Head.m_nKind = static_cast<ERobotUnitKind>(rs.head);

    for (int i = 0; i < rs.weapon_cnt; i++)
        bot.m_Weapon[i].m_Unit.m_nKind =  static_cast<ERobotUnitKind>(rs.weapons[i]);

    // bot.m_Weapon[0].m_Unit.m_nKind = RUK_WEAPON_LASER;

    D3DXVECTOR3 pos = D3DXVECTOR3(rs.x, rs.y, 0);
    CMatrixRobotAI *r = bot.GetRobot(pos, rs.side);
    r->m_NID = rs.nid;

    // g_MatrixMap->AddObject(r, true);

    r->JoinToGroup();
    r->CreateTextures();
    r->InitMaxHitpoint(10000.0);

    robots.emplace(rs.nid, r);
}

void Network::add_event_to_current_tick(EventFire e)
{
    this_tick_event_pool.push_back(e);
}

void Network::clear_events_for_current_tick()
{
    this_tick_event_pool.clear();
}

void Network::play_fire_event(const EventFire &event)
{

}

void Network::initialize_replay_mode_with_files(std::wstring commands_filename)
{
}

void Network::initialize_replay_mode_with_files(std::wstring commands_filename, std::wstring checksums_filename)
{
}

void Network::process_network_frame([[maybe_unused]] u32 delta_ns)
{
    static std::chrono::time_point<std::chrono::steady_clock>   prev_time = std::chrono::steady_clock::now();

    std::chrono::time_point<std::chrono::steady_clock>          curr_time = std::chrono::steady_clock::now();
    f64 delta = std::chrono::duration<f64>(curr_time - prev_time).count();
    prev_time = curr_time;

    // if (game_ongoing && (physics_frame + INPUT_BUFFER_SIZE) > input_frame)
    // {
    //     time_to_next_input -= delta;
    //     if (time_to_next_input <= 0)
    //     {
    //         approve_final_input(input_frame);
    //         const float INPUT_FRAME_DURATION = 0.013;
    //         time_to_next_input = INPUT_FRAME_DURATION;
    //         input_frame += 1;
    //     }
    // }
    if (is_client())
    {
        ENetEvent event;
        while (enet_host_service (host, &event, 0) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                    lgr.info("A new thing connected from %x:%u.\n")
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
                    // std::cout << "new data " << std::endl;
                    BitReader reader = BitReader(event.packet->data);
                    Message msg{Message::deserialize_from_bitstream(reader)};
                    process_incoming_message(msg);
                    //
                    // /* Clean up the packet now that we're done using it. */
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
    }
    else
    {
        process_server_network_frame();
    }
    // for (int i = 0; i < host->peerCount; i++)
    // {
    //     g_MatrixMap->m_DI.T(utils::format(L"Connected to %d", host->peers[i].connectID).c_str(), L"");
    // }
    for (size_t i = 0; i < host->peerCount; i++)
    {
        ENetPeer* currentPeer = &host->peers[i];

        // SKIP empty slots, disconnected peers, or peers currently in the handshake phase
        if (currentPeer->state != ENET_PEER_STATE_CONNECTED)
        {
            continue;
        }

        g_MatrixMap->m_DI.T(utils::format(L"Connected to %d", currentPeer->connectID).c_str(), L"");
    }
}

void Network::process_server_network_frame()
{
    ENetEvent event;
    while (enet_host_service (host, &event, 0) > 0)
    {
        std::cout << "\nNew event: " << event.type << "\n";
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            std::cout << "Connected: " << event.peer->connectID << "\n\n";

            std::cout << "The full list of connected peers: " << host->connectedPeers << "\n" ;

            for (int i = 0; i < host->connectedPeers; i++)
            {
                std::cout << host->peers[i].connectID << std::endl;
            }

            // Register the player
            // peer_to_side.emplace(event.peer->connectID, g_server_host->connectedPeers - 1);

            // Initiate the starting of the game
            // if (g_server_host->connectedPeers == 2)
            // {
            //     Message msg { MessageType::START };
            //
            //     BitWriter writer;
            //     msg.serialize_to_bitstream(writer);
            //
            //     ENetPacket* packet = enet_packet_create(
            //         writer.get_buffer(), writer.get_buffer_size(), ENET_PACKET_FLAG_RELIABLE
            //     );
            //
            //     enet_host_broadcast(g_server_host, 0, packet);
            //     // enet_packet_destroy(packet);
            // }

            break;
        case ENET_EVENT_TYPE_RECEIVE:
            std::cout << "Got new packet from " << event.peer->connectID << ":\n";
            // if (g_server_host->connectedPeers < 2)
            // {
            //     std::cerr << "Only one player...\n";
            //     std::terminate();
            // }

            BitReader reader = BitReader(event.packet->data);
            Message m { Message::deserialize_from_bitstream(reader) };

            if (m.type == MessageType::COMMAND_MOVE)
            {
                if (m.command_move.number_of_robots > 0)
                {
                    m.command_move.execute();
                }
            }
            else if (m.type == MessageType::COMMAND_BUILD)
            {
                m.command_build.execute_for_side();
            }

            //
            // if (g_server_state == ServerState::BROADCASTING)
            // {
            //     if (m.type == MessageType::COMMAND_BATCH)
            //     {
            //         std::cout << "Got command batch for " << m.command_batch.target_frame << "\n";
            //         // Find to which peer to retranslate the command_batch
            //         ENetPeer* target = nullptr;
            //         for (int i = 0; i < g_server_host->connectedPeers; i++)
            //         {
            //             if (g_server_host->peers[i].connectID != event.peer->connectID)
            //             {
            //                 target = g_server_host->peers + i;
            //                 break;
            //             }
            //         }
            //
            //         if (target != nullptr)
            //         {
            //             std::cout << "Relaying to " << target->connectID << "\n\n";
            //             ENetPacket* packet = enet_packet_create(nullptr, event.packet->dataLength, ENET_PACKET_FLAG_RELIABLE);
            //             memcpy(packet->data, event.packet->data, event.packet->dataLength);
            //             enet_peer_send(target, 0, packet);
            //             enet_host_flush(g_server_host);
            //             // enet_packet_destroy(packet);
            //
            //         }
            //         else
            //         {
            //             std::cerr << "No target found.\n";
            //         }
            //     }
            //     else if (m.type == MessageType::CHECKSUM)
            //     {
            //         std::cout << "got checksum: " << m.checksum.checksum << ", for frame: " << m.checksum.target_frame << std::endl;
            //         register_checksum(peer_to_side[event.peer->connectID], m.checksum);
            //         bool is_desync = check_checksums(m.checksum.target_frame);
            //
            //         // Panic here
            //         if (is_desync)
            //         {
            //             Message msg { MessageDesyncParams {m.checksum.target_frame} };
            //             BitWriter writer;
            //             msg.serialize_to_bitstream(writer);
            //
            //             ENetPacket* packet = enet_packet_create(
            //                 writer.get_buffer(), writer.get_buffer_size(), ENET_PACKET_FLAG_RELIABLE
            //             );
            //
            //             // memcpy(packet->data, &msg, msg.get_serialized_size());
            //             enet_host_broadcast(g_server_host, 0, packet);
            //             enet_host_flush(g_server_host);
            //             // enet_packet_destroy(packet);
            //
            //             std::cerr << "\nDesync detected at frame: " << m.checksum.target_frame << "\n";
            //             // std::cerr << "Panicking!" << m.checksum.target_frame << "\n";
            //             g_server_state = ServerState::DESYNC_HAPPENED;
            //             std::cout << "\nDesync detected at frame: " << m.checksum.target_frame << "\n";
            //             std::cout << "\nStopping broadcasting..." << "\n";
            //             // std::terminate();
            //         }
            //     }
            // }
            // else
            // {
            //
            //
            // }
            //     if (m.type == MessageType::STATE_REPORT)
            //     {
            //         if (m.report.type == ReportType::DEFAULT)
            //         {
            //             std::string filename = "received_report_side" + std::to_string(m.report.player_side) + ".txt";
            //             std::ofstream file(filename, std::ios::out | std::ios::trunc);
            //             file << m.report.data;
            //             std::cout << "Got world snapshot from side: " << std::to_string(m.report.player_side)
            //                         << ", saving the data to the file: " << filename << "\n";
            //         }
            //         else
            //         {
            //             std::string filename = "code_trace_report_side" + std::to_string(m.report.player_side) + ".json";
            //             std::ofstream file(filename, std::ios::out | std::ios::trunc);
            //             file << m.report.data;
            //             std::cout << "Got code trace report from side: " << std::to_string(m.report.player_side)
            //                         << ", saving the data to the file: " << filename << "\n";
            //         }
            //     }

            enet_packet_destroy(event.packet);
            break;
        }
    }
}

void Network::init_client_host()
{
    host = enet_host_create (NULL /* create a client host */,
                1 /* only allow 1 outgoing connection */,
                2 /* allow up 2 channels to be used, 0 and 1 */,
                0 /* assume any amount of incoming bandwidth */,
                0 /* assume any amount of outgoing bandwidth */);

    if (host == NULL)
    {
        fprintf (stderr, "An error occurred while trying to create an ENet client host.\n");
        debugbreak();
    }
}

// TODO: actually call this function
void Network::deinit_client_host()
{
    enet_host_destroy(host);
}

void Network::connect_to_server()
{
    ENetAddress address;
    ENetEvent event;
    ENetPeer *peer;

    /* Connect to some.server.net:1234. */
    enet_address_set_host (& address, server_ip.c_str());
    address.port = 1234;

    /* Initiate the connection, allocating the two channels 0 and 1. */
    peer = enet_host_connect (host, &address, 2, 0);
    if (peer == NULL)
    {
        fprintf (stderr, "No available peers for initiating an ENet connection.\n");
        exit (EXIT_FAILURE);
    }

    while (true)
    {

        if (enet_host_service (host, &event, 1000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
        {
            std::cout << "Connected to the server!\n";
            break;
        }
        else
        {
            std::cout << "Could not connect to the server.\n";
            // exit(1);
            std::cout << "Attempting to reconnect...\n";
            Sleep(1000);
            // enet_peer_reset(peer);
            continue;
        }
    }
}

void Network::init_server_host()
{
    ENetAddress address;
    /* enet_address_set_host (& address, "x.x.x.x"); */

    address.host = ENET_HOST_ANY;
    address.port = 1234;

    host = enet_host_create (& address /* the address to bind the server host to */,
                                 32      /* allow up to 32 clients and/or outgoing connections */,
                                  2      /* allow up to 2 channels to be used, 0 and 1 */,
                                  0      /* assume any amount of incoming bandwidth */,
                                  0      /* assume any amount of outgoing bandwidth */);

    if (host == nullptr)
    {
        fprintf (stderr, "An error occurred while trying to create an ENet server host.\n");
        std::terminate();
    }
}

void Network::static_init_networking()
{
   // history_game_states.set_next_frame(1); // skip 0
    //g_SyncLogs.set_next_frame(2);

    // controllable_side_id = static_cast<u8>(isClient2 ? SideID::BLUE : SideID::RED);
    // controllable_side_id = static_cast<u8>(isClient2 ? SideID::BLUE : SideID::RED);
    //commands_journal.push_back(CommandsFrameRecord(0));

    // Init ENet
    if (enet_initialize() != 0)
    {
        fprintf (stderr, "An error occurred while initializing ENet.\n");
        debugbreak(); // Todo: improve handing
    }
    std::atexit(enet_deinitialize);

    // Init ENet client host
    if (is_client())
    {
        init_client_host();
        connect_to_server();
    }
    else
    {
        init_server_host();
    }
}

// void Network::consume_input_frame(const u32 frame)
// {
//     CommandsFrameRecord* record = get_frame_record(frame);
//
//     for (u32 side_id = 1; side_id < 5; side_id++)
//     {
//         if (record->is_side_input_ready(side_id))
//         {
//             std::vector<Command>* commands = record->get_side_inputs(side_id);
//             for (u32 i = 0; i < commands->size(); ++i)
//             {
//                 (*commands)[i].execute_for_side(side_id);
//             }
//         }
//     }
// }

// void Network::save_commands_journal_to_file()
// {
//     std::ofstream fs(isClient2 ? "Client2_commands.json" : "Client1_commands.json");
//     cereal::JSONOutputArchive oarchive(fs);
//     oarchive(cereal::make_nvp("commands_journal", commands_journal));
// }
//
// std::string Network::commands_journal_to_json_string()
// {
//     std::ostringstream ss;
//
//     {
//         cereal::JSONOutputArchive oarchive(ss);
//         oarchive(cereal::make_nvp("commands_journal", commands_journal));
//     }
//
//     return ss.str();
// }
//
// void Network::add_input_for_current_input_frame(const Command &command)
// {
//     // if there is no vector for inputs yet - create it
//     if (!this->get_frame_record(this->input_frame)->is_side_input_ready(this->controllable_side_id))
//     {
//         this->get_frame_record(this->input_frame)->set_side_inputs(this->controllable_side_id);
//     }
//
//     // Add command to the vector
//     this->get_frame_record(this->input_frame)->get_side_inputs(this->controllable_side_id)->push_back(command);
// }

void NetOrderMoveTo(const std::vector<u32> &entities_nid, const D3DXVECTOR3& destination)
{
    Message command { MessageCommandMoveParams {entities_nid, destination} };
    g_Network.send_message(command);
}

void NetOrderMoveTo(const std::vector<u32> &entities_nid, const D3DXVECTOR3& destination, int local_side_id)
{
    Message command { MessageCommandMoveParams {entities_nid, destination} };
    g_Network.send_message(command);

    command.command_move.execute_for_side(local_side_id);
}

// void NetOrderCapture(const std::vector<u32> &entities_nid, u32 target_nid)
// {
//     Command command { CommandCaptureParams{entities_nid, target_nid} };
//     g_Network.add_input_for_current_input_frame(command);
// }
//
// void NetOrderAttack(const std::vector<u32> &entities_nid, u32 target_nid)
// {
//     Command command { CommandAttackParams{entities_nid, target_nid} };
//     g_Network.add_input_for_current_input_frame(command);
// }
//
// void NetOrderConstruct(ERobotUnitKind chassis, ERobotUnitKind hull, ERobotUnitKind head,
//                         const std::vector<ERobotUnitKind> &weapons, u8 robot_count, u32 base)
// {
//     DTRACE();
//     assert(weapons.size() == MAX_WEAPON_CNT);
//     // ERobotUnitKind *weapons_data = weapons.data();
//     Command command { CommandBuildParams {chassis, hull, head, weapons, robot_count, base} };
//     // for (i32 i = 0; i < MAX_WEAPON_CNT; i++)
//     // {
//     //     command.build.weapons[i] = weapons[i];
//     // }
//     DCP();
//     g_Network.add_input_for_current_input_frame(command);
// }


// }

