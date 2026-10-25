#include "Network.hpp"

#include "3g.hpp"

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/memory.hpp>
#include <sstream>

#include "CException.hpp"
#include "MatrixGame.h"
#include "MatrixGameDll.hpp"
#include "MatrixLogic.hpp"
#include "Message.hpp"
#include "stupid_logger.hpp"

#include <enet/enet.h>
#include <algorithm>
#include <map>
#include "Interface/CConstructor.h"
#include "MatrixRobot.hpp"
#include "MatrixObjectBuilding.hpp"
#include "MatrixObjectCannon.hpp"
#include "Effects/MatrixEffectWeapon.hpp"
#include "MatrixSide.hpp"
#include "MatrixMapStatic.hpp"
#include "MatrixSoundManager.hpp"
// #include "SyncDebugger.hpp"
// logger_type cli_lgr{"client.log"};

namespace {

void apply_snapshot_animation(CMatrixRobotAI *r, EAnimation target_anim) {
    // Match authority LogicTakt (do_animation): call SwitchAnimation every tick, not only on
    // transitions. BEGINMOVE -> MOVE and ENDMOVE -> STAY need repeated SwitchAnimation calls
    // while the chassis clip plays (IsAnimEnd checks inside SwitchAnimation).
    if (target_anim == ANIMATION_BEGINMOVE || target_anim == ANIMATION_MOVE) {
        r->SwitchAnimation(ANIMATION_MOVE);
    } else if (target_anim == ANIMATION_BEGINMOVE_BACK || target_anim == ANIMATION_MOVE_BACK) {
        r->SwitchAnimation(ANIMATION_MOVE_BACK);
    } else if (target_anim == ANIMATION_ENDMOVE || target_anim == ANIMATION_ENDMOVE_BACK || target_anim == ANIMATION_STAY) {
        r->SwitchAnimation(ANIMATION_STAY);
    } else if (r->GetAnimation() != target_anim) {
        r->SwitchAnimation(target_anim);
    }
}

CMatrixBuilding *find_building_by_nid(u32 nid) {
    if (nid == 0)
        return nullptr;

    for (CMatrixMapStatic *obj = CMatrixMapStatic::GetFirstLogic(); obj; obj = obj->GetNextLogic()) {
        if (obj->IsLiveBuilding() && obj->m_NID == nid)
            return obj->AsBuilding();
    }
    return nullptr;
}

void apply_robot_spawn_sync(CMatrixRobotAI *r, const RobotSnapshot &rs) {
    const ERobotState new_state = static_cast<ERobotState>(rs.robot_state);
    if (r->m_CurrState != new_state) {
        r->m_CurrState = new_state;
        r->RChange(MR_Matrix);
    }

    if (rs.spawn_base_nid != 0) {
        if (CMatrixBuilding *base = find_building_by_nid(rs.spawn_base_nid))
            r->SetBase(base);
    } else if (new_state == ROBOT_SUCCESSFULLY_BUILD || new_state == ROBOT_DIP) {
        r->SetBase(nullptr);
    }

    if (new_state == ROBOT_IN_SPAWN || new_state == ROBOT_BASE_MOVEOUT)
        SETFLAG(r->m_ObjectState, ROBOT_FLAG_DISABLE_MANUAL);
}

void sync_base_building_from_snapshot(CMatrixBuilding *building, const BuildingSnapshot &b_from,
                                      const BuildingSnapshot &b_to, f32 k) {
    if (!building->IsBase())
        return;

    building->m_State = static_cast<EBaseState>(b_to.base_state);

    f32 floor_q = static_cast<f32>(b_to.base_floor_q);
    if (b_from.nid == b_to.nid)
        floor_q = LERPFLOAT(k, static_cast<f32>(b_from.base_floor_q), static_cast<f32>(b_to.base_floor_q));
    building->m_BaseFloor = floor_q / 255.f;

    if (b_to.spawning_bot)
        building->SetSpawningBot();
    else
        building->ResetSpawningBot();

    building->ApplyBaseFloorGraphics();
}

CMatrixCannon *find_live_cannon_by_nid(u32 nid) {
    for (CMatrixMapStatic *ms = CMatrixMapStatic::GetFirstLogic(); ms; ms = ms->GetNextLogic()) {
        if (ms->m_NID == nid && ms->IsCannon())
            return ms->AsCannon();
    }
    return nullptr;
}

bool turret_map_entry_is_live(CMatrixCannon *cannon) {
    return cannon != nullptr && find_live_cannon_by_nid(cannon->m_NID) == cannon;
}

void drop_stale_turret_map_entry(u32 nid) {
    if (!g_Network.turrets.contains(nid))
        return;
    CMatrixCannon *cannon = g_Network.turrets.at(nid);
    if (!turret_map_entry_is_live(cannon))
        g_Network.turrets.erase(nid);
}

void detach_cannon_from_all_build_stacks(CMatrixCannon *cannon) {
    for (CMatrixMapStatic *ms = CMatrixMapStatic::GetFirstLogic(); ms; ms = ms->GetNextLogic()) {
        if (ms->IsLiveBuilding())
            ms->AsBuilding()->m_BS.DetachItem(cannon);
    }
}

void stop_cannon_weapon_vfx(CMatrixCannon *cannon) {
    if (cannon == nullptr)
        return;
    for (int i = 0; i < cannon->m_WeaponCnt; ++i) {
        CMatrixEffectWeapon *weapon = cannon->m_Weapons[i];
        if (weapon == nullptr)
            continue;
        weapon->StopEffects();
        weapon->FireEnd();
    }
    cannon->EndFireAnimation();
}

void kill_turret_from_network_sync(CMatrixCannon *cannon) {
    if (cannon == nullptr || cannon->m_CurrState == CANNON_DIP)
        return;
    // ReleaseMe() -> DeleteItem(this) would StaticDelete while still inside Damage/ReleaseMe.
    if (g_Network.is_client())
        detach_cannon_from_all_build_stacks(cannon);
    stop_cannon_weapon_vfx(cannon);
    cannon->Damage(WEAPON_INSTANT_DEATH, D3DXVECTOR3(0, 0, 0), D3DXVECTOR3(0, 0, 0), 0, NULL);
}

void kill_client_turrets_on_building(CMatrixBuilding *building) {
    if (building == nullptr)
        return;

    std::vector<u32> turret_nids;
    turret_nids.reserve(g_Network.turrets.size());
    for (const auto &[nid, cannon] : g_Network.turrets) {
        if (cannon != nullptr && cannon->m_ParentBuilding == building)
            turret_nids.push_back(nid);
    }

    for (const u32 nid : turret_nids) {
        if (!g_Network.turrets.contains(nid))
            continue;
        CMatrixCannon *cannon = g_Network.turrets.at(nid);
        kill_turret_from_network_sync(cannon);
        drop_stale_turret_map_entry(nid);
    }
}

void purge_stale_turret_map_entries() {
    std::vector<u32> stale;
    stale.reserve(g_Network.turrets.size());
    for (const auto &[nid, cannon] : g_Network.turrets) {
        if (!turret_map_entry_is_live(cannon))
            stale.push_back(nid);
    }
    for (const u32 nid : stale)
        g_Network.turrets.erase(nid);
}

bool snapshot_turret_is_dead(const TurretSnapshot &ts) {
    const ECannonState state = static_cast<ECannonState>(ts.cannon_state);
    if (state == CANNON_DIP)
        return true;
    // Turrets being built report 0 HP until construction finishes.
    if (state == CANNON_UNDER_CONSTRUCTION)
        return false;
    return ts.health <= 0.f;
}

void apply_cannon_turret_yaw(CMatrixCannon *cannon, float yaw) {
    for (int i = 0; i < cannon->m_UnitCnt; ++i) {
        if (cannon->m_Unit[i].m_Type == CUT_TURRET || cannon->m_Unit[i].m_Type == CUT_SHAFT)
            cannon->m_Unit[i].u1.s1.m_Angle = yaw;
    }
}

void apply_turret_snapshot(CMatrixCannon *cannon, const TurretSnapshot &ts, f32 health_lerp, float base_angle,
                           float turret_yaw, float pitch) {
    cannon->m_Pos.x = ts.x;
    cannon->m_Pos.y = ts.y;
    cannon->SetSide(ts.side);
    cannon->m_CurrState = static_cast<ECannonState>(ts.cannon_state);
    cannon->m_Angle = base_angle;
    cannon->m_AngleX = pitch;
    apply_cannon_turret_yaw(cannon, turret_yaw);
    cannon->m_Place = ts.place;

    if (CMatrixBuilding *parent = find_building_by_nid(ts.parent_building_nid))
        cannon->m_ParentBuilding = parent;

    const float max_hp = ts.maxhealth * 10.f;
    const float hp = health_lerp * 10.f;
    if (hp < cannon->GetHitPoint())
        cannon->ShowHitpoint();
    cannon->ApplyNetworkHealth(hp, max_hp);

    cannon->RChange(MR_Matrix);
    cannon->RNeed(MR_Matrix);
}

void draw_client_turret_marker_at(float x, float y) {
    if (!g_Network.is_client())
        return;

    // Core matrix Z is rv/cnt inside RNeed. A missed terrain sample makes that NaN and the
    // helper line is dropped. The snapshot position is the turret slot either way.
    float z = g_MatrixMap->GetZ(x, y);
    if (z < -500.f)
        z = 0.f;

    // const D3DXVECTOR3 pos(x, y, z);
    // const D3DXVECTOR3 top = pos + D3DXVECTOR3(0, 0, 2000.f);

}

void draw_client_turret_marker(const CMatrixCannon *cannon) {
    if (cannon == nullptr)
        return;
    draw_client_turret_marker_at(cannon->m_Pos.x, cannon->m_Pos.y);
}

void sync_pneumatic_client_visuals(CMatrixRobotAI *r) {
    if (r->m_Unit[0].u1.s1.m_Kind != RUK_CHASSIS_PNEUMATIC)
        return;

    const EAnimation anim = r->GetAnimation();
    const bool moving_anim = anim == ANIMATION_MOVE || anim == ANIMATION_BEGINMOVE || anim == ANIMATION_MOVE_BACK ||
                             anim == ANIMATION_BEGINMOVE_BACK;
    if (!moving_anim)
        return;

    if (r->m_Speed <= 0.01f)
        r->m_Speed = 0.5f;

    if (!FLAG(r->m_ObjectState, ROBOT_FLAG_LINKED))
        r->FirstLinkPneumatic();
}

}  // namespace

Network g_Network{};
bool g_MatrixDebugOverlayVisible = false;

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
        writer.get_buffer(), writer.get_buffer_size(), 0
    );

    // Broadcast to all connected peers on Channel 0
    enet_host_broadcast(host, 0, packet);

    // Send the data immediately
    enet_host_flush(host);
}

void Network::send_message(Message &msg, ENetPeer *peer)
{
    if (peer == nullptr || host == nullptr)
        return;

    BitWriter writer;
    msg.serialize_to_bitstream(writer);

    ENetPacket *packet = enet_packet_create(
        writer.get_buffer(), writer.get_buffer_size(), ENET_PACKET_FLAG_RELIABLE);

    if (packet == nullptr)
        return;

    if (enet_peer_send(peer, 0, packet) < 0)
        enet_packet_destroy(packet);

    enet_host_flush(host);
}

void Network::register_side_peer(u8 side, ENetPeer *peer)
{
    if (peer == nullptr || side < 1 || side > 4)
        return;

    unregister_peer(peer);
    side_peers[side].push_back(peer);
}

void Network::unregister_peer(ENetPeer *peer)
{
    if (peer == nullptr)
        return;

    for (int side = 1; side <= 4; ++side)
    {
        std::vector<ENetPeer *> &peers = side_peers[side];
        peers.erase(std::remove(peers.begin(), peers.end(), peer), peers.end());
    }
}

void Network::emit_side_sound(u8 side, int sound)
{
    if (side == controllable_side_id)
        CSound::Play(static_cast<ESound>(sound), SL_ALL);

    if (!is_authority() || side < 1 || side > 4)
        return;

    const Event sound_event = Event::Sound(static_cast<u16>(sound), physics_frame, 1);
    Message msg{MessageEventsParams{std::vector<Event>{sound_event}}};

    for (ENetPeer *peer : side_peers[side])
    {
        if (peer == nullptr || peer->state != ENET_PEER_STATE_CONNECTED)
            continue;
        send_message(msg, peer);
    }
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
    if (interpolation_buffer.size() > 0 && ws.frame <= interpolation_buffer.back().frame)
    {
        std::cout << "Snapshot for frame: " << ws.frame << " is outdated. Dropping it." << std::endl;
        return;
    }
    interpolation_buffer.push(std::move(ws));
}

void Network::process_incoming_message(const Message &msg)
{
    if (msg.type == MessageType::EVENTS)
        MatrixGameDllBreadcrumbf("rx events count=%u", static_cast<unsigned>(msg.events.events.size()));
    else if (msg.type == MessageType::DESYNC)
        MatrixGameDllBreadcrumbf("rx desync frame=%u", msg.desync.target_frame);
    else if (msg.type == MessageType::START)
        MatrixGameDllBreadcrumb("rx start");
    else if (msg.type == MessageType::COMMAND_ATTACK || msg.type == MessageType::COMMAND_BUILD ||
             msg.type == MessageType::COMMAND_BUILD_TURRET || msg.type == MessageType::COMMAND_MOVE ||
             msg.type == MessageType::COMMAND_CAPTURE || msg.type == MessageType::COMMAND_STOP)
        MatrixGameDllBreadcrumbf("rx command type=%u", static_cast<unsigned>(msg.type));

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

        EventComparator compare;
        for (const Event &incoming : msg.events.events)
        {
            if (!events_queue.empty() && compare(events_queue.top(), incoming))
            {
                std::cout << "Dropping the event, because it is from the past" << std::endl;
            }
            else
            {
                events_queue.push(incoming);
            }
        }

        // std::cout << "Processing events: " << msg.events.events.size() << std::endl;

        // std::set<std::pair<u32, u8>> robot_fired{};
        //
        // for (EventFire event_fire : msg.events.events)
        // {
        //     if (robot_fired.contains(std::pair(event_fire.nid, event_fire.weapons)))
        //         continue;
        //
        //     if (robots.contains(event_fire.nid))
        //     {
        //         robot_fired.insert(std::pair(event_fire.nid, event_fire.weapons));
        //         CMatrixRobotAI *robot = robots.at(event_fire.nid);
        //
        //
        //         // std::cout << "Trigger fire for: " << robot->m_NID << " at " << event_fire.frame << std::endl;
        //         // Trigger all rocket launchers
        //         for (int i = 0; i < robot->m_WeaponsCnt; i++)
        //         {
        //             // static bool shot = false;
        //             if (robot->m_Weapons[i].GetWeaponType() == event_fire.weapons && robot->m_Weapons[i].m_Weapon)
        //             {
        //                 // shot = true;
        //
        //                 D3DXMATRIX m = (*robot->m_Weapons[i].m_Unit->m_Graph->GetMatrixById(1)) * robot->m_Weapons[i].m_Unit->m_Matrix;
        //                 D3DXVECTOR3 vPos;
        //                 D3DXVec3TransformCoord(&vPos, &vPos, &m);
        //                 robot->m_Weapons[i].m_Weapon->m_Pos = vPos;
        //
        //                 if (robot->m_Weapons[i].GetWeaponType() == WEAPON_BOMB)
        //                 {
        //                     robot->m_Weapons[i].m_Weapon->m_Dir = event_fire.target_pos;
        //                 }
        //                 else
        //                 {
        //                     D3DXVECTOR3 dir = event_fire.target_pos - robot->m_Weapons[i].m_Weapon->m_Pos;
        //                     D3DXVec3Normalize(&dir, &dir);
        //                     robot->m_Weapons[i].m_Weapon->m_Dir = dir;
        //                 }
        //
        //
        //                 robot->m_Weapons[i].m_Weapon->m_Skip = robot;
        //                 //robot->m_Weapons[i].m_On = true;
        //                 robot->m_Weapons[i].m_Weapon->Fire();
        //                 // std::cout << "Rocket launched. " << std::endl;
        //             }
        //         }
        //     }
        // }


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
    // std::cout << "Broadcasting events: " << msg.events.events.size() << std::endl;
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

void Network::delete_turret(CMatrixCannon *cannon)
{
    kill_turret_from_network_sync(cannon);
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
    constexpr auto dt = std::chrono::milliseconds(50);

    static std::chrono::steady_clock::time_point playback_time = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point last_update_time = playback_time;

    size_t buf_size = g_Network.interpolation_buffer.size();
    if (buf_size <= 2)
    {
        g_Network.playback_speed = 0.97f;
    }
    else if (buf_size == 3)
    {
        g_Network.playback_speed = 1.0f;
    }
    else if (buf_size == 4)
    {
        g_Network.playback_speed = 1.03f;
    }
    else if (buf_size == 5)
    {
        g_Network.playback_speed = 1.10f;
    }
    else
    {
        g_Network.playback_speed = 1.35f;
    }

    auto now = std::chrono::steady_clock::now();

    auto real_delta = now - last_update_time;
    last_update_time = now;
    if (real_delta > std::chrono::milliseconds(100))
    {
        real_delta = std::chrono::milliseconds(100);
    }
    auto modified_delta = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(real_delta) * playback_speed
    );

    playback_time += modified_delta;

    static u32 previous_frame = 0;
    static std::chrono::steady_clock::time_point previous_snapshot_time = playback_time;

    const WorldSnapshot *ws_from_ptr = nullptr;
    const WorldSnapshot *ws_to_ptr = nullptr;
    if (g_Network.interpolation_buffer.get_lerp_target_ptrs(ws_from_ptr, ws_to_ptr))
    {
        // Only resync the clock on initial startup or if the buffer overflowed and skipped frames
        if (previous_frame != ws_from_ptr->frame)
        {
            previous_frame = ws_from_ptr->frame;
            previous_snapshot_time = playback_time;
        }

        // Advance to the next snapshot pair ONLY if there is a 3rd snapshot waiting in the buffer!
        // Never pop when size() == 2, because that leaves size() == 1 with no ws_to to interpolate toward.
        while (playback_time - previous_snapshot_time >= dt && g_Network.interpolation_buffer.size() >= 3)
        {
            previous_snapshot_time += dt;
            g_Network.interpolation_buffer.pop_front();
            g_Network.interpolation_buffer.get_lerp_target_ptrs(ws_from_ptr, ws_to_ptr);
            previous_frame = ws_from_ptr->frame;
        }

        const WorldSnapshot ws_from = *ws_from_ptr;
        const WorldSnapshot ws_to = *ws_to_ptr;

        // If the buffer ran dry (still waiting for the next packet after dt), clamp playback_time
        // to the end of the current span so we don't accumulate debt and snap when it arrives.
        if (playback_time - previous_snapshot_time > dt)
        {
            playback_time = previous_snapshot_time + dt;
        }

        f32 k = std::clamp(
            std::chrono::duration<float>(playback_time - previous_snapshot_time).count() / std::chrono::duration<float>(dt).count(),
            0.0f,
            1.0f
        );

        u32 event_treshold_frame = ws_from.frame + std::ceil((ws_to.frame - ws_from.frame) * k);
        g_MatrixMap->m_DI.T(L"Interpolating:", utils::format(L"%d -> %d [%d] (%.2f)", ws_from.frame, ws_to.frame, event_treshold_frame, k).c_str(), 1000);
        g_MatrixMap->m_DI.T(L"Inter. Buffer:", utils::format(L"%2d/%d (speed %.2f)", g_Network.interpolation_buffer.size(), g_Network.interpolation_buffer.capacity(), g_Network.playback_speed).c_str(), 1000);

        // std::cout << "Interpolating: " << ws_from.frame << " -> " << ws_to.frame << "[" << event_treshold_frame << "] k=" << k << std::endl;
        // std::cout << "Inter. Buffer: " << g_Network.interpolation_buffer.size() << "/" << g_Network.interpolation_buffer.capacity() << " (speed = " << g_Network.playback_speed << ")" << std::endl << std::endl;


        for (auto& [id, side_ss] : ws_to.sides)
        {
            if (g_MatrixMap->GetSideById(id)->GetResourcesAmount(TITAN) < side_ss.titanium)
            {
                CMatrixMapStatic *ms = CMatrixMapStatic::GetFirstLogic();
                for (; ms; ms = ms->GetNextLogic()) {
                    if (ms->IsLiveBuilding() && ms->GetSide() == id)
                    {
                        SETFLAG(ms->m_ObjectState, BUILDING_NEW_INCOME);
                    }
                }
            }
            g_MatrixMap->GetSideById(id)->SetResourceAmount(TITAN, side_ss.titanium);
            g_MatrixMap->GetSideById(id)->SetResourceAmount(ELECTRONICS, side_ss.electronics);
            g_MatrixMap->GetSideById(id)->SetResourceAmount(ENERGY, side_ss.energy);
            g_MatrixMap->GetSideById(id)->SetResourceAmount(PLASMA, side_ss.plasma);
        }

        while (!events_queue.empty() && events_queue.top().frame <= event_treshold_frame)
        {
            Event e = events_queue.top();
            play_event(e);
            events_queue.pop();
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
                float ticks_in_span = std::max(1.0f, float(std::chrono::duration_cast<std::chrono::milliseconds>(dt).count()) / float(LOGIC_TAKT_PERIOD));
                r->m_Speed = D3DXVec2Length(&shift) / ticks_in_span;

                RobotSnapshot rs_spawn = rs_to;
                if (k < 0.5f)
                    rs_spawn.robot_state = rs_from.robot_state;
                apply_robot_spawn_sync(r, rs_spawn);

                const RobotSnapshot &rs_anim = k < 0.5f ? rs_from : rs_to;
                EAnimation target_anim = static_cast<EAnimation>(rs_anim.animation);
                apply_snapshot_animation(r, target_anim);
                sync_pneumatic_client_visuals(r);

                if (event_treshold_frame - r->last_shot_frame > 20)
                {

                    // SETFLAG(r->m_Flags, WEAPFLAGS_FIRE);
                    // r->LowLevelStopFire();
                    for (int nC = 0; nC < r->m_WeaponsCnt; nC++) {
                        if (r->m_Weapons[nC].IsEffectPresent()) {
                            r->m_Weapons[nC].m_Weapon->StopEffects();
                            r->m_Weapons[nC].m_Unit->m_Graph->SetAnimLooped(0);
                        }
                    }
                }
            }
        }

        purge_stale_turret_map_entries();

        for (auto& [id, ts] : ws_from.turrets) {
            if (!g_Network.turrets.contains(id)) {
                if (snapshot_turret_is_dead(ts))
                    continue;
                g_Network.populate_turret(ts);
            }
            if (!g_Network.turrets.contains(id))
                continue;

            CMatrixCannon *c = g_Network.turrets.at(id);
            if (!turret_map_entry_is_live(c)) {
                drop_stale_turret_map_entry(id);
                continue;
            }

            c->is_processed_by_network = true;

            if (ws_to.turrets.contains(id))
            {
                const TurretSnapshot ts_from = ws_from.turrets.at(id);
                const TurretSnapshot ts_to = ws_to.turrets.at(id);

                TurretSnapshot ts_apply = ts_to;
                ts_apply.x = LERPFLOAT(k, ts_from.x, ts_to.x);
                ts_apply.y = LERPFLOAT(k, ts_from.y, ts_to.y);
                if (k < 0.5f)
                    ts_apply.cannon_state = ts_from.cannon_state;

                const f32 health = LERPFLOAT(k, ts_from.health, ts_to.health);
                const float base_angle =
                    lerp_angle(k, unpack_rotation(ts_from.rotation), unpack_rotation(ts_to.rotation));
                const float turret_yaw =
                    lerp_angle(k, unpack_rotation(ts_from.turret_yaw), unpack_rotation(ts_to.turret_yaw));
                const float pitch = lerp_angle(k, unpack_rotation(ts_from.pitch), unpack_rotation(ts_to.pitch));

                if (snapshot_turret_is_dead(ts_to)) {
                    kill_turret_from_network_sync(c);
                    drop_stale_turret_map_entry(id);
                } else {
                    apply_turret_snapshot(c, ts_apply, health, base_angle, turret_yaw, pitch);
                    // Emit before erase_if. That pass can drop the map entry; the helper
                    // vertices are already stored by then.
                    //draw_client_turret_marker_at(ts_apply.x, ts_apply.y);

                    if (event_treshold_frame - c->last_shot_frame > 20)
                        stop_cannon_weapon_vfx(c);
                }
            } else {
                //draw_client_turret_marker(c);
            }
        }

        const int building_tick_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(modified_delta).count();

        CMatrixMapStatic *ms = CMatrixMapStatic::GetFirstLogic();
        while (ms)
        {
            CMatrixMapStatic *next = ms->GetNextLogic();
            if (ms->IsBuilding())
            {
                CMatrixBuilding *building = ms->AsBuilding();
                const u32 nid = ms->m_NID;

                if (ms->IsLiveBuilding())
                {
                    if (ws_to.buildings.contains(nid))
                    {
                        const BuildingSnapshot b_to = ws_to.buildings.at(nid);
                        building->m_Side = b_to.side;
                        building->m_TrueColor.m_ColoredCnt = b_to.capturing_progress;
                        building->m_TrueColor.m_Color = b_to.capturing_color;

                        if (b_to.health <= 0.f)
                        {
                            if (building->IsBase())
                                building->m_BS.SyncClientVisualFromNetwork(nullptr, 0, 0);
                            kill_client_turrets_on_building(building);
                            building->KillFromNetworkSync();
                        }
                        else
                        {
                            f32 health = b_to.health;
                            if (ws_from.buildings.contains(nid))
                            {
                                const BuildingSnapshot &b_from = ws_from.buildings.at(nid);
                                health = LERPFLOAT(k, b_from.health, b_to.health);
                            }

                            const float new_hp = health * 10.f;
                            if (new_hp < building->m_HitPoint)
                                building->ShowHitpoint();
                            building->m_HitPoint = new_hp;
                            if (building->m_HitPointMax > 0.f)
                                building->m_PB.Modify(building->m_HitPoint * building->m_MaxHitPointInversed);
                        }

                        if (ws_from.buildings.contains(nid))
                            sync_base_building_from_snapshot(building, ws_from.buildings.at(nid), b_to, k);
                        else
                            sync_base_building_from_snapshot(building, b_to, b_to, 1.f);

                        if (building->IsBase()) {
                            const BuildingSnapshot &stack_from =
                                ws_from.buildings.contains(nid) ? ws_from.buildings.at(nid) : b_to;
                            const BuildingSnapshot &stack_to = b_to;
                            const BuildingSnapshot &stack_view = k < 0.5f ? stack_from : stack_to;
                            int build_timer = stack_to.build_timer;
                            if (ws_from.buildings.contains(nid))
                                build_timer = static_cast<int>(
                                    LERPFLOAT(k, static_cast<f32>(stack_from.build_timer),
                                              static_cast<f32>(stack_to.build_timer)));
                            building->m_BS.SyncClientVisualFromNetwork(stack_view.build_stack,
                                                                       stack_view.build_stack_count, build_timer);
                        }
                    }
                    else if (ws_from.buildings.contains(nid) && ws_from.buildings.at(nid).health <= 0.f)
                    {
                        kill_client_turrets_on_building(building);
                        building->KillFromNetworkSync();
                    }
                }
                else if (building->m_State == BUILDING_DIP || building->m_State == BUILDING_DIP_EXPLODED)
                {
                    if (!ms->StaticTakt(building_tick_ms))
                    {
                        ms->RChange(MR_Matrix | MR_ShadowProjGeom | MR_ShadowProjTex | MR_ShadowStencil);
                        ms->RNeed(MR_Matrix);
                        ms->JoinToGroup();
                    }
                }
            }
            ms = next;
        }

        std::erase_if(robots, [this, modified_delta](auto& pair) {
            auto id = pair.first;
            CMatrixRobotAI *robot = pair.second;

            if (!robot->is_processed_by_network && robot->m_CurrState != ROBOT_DIP)
            {
                std::cout << "Robot " << id << " is missing from the snapshot, removing it.\n";
                delete_robot(robot);
            }
            robot->is_processed_by_network = false;

            bool deleted = static_cast<CMatrixMapStatic *>(robot)->StaticTakt(
                std::chrono::duration_cast<std::chrono::milliseconds>(modified_delta).count()
            );
            if (!deleted)
            {
                robot->RChange(MR_Matrix | MR_ShadowProjGeom | MR_ShadowProjTex | MR_ShadowStencil);
                robot->RNeed(MR_Matrix);
                robot->JoinToGroup();
            }
            return deleted;
        });

        std::erase_if(turrets, [this, modified_delta](auto& pair) {
            const u32 id = pair.first;
            CMatrixCannon *cannon = pair.second;

            if (!turret_map_entry_is_live(cannon))
                return true;

            if (!cannon->is_processed_by_network && cannon->m_CurrState != CANNON_DIP) {
                std::cout << "Turret " << id << " is missing from the snapshot, removing it.\n";
                delete_turret(cannon);
            }
            cannon->is_processed_by_network = false;

            const bool deleted = static_cast<CMatrixMapStatic *>(cannon)->StaticTakt(
                std::chrono::duration_cast<std::chrono::milliseconds>(modified_delta).count());
            if (!deleted) {
                cannon->RChange(MR_Matrix | MR_ShadowProjGeom | MR_ShadowProjTex | MR_ShadowStencil);
                cannon->RNeed(MR_Matrix);
                cannon->JoinToGroup();
            }
            return deleted;
        });

        for (int i = 0; i < g_MatrixMap->m_SideCnt; i++) {
            if (g_MatrixMap->m_Side[i].GetStatus() != SS_NONE) {
                g_MatrixMap->m_Side[i].RefreshRobotSupplyCount();
            }
        }
    }
}

u32 Network::allocate_nid()
{
    return next_nid++;
}

void Network::note_authoritative_nid(u32 nid)
{
    if (nid >= next_nid)
        next_nid = nid + 1;
}

void Network::assign_network_id(CMatrixMapStatic *obj, bool allow_on_client)
{
    if (obj == nullptr || obj->m_NID != 0)
        return;
    if (!(obj->IsBuilding() || obj->IsRobot() || obj->IsCannon()))
        return;
    const bool may_assign =
        is_authority() || network_mode == NetworkMode::NONE || allow_on_client;
    if (!may_assign)
        return;
    obj->m_NID = allocate_nid();
}

void Network::populate_turret(const TurretSnapshot &ts)
{
    if (turrets.contains(ts.nid))
        return;

    if (CMatrixCannon *existing = find_live_cannon_by_nid(ts.nid)) {
        if (existing->m_CurrState == CANNON_DIP)
            return;
        apply_turret_snapshot(existing, ts, ts.health, unpack_rotation(ts.rotation),
                              unpack_rotation(ts.turret_yaw), unpack_rotation(ts.pitch));
        existing->RNeed(MR_Matrix | MR_Graph);
        existing->JoinToGroup();
        turrets.emplace(ts.nid, existing);
        return;
    }

    CMatrixCannon *c = g_MatrixMap->StaticAdd<CMatrixCannon>(true);
    c->m_NID = ts.nid;
    note_authoritative_nid(ts.nid);

    c->UnitInit(ts.type);
    c->m_ShadowType = SHADOW_STENCIL;
    c->m_ShadowSize = 128;
    c->RNeed(MR_Matrix | MR_Graph);
    c->JoinToGroup();
    apply_turret_snapshot(c, ts, ts.health, unpack_rotation(ts.rotation), unpack_rotation(ts.turret_yaw),
                        unpack_rotation(ts.pitch));

    // if (c->m_CurrState == CANNON_UNDER_CONSTRUCTION && c->m_ParentBuilding != nullptr)
    //     c->m_ParentBuilding->m_BS.AddItem(c);

    turrets.emplace(ts.nid, c);
}

void Network::populate_robot(const RobotSnapshot &rs)
{
    // g_Network.
    // D3DXVECTOR3 pos = g_MatrixMap->m_TraceStopPos;

    SSpecialBot bot{};

    bot.m_Chassis.m_nKind = static_cast<ERobotUnitKind>(rs.chassis);
    bot.m_Armor.m_Unit.m_nKind = static_cast<ERobotUnitKind>(rs.hull);
    bot.m_Head.m_nKind = static_cast<ERobotUnitKind>(rs.head);

    for (int i = 0; i < rs.weapon_cnt; i++)
        bot.m_Weapon[i].m_Unit.m_nKind =  static_cast<ERobotUnitKind>(rs.weapons[rs.weapon_cnt - 1 - i]);

    // bot.m_Weapon[0].m_Unit.m_nKind = RUK_WEAPON_LASER;

    D3DXVECTOR3 pos = D3DXVECTOR3(rs.x, rs.y, 0);
    CMatrixRobotAI *r = bot.GetRobot(pos, rs.side);
    r->m_NID = rs.nid;
    note_authoritative_nid(rs.nid);

    g_MatrixMap->AddObject(r, true); // should we do this?

    r->JoinToGroup();
    r->CreateTextures();
    r->InitMaxHitpoint(10000.0);

    apply_robot_spawn_sync(r, rs);

    apply_snapshot_animation(r, static_cast<EAnimation>(rs.animation));
    sync_pneumatic_client_visuals(r);

    robots.emplace(rs.nid, r);
    MatrixGameDllBreadcrumbf(
        "spawn robot nid=%u side=%u weapons=%u", rs.nid, static_cast<unsigned>(rs.side),
        static_cast<unsigned>(rs.weapon_cnt));
}

void Network::add_event_to_current_tick(Event e)
{
    this_tick_event_pool.push_back(e);
}

void Network::clear_events_for_current_tick()
{
    this_tick_event_pool.clear();
}

void Network::play_event(const Event &event)
{
    switch (event.type)
    {
        case EventType::FIRE:  play_fire_event(event); break;
        case EventType::SOUND: play_sound_event(event); break;
        default: break;
    }
}

void Network::play_sound_event(const Event &event)
{
    const SoundEvent &sound = event.sound;
    const ESound snd = static_cast<ESound>(sound.sound);
    const ESoundLayer layer = static_cast<ESoundLayer>(sound.layer);
    if (sound.positional)
        CSound::Play(snd, sound.pos, layer);
    else
        CSound::Play(snd, layer);
}

void Network::play_fire_event(const Event &event)
{
    const EventFire &fire = event.fire;
    MatrixGameDllBreadcrumbf(
        "play fire nid=%u weapon=%u frame=%u", fire.nid, static_cast<unsigned>(fire.weapons), event.frame);

    if (robots.contains(fire.nid)) {
        CMatrixRobotAI *robot = robots.at(fire.nid);
        robot->last_shot_frame = event.frame;

        for (int i = 0; i < robot->m_WeaponsCnt; i++) {
            if (robot->m_Weapons[i].GetWeaponType() != fire.weapons || !robot->m_Weapons[i].m_Weapon)
                continue;

            const bool has_unit = robot->m_Weapons[i].m_Unit != nullptr;
            const bool has_graph = has_unit && robot->m_Weapons[i].m_Unit->m_Graph != nullptr;
            const bool has_matrix = has_graph && robot->m_Weapons[i].m_Unit->m_Graph->GetMatrixById(1) != nullptr;
            if (!has_matrix) {
                MatrixGameDllBreadcrumbf(
                    "fire robot nid=%u slot=%d missing unit=%d graph=%d matrix=%d", fire.nid, i,
                    static_cast<int>(has_unit), static_cast<int>(has_graph), static_cast<int>(has_matrix));
            }

            D3DXMATRIX m =
                (*robot->m_Weapons[i].m_Unit->m_Graph->GetMatrixById(1)) * robot->m_Weapons[i].m_Unit->m_Matrix;
            D3DXVECTOR3 vPos;
            D3DXVec3TransformCoord(&vPos, &vPos, &m);
            robot->m_Weapons[i].m_Weapon->m_Pos = vPos;

            if (robot->m_Weapons[i].GetWeaponType() == WEAPON_BOMB)
                robot->m_Weapons[i].m_Weapon->m_Dir = fire.target_pos;
            else {
                D3DXVECTOR3 dir = fire.target_pos - robot->m_Weapons[i].m_Weapon->m_Pos;
                D3DXVec3Normalize(&dir, &dir);
                robot->m_Weapons[i].m_Weapon->m_Dir = dir;
            }

            robot->m_Weapons[i].m_Weapon->m_Target = fire.target_pos;
            robot->m_Weapons[i].m_Weapon->m_Speed = fire.target_pos;
            robot->m_Weapons[i].m_Weapon->m_Skip = robot;
            robot->m_Weapons[i].m_Weapon->Fire();
        }
        return;
    }

    if (!turrets.contains(fire.nid))
        return;

    CMatrixCannon *cannon = turrets.at(fire.nid);
    if (!turret_map_entry_is_live(cannon)) {
        drop_stale_turret_map_entry(fire.nid);
        return;
    }

    cannon->last_shot_frame = event.frame;
    cannon->RNeed(MR_Matrix);

    for (int i = 0; i < cannon->m_WeaponCnt; ++i) {
        CMatrixEffectWeapon *weapon = cannon->m_Weapons[i];
        if (!weapon || weapon->GetWeaponType() != fire.weapons)
            continue;

        weapon->Modify(cannon->m_FireFrom[i], cannon->m_FireDir[i], D3DXVECTOR3(0, 0, 0));
        weapon->m_Pos = cannon->m_FireFrom[i];

        D3DXVECTOR3 dir = fire.target_pos - weapon->m_Pos;
        if (D3DXVec3LengthSq(&dir) > 1e-4f)
            D3DXVec3Normalize(&dir, &dir);
        else
            dir = cannon->m_FireDir[i];

        weapon->m_Dir = dir;
        weapon->m_Target = fire.target_pos;
        weapon->m_Speed = fire.target_pos;
        weapon->m_Skip = cannon;
        weapon->Fire();
        cannon->BeginFireAnimation();
    }
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

            {
                Message msg{ MessageGameSettingsParams( g_Network.lobby.get_selected_map_name() ) };
                g_Network.send_message(msg, event.peer);
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
        case ENET_EVENT_TYPE_RECEIVE: {
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
            else if (m.type == MessageType::COMMAND_CAPTURE)
            {
                m.capture.execute();
            }
            else if (m.type == MessageType::COMMAND_ATTACK)
            {
                m.attack.execute();
            }
            else if (m.type == MessageType::COMMAND_STOP)
            {
                m.command_stop.execute();
            }
            else if (m.type == MessageType::COMMAND_BUILD_TURRET)
            {
                m.command_build_turret.execute();
            }
            else if (m.type == MessageType::JOIN)
            {
                register_side_peer(m.join.player_side, event.peer);
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

        case ENET_EVENT_TYPE_DISCONNECT:
            unregister_peer(event.peer);
            event.peer->data = nullptr;
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

MessageGameSettingsParams Network::connect_to_server(std::string server_ip)
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

    // Short waits so WM_CLOSE is dispatched while no server is answering.
    int waited_ms = 0;
    while (true)
    {
        if (L3GProcessWindowMessages())
        {
            enet_peer_reset(peer);
            return MessageGameSettingsParams("");
        }

        if (enet_host_service (host, &event, 50) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
        {
            std::cout << "Connected to the server!\n";
            break;
        }

        waited_ms += 50;
        if (waited_ms >= 1000)
        {
            waited_ms = 0;
            std::cout << "Could not connect to the server.\n";
            std::cout << "Attempting to reconnect...\n";
        }
    }

    // Waiting for game settings
    while (true)
    {
        if (L3GProcessWindowMessages())
        {
            enet_peer_reset(peer);
            return MessageGameSettingsParams("");
        }

        if (enet_host_service (host, &event, 50) > 0 && event.type == ENET_EVENT_TYPE_RECEIVE)
        {
            BitReader reader = BitReader(event.packet->data);
            Message m { Message::deserialize_from_bitstream(reader) };

            if (m.type == MessageType::GAME_SETTINGS)
            {
                Message join{MessageJoinParams(controllable_side_id, std::string(lobby.input_username))};
                send_message(join, peer);
                return m.game_settings;
            }
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
        MatrixGameDllTrace("init_server_host: enet_host_create failed on port 1234");
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
    // if (is_client())
    // {
    //     init_client_host();
    //     connect_to_server();
    // }
    // else
    // {
    //     init_server_host();
    // }
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

void NetOrderCapture(const std::vector<u32> &entities_nid, u32 target_nid)
{
    Message command { MessageCommandCaptureParams {entities_nid, target_nid} };
    g_Network.send_message(command);
}

void NetOrderCapture(const std::vector<u32> &entities_nid, u32 target_nid, int local_side_id)
{
    Message command { MessageCommandCaptureParams {entities_nid, target_nid} };
    g_Network.send_message(command);

    command.capture.execute_for_side(local_side_id);
}

void NetOrderAttack(const std::vector<u32> &entities_nid, u32 target_nid)
{
    Message command { MessageCommandAttackParams {entities_nid, target_nid} };
    g_Network.send_message(command);
}

void NetOrderAttack(const std::vector<u32> &entities_nid, u32 target_nid, int local_side_id)
{
    Message command { MessageCommandAttackParams {entities_nid, target_nid} };
    g_Network.send_message(command);

    command.attack.execute_for_side(local_side_id);
}

void NetOrderAttack(const std::vector<u32> &entities_nid, u32 target_map_x, u32 target_map_y, int local_side_id)
{
    Message command { MessageCommandAttackParams {entities_nid, target_map_x, target_map_y, true} };
    g_Network.send_message(command);

    command.attack.execute_for_side(local_side_id);
}

void NetOrderStop(const std::vector<u32> &entities_nid, bool break_robot_orders)
{
    Message command { MessageCommandStopParams {entities_nid, break_robot_orders} };
    g_Network.send_message(command);
}

void NetOrderStop(const std::vector<u32> &entities_nid, int local_side_id, bool break_robot_orders)
{
    Message command { MessageCommandStopParams {entities_nid, break_robot_orders} };
    g_Network.send_message(command);

    command.command_stop.execute_for_side(local_side_id);
}

// void NetOrderCapture(const std::vector<u32> &entities_nid, u32 target_nid)
// {
//     Command command { CommandCaptureParams{entities_nid, target_nid} };
//     g_Network.add_input_for_current_input_frame(command);
// }
//
void NetOrderBuildTurret(u8 turret_type, u32 parent_building_nid, i32 place, f32 pos_x, f32 pos_y, u8 angle_packed)
{
    Message msg{MessageCommandBuildTurretParams(turret_type, parent_building_nid, place, pos_x, pos_y, angle_packed)};
    g_Network.send_message(msg);
}

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

