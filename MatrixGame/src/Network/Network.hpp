#pragma once

#include "Message.hpp"
#include "Types.hpp"
#include "RingBuffer.hpp"
#include "Snapshot.hpp"
#include "stupid_logger.hpp"

#include <enet/enet.h>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/vector.hpp>

#include <atomic>
#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

#include "Lobby.hpp"

constexpr u32 INPUT_BUFFER_SIZE = 15; // Size of input buffering
constexpr u32 PHYSICS_FRAME_RATE = 10;
static_assert(PHYSICS_FRAME_RATE >= 1 && PHYSICS_FRAME_RATE < 200);
constexpr u32 PHYSICS_FRAME_PERIOD_MS = static_cast<u32>(1000.0 / PHYSICS_FRAME_RATE) + 1;
constexpr u32 INTERPOLATION_BUFFER_SIZE = 8;


enum class SideID : u8
{
    YELLOW = 1,
    RED = 2,
    BLUE = 3,
    GREEN = 4,
};

// struct CommandsFrameRecord
// {
//     u32 frame; // The commands record for this physics frame (tick).
//
//     /**
//      * @brief Vectors of commands for each side (0 is yellow) for this frame.
//      *
//      * If s[i] == nullptr then the commands are not present for that side and frame.
//      */
//     std::unique_ptr< std::vector<Command> > commands [4];
//
//     CommandsFrameRecord(const u32 f = 0) : frame(f) {}
//
//     bool is_side_input_ready(const u32 side_id) const
//     {
//         return commands[side_id - 1].get() != nullptr;
//     }
//
//     std::vector<Command>* get_side_inputs(const u32 side_id) const
//     {
//         return commands[side_id - 1].get();
//     }
//
//     void set_side_inputs(const u32 side_id, const std::vector<Command>& side_inputs)
//     {
//         commands[side_id - 1] = std::make_unique<std::vector<Command>>(side_inputs);
//     }
//
//     void set_side_inputs(const u32 side_id)
//     {
//         commands[side_id - 1] = std::make_unique<std::vector<Command>>();
//     }
//
//     template <class Archive>
//     void serialize(Archive& ar) {
//         ar(
//             CEREAL_NVP(frame)
//         );
//         for (int i = 0; i < 4; i++) {
//             ar( cereal::make_nvp("commands_" + std::to_string(i + 1), commands[i]) );
//         }
//     }
// };

// extern std::list<CommandsFrameRecord> commands_journal;

enum class NetworkMode : u8
{
    NONE = 0,
    SERVER = 1,
    CLIENT = 2,
    SINGLEPLAYER = 3
};

enum class GameState : u8
{
    LOBBY = 0,
    GAME = 1,
};

class Network
{
public:
    Network()  = default;
    ~Network();

    void broadcast_world_snapshot();
    void broadcast_events();
    /** Play for the local side, and on the host send the stinger only to peers registered for @p side. */
    void emit_side_sound(u8 side, int sound);
    /** Positional one-shot. The host plays it immediately; clients receive it on the event clock. */
    void emit_world_sound(int sound, const D3DXVECTOR3 &pos);
    void register_side_peer(u8 side, ENetPeer *peer);
    void unregister_peer(ENetPeer *peer);
    /** Host or the client's own provisional row. Clears any previous roster. */
    void set_local_player(u8 side, const std::string &name);
    /** Server: remember a joined peer and send the roster to every client. */
    void note_joined_player(ENetPeer *peer, u8 side, const std::string &name);
    /** Server: drop a disconnected peer and send the roster again. */
    void drop_player(ENetPeer *peer);
    /** Client: replace the roster with the one the server just sent. */
    void apply_player_list(const std::vector<PlayerListEntry> &entries);

    void delete_robot(CMatrixRobotAI * robot);

    void process_playback(int ms);
    void populate_robot(const RobotSnapshot &rs);
    void populate_turret(const TurretSnapshot &ts);
    void add_event_to_current_tick(Event e);
    void clear_events_for_current_tick();
    void play_event(const Event &event);
    void play_fire_event(const Event &event);
    void play_sound_event(const Event &event);

    ENetHost* host = nullptr;

    GameState game_state;
    NetworkMode network_mode = NetworkMode::NONE;
    u8 controllable_side_id     = static_cast<u8>(SideID::YELLOW); // SideID
    u32 graphics_frame        = 0; // current rendering frame
    u32 physics_frame         = 0; // current physics frame
    // u32 input_frame           = 0; // new inputs are sampled for this physics frame
    // u32 total_ms              = 0;
    bool isClient2              = false;
    std::string server_ip;
    bool flag_server_skip_lobby = false;
    int flag_client_skip_lobby = 0;
    f64 time_to_next_input    = 0.017; // time in seconds until switching input_frame
    bool game_ongoing           = true;
    u32 desync_happened_at_frame = 0;
    bool desync_happened = false;
    bool has_physics_frame_run = false;
    u32 code_logic_frame = 1;
    std::vector<Event> this_tick_event_pool;
    /** Remote players, keyed by side id (1..4). The host's own side is not in this list. */
    std::vector<ENetPeer *> side_peers[5];
    /**
     * Joined clients. On the server, peer is the ENet peer and id is its connectID.
     * On a client every peer is null; id is the connectID the server sent.
     */
    struct ConnectedPlayer
    {
        ENetPeer *peer = nullptr;
        u8 side = 0;
        u32 id = 0;
        std::string name;
        u16 ping_ms = kPlayerPingUnknown;
        u32 ping_token = 0;
        std::chrono::steady_clock::time_point ping_sent{};
        std::chrono::steady_clock::time_point last_pong{};
        bool ping_waiting = false;
        bool got_pong = false;
    };
    std::vector<ConnectedPlayer> player_list;
    /** This client's ENet connectID. Zero on the host and before the handshake. */
    u32 local_connect_id = 0;
    /** Server peer on a client. Used to echo ping probes. */
    ENetPeer *server_peer = nullptr;
    std::chrono::steady_clock::time_point last_ping_poll{};
    u32 next_ping_token = 1;
    bool player_list_dirty = false;

    /**
     * Socket events pulled by the pump thread. Ping replies are sent there, so a sample is not
     * stuck waiting for the next 16 ms game tick.
     */
    struct PumpedEvent
    {
        ENetEventType type = ENET_EVENT_TYPE_NONE;
        ENetPeer *peer = nullptr;
        std::vector<u8> packet;
        std::chrono::steady_clock::time_point received_at{};
    };
    /** Serialized send. The pump thread writes it to the socket so the game thread never waits on select. */
    struct OutboundPacket
    {
        ENetPeer *peer = nullptr;
        bool broadcast = false;
        bool stamp_ping = false;
        enet_uint32 flags = 0;
        std::vector<u8> data;
    };
    struct ProbeStamp
    {
        ENetPeer *peer = nullptr;
        std::chrono::steady_clock::time_point when{};
    };
    std::mutex enet_mu;
    std::mutex inbox_mu;
    std::mutex outbox_mu;
    std::mutex probe_mu;
    std::mutex shown_peers_mu;
    std::vector<PumpedEvent> inbox;
    std::vector<OutboundPacket> outbox;
    std::vector<ProbeStamp> probe_stamps;
    std::vector<u32> shown_connect_ids;
    std::thread net_thread;
    std::atomic<bool> net_pump_stop{false};
    f32 playback_speed = 1.0f;

    std::map<u32, CMatrixRobotAI*> robots;
    std::map<u32, CMatrixCannon *> turrets;

    void delete_turret(CMatrixCannon *cannon);

    /** Host-only counter for the next gameplay entity NID (robots, turrets, buildings). */
    u32 allocate_nid();
    /** After receiving an authoritative NID (snapshot spawn), keep the local counter above it. */
    void note_authoritative_nid(u32 nid);
    /**
     * Assign m_NID when zero. Map-placed buildings use @p allow_on_client so clients match the host
     * load order; robots/turrets/runtime spawns are host-only (clients get NIDs from snapshots).
     */
    void assign_network_id(class CMatrixMapStatic *obj, bool allow_on_client = false);

    Lobby lobby;

    // bool next_frame_requested   = false; // should simulate next physics frame

    bool isCompactMode;
    /** F2 toggles on-screen / 3D debug overlay (m_DI text, CHelper gizmos, etc.). */
    bool show_debug_overlay = false;
    // The next "free" networkable ID.
    // Used for robots, turrets, factories and bases.
    u32 next_nid              = 0;

    RingBuffer<WorldSnapshot, INTERPOLATION_BUFFER_SIZE> interpolation_buffer;

    std::priority_queue<Event, std::vector<Event>, EventComparator> events_queue;

    // RingBuffer<WorldSnapshot, INPUT_BUFFER_SIZE> history_game_states;

    // std::vector<Command> current_input{}; // list of all actions for

    u32 frames_passed_since_last_check = 0; // to calculate physics fps
    std::chrono::steady_clock::time_point last_check{}; // to calculate physics fps
    u32 physics_fps = 0;

    void process_network_frame(u32 delta_ms);
    void approve_final_input(u32 target_frame);

    // /**
    //  * @brief Access the record for frame n. If it doesn't exist yet, it is created and returned.
    //  */
    // inline CommandsFrameRecord* get_frame_record(const u32 frame)
    // {
    //     // Create missing elements if needed
    //     if (frame >= commands_journal.size())
    //     {
    //         for (u32 f = commands_journal.size(); f <= frame; f++)
    //         {
    //             commands_journal.push_back(CommandsFrameRecord(f));
    //         }
    //         commands_journal.resize(frame + 1);
    //     }
    //     // Retrieve
    //     auto it = commands_journal.rbegin();
    //     std::advance(it, commands_journal.size() - frame - 1);
    //     assert((*it).frame == frame);
    //     return &*it;
    // }
    //
    // inline CommandsFrameRecord* get_current_frame_record()
    // {
    //     return get_frame_record(physics_frame);
    // }
    void static_init_networking();
    void consume_input_frame(const u32 frame);
    void send_message(Message &msg);
    void send_message(Message &msg, ENetPeer *peer);
    /** Unreliable. Used for ping probes so a lost packet is not retransmitted into the sample. */
    void send_message_unreliable(Message &msg, ENetPeer *peer);
    void send_unreliable_unlocked(Message &msg, ENetPeer *peer);
    void poll_player_pings();
    void note_player_pong(ENetPeer *peer, u32 token, std::chrono::steady_clock::time_point when);
    void start_net_pump();
    void stop_net_pump();
    void net_pump_loop();
    /** Pump thread, or this thread when the pump is not running. */
    bool gather_net_events(std::vector<PumpedEvent> &events);
    void send_pong_unlocked(ENetPeer *peer, u32 token);
    void enqueue_packet(OutboundPacket pkt);
    bool transmit_packet_unlocked(OutboundPacket &pkt);
    void send_outbox_batch_unlocked(std::vector<OutboundPacket> &pending);
    std::vector<OutboundPacket> take_outbox();
    void apply_probe_stamps();
    void publish_connected_ids_unlocked();

    void save_commands_journal_to_file();

    std::string commands_journal_to_json_string();

    logger_type lgr{isClient2 ? "client2.log" : "client3.log"};

    // void add_input_for_current_input_frame(const Command &command);

    bool is_client() const
    {
        return network_mode == NetworkMode::CLIENT;
    }

    bool is_authority() const
    {
        return network_mode == NetworkMode::SERVER;
    }

    /** True on host and offline play; false on multiplayer clients (view/interpolate only). */
    bool loads_map_dynamics() const
    {
        return network_mode != NetworkMode::CLIENT;
    }

    void set_network_mode(NetworkMode new_network_mode)
    {
        network_mode = new_network_mode;
    }

public:
    // double linked list of all commands for all frames
    // Access through public methods
    // TODO: consider changing to std::map?
    // std::list<CommandsFrameRecord> commands_journal;

    void handle_new_world_snapshot(WorldSnapshot ws);
    void process_incoming_message(const Message &msg);
    void init_client_host();
    void init_server_host();
    /** Notify the server if the link is still up, destroy the client host, and drop session state. */
    void deinit_client_host();
    /** Disconnect every client, destroy the server host, and drop session state. */
    void deinit_server_host();
    void release_session();
    MessageGameSettingsParams connect_to_server(std::string ip);
    void process_server_network_frame();
    void initialize_replay_mode_with_files(std::wstring commands_filename);
    void initialize_replay_mode_with_files(std::wstring commands_filename, std::wstring checksums_filename);
};

// Places an order to move robot
void NetOrderMoveTo(u32 entity_nid, const D3DXVECTOR3& destination);

// Places an order to move robot
void NetOrderMoveTo(const std::vector<u32> &entities_nid, const D3DXVECTOR3& destination);
void NetOrderMoveTo(const std::vector<u32> &entities_nid, const D3DXVECTOR3& destination, int local_side_id);

// Places an order to attack robot
void NetOrderAttack(const std::vector<u32> &entities_nid, u32 target_nid);
void NetOrderAttack(const std::vector<u32> &entities_nid, u32 target_nid, int local_side_id);
void NetOrderAttack(const std::vector<u32> &entities_nid, u32 target_map_x, u32 target_map_y, int local_side_id);

void NetOrderStop(const std::vector<u32> &entities_nid, bool break_robot_orders);
void NetOrderStop(const std::vector<u32> &entities_nid, int local_side_id, bool break_robot_orders);

void NetOrderCapture(const std::vector<u32> &entities_nid, u32 target_nid);
void NetOrderCapture(const std::vector<u32> &entities_nid, u32 target_nid, int local_side_id);

// Places an order to construct a robot
void NetOrderConstruct(ERobotUnitKind chassis, ERobotUnitKind hull, ERobotUnitKind head,
                        const std::vector<ERobotUnitKind> &weapons, u8 robot_count, u32 base);

void NetOrderBuildTurret(u8 turret_type, u32 parent_building_nid, i32 place, f32 pos_x, f32 pos_y, u8 angle_packed);

// namespace nw = network;

extern Network g_Network;
/** Kept in sync with g_Network.show_debug_overlay for MatrixLib debug gating. */
extern bool g_MatrixDebugOverlayVisible;