#pragma once

#include "Command.hpp"
#include "Message.hpp"
#include "Types.hpp"
#include <enet/enet.h>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/vector.hpp>

#include <chrono>
#include <list>
#include <memory>
#include <vector>

constexpr u32 INPUT_BUFFER_SIZE = 15; // Size of input buffering
constexpr u32 PHYSICS_FRAME_RATE = 10;
static_assert(PHYSICS_FRAME_RATE >= 1 && PHYSICS_FRAME_RATE < 200);
constexpr u32 PHYSICS_FRAME_PERIOD_MS = static_cast<u32>(1000.0 / PHYSICS_FRAME_RATE) + 1;

enum class SideID : u8
{
    YELLOW = 1,
    RED = 2,
    BLUE = 3,
    GREEN = 4,
};

struct CommandsFrameRecord
{
    u32 frame; // The commands record for this physics frame (tick).

    /**
     * @brief Vectors of commands for each side (0 is yellow) for this frame.
     *
     * If s[i] == nullptr then the commands are not present for that side and frame.
     */
    std::unique_ptr< std::vector<Command> > commands [4];

    CommandsFrameRecord(const u32 f = 0) : frame(f) {}

    bool is_side_input_ready(const u32 side_id) const
    {
        return commands[side_id - 1].get() != nullptr;
    }

    std::vector<Command>* get_side_inputs(const u32 side_id) const
    {
        return commands[side_id - 1].get();
    }

    void set_side_inputs(const u32 side_id, const std::vector<Command>& side_inputs)
    {
        commands[side_id - 1] = std::make_unique<std::vector<Command>>(side_inputs);
    }

    void set_side_inputs(const u32 side_id)
    {
        commands[side_id - 1] = std::make_unique<std::vector<Command>>();
    }

    template <class Archive>
    void serialize(Archive& ar) {
        ar(
            CEREAL_NVP(frame)
        );
        for (int i = 0; i < 4; i++) {
            ar( cereal::make_nvp("commands_" + std::to_string(i + 1), commands[i]) );
        }
    }
};

// extern std::list<CommandsFrameRecord> commands_journal;



class Network
{
public:
    Network()  = default;
    ~Network() = default;

    ENetHost* client_host;

    u8 controllable_side_id     = static_cast<u8>(SideID::RED); // SideID
    u32 graphics_frame        = 0; // current rendering frame
    u32 physics_frame         = 0; // current physics frame
    u32 input_frame           = 0; // new inputs are sampled for this physics frame
    u32 total_ms              = 0;
    bool isClient2              = std::getenv("CLIENT2") != nullptr;
    f64 time_to_next_input    = 0.017; // time in seconds until switching input_frame
    bool game_ongoing           = false;
    bool next_frame_requested   = false; // should simulate next physics frame

    // The next "free" networkable ID.
    // Used for robots, turrets, factories and bases.
    u32 next_nid              = 0;

    // std::vector<Command> current_input{}; // list of all actions for

    u32 frames_passed_since_last_check = 0; // to calculate physics fps
    std::chrono::high_resolution_clock::time_point last_check{}; // to calculate physics fps
    u32 physics_fps = 0;

    void process_network_frame(u32 delta_ms);
    void approve_final_input(u32 target_frame);

    /**
     * @brief Access the record for frame n. If it doesn't exist yet, it is created and returned.
     */
    inline CommandsFrameRecord* get_frame_record(const u32 frame)
    {
        // Create missing elements if needed
        if (frame >= commands_journal.size())
        {
            for (u32 f = commands_journal.size(); f <= frame; f++)
            {
                commands_journal.push_back(CommandsFrameRecord(f));
            }
            commands_journal.resize(frame + 1);
        }
        // Retrieve
        auto it = commands_journal.rbegin();
        std::advance(it, commands_journal.size() - frame - 1);
        assert((*it).frame == frame);
        return &*it;
    }

    inline CommandsFrameRecord* get_current_frame_record()
    {
        return get_frame_record(physics_frame);
    }
    void static_init_networking();
    void consume_input_frame(const u32 frame);

    void save_commands_journal_to_file();

private:
    // double linked list of all commands for all frames
    // Access through public methods
    // TODO: consider changing to std::map?
    std::list<CommandsFrameRecord> commands_journal;

    void send_message(Message &msg);
    void process_incoming_message(const Message &msg);
    void init_client_host();
    void deinit_client_host();
    void connect_to_server();
};

// namespace nw = network;

extern Network g_Network;