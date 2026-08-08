#pragma once

#include "Command.hpp"
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

#include <chrono>
#include <list>
#include <memory>
#include <vector>
#include <queue>

constexpr u32 INPUT_BUFFER_SIZE = 15; // Size of input buffering
constexpr u32 PHYSICS_FRAME_RATE = 10;
static_assert(PHYSICS_FRAME_RATE >= 1 && PHYSICS_FRAME_RATE < 200);
constexpr u32 PHYSICS_FRAME_PERIOD_MS = static_cast<u32>(1000.0 / PHYSICS_FRAME_RATE) + 1;
constexpr u32 INTERPOLATION_BUFFER_SIZE = 5;


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

class Network
{
public:
    Network()  = default;
    ~Network() = default;

    void broadcast_world_snapshot();
    void broadcast_events();

    void delete_robot(CMatrixRobotAI * robot);

    void process_playback(int ms);
    void populate_robot(RobotSnapshot &rs);
    void add_event_to_current_tick(EventFire e);
    void clear_events_for_current_tick();
    void play_fire_event(const EventFire &event);

    ENetHost* host;

    NetworkMode network_mode = NetworkMode::NONE;
    u8 controllable_side_id     = static_cast<u8>(SideID::RED); // SideID
    u32 graphics_frame        = 0; // current rendering frame
    u32 physics_frame         = 0; // current physics frame
    // u32 input_frame           = 0; // new inputs are sampled for this physics frame
    // u32 total_ms              = 0;
    bool isClient2              = false;
    std::string server_ip;
    f64 time_to_next_input    = 0.017; // time in seconds until switching input_frame
    bool game_ongoing           = false;
    u32 desync_happened_at_frame = 0;
    bool desync_happened = false;
    bool has_physics_frame_run = false;
    u32 code_logic_frame = 1;
    std::vector<EventFire> this_tick_event_pool;
    f32 playback_speed = 1.0f;

    std::map<u32, CMatrixRobotAI*> robots;

    // bool next_frame_requested   = false; // should simulate next physics frame

    bool isCompactMode;
    // The next "free" networkable ID.
    // Used for robots, turrets, factories and bases.
    u32 next_nid              = 0;

    RingBuffer<WorldSnapshot, INTERPOLATION_BUFFER_SIZE> interpolation_buffer;

    std::priority_queue<EventFire, std::vector<EventFire>, EventFireComparator> events_queue;

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

    void set_network_mode(NetworkMode new_network_mode)
    {
        network_mode = new_network_mode;
    }

private:
    // double linked list of all commands for all frames
    // Access through public methods
    // TODO: consider changing to std::map?
    // std::list<CommandsFrameRecord> commands_journal;

    void handle_new_world_snapshot(WorldSnapshot ws);
    void process_incoming_message(const Message &msg);
    void init_client_host();
    void init_server_host();
    void deinit_client_host();
    void connect_to_server();
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

void NetOrderCapture(const std::vector<u32> &entities_nid, u32 target_nid);

// Places an order to construct a robot
void NetOrderConstruct(ERobotUnitKind chassis, ERobotUnitKind hull, ERobotUnitKind head,
                        const std::vector<ERobotUnitKind> &weapons, u8 robot_count, u32 base);

// namespace nw = network;

extern Network g_Network;