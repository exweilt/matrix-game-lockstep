#pragma once

#include "Command.hpp"
#include "Types.hpp"

#include <chrono>
#include <list>
#include <memory>
#include <vector>

constexpr u32 input_buffer_size = 15; // Size of input buffering

extern u8 controllable_side_id; // SideID
extern u32 g_graphics_frame;
extern u32 g_physics_frame;
extern u32 g_input_frame; // Sampling for
extern u32 g_total_ms;
extern bool isClient2;
extern f64 g_time_to_next_input;
extern bool game_ongoing;

extern bool next_frame_requested;

// The next "free" networkable ID.
// Used for robots, turrets, factories and bases.
extern u32 g_next_nid;
extern std::vector<network::Command> current_input;

extern u32 frames_passed_since_last_check;
extern std::chrono::high_resolution_clock::time_point last_check;
extern u32 physics_fps;

namespace network
{
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
    };

    extern std::list<CommandsFrameRecord> commands_journal;

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
        return &*it;
    }

    inline CommandsFrameRecord* get_current_frame_record()
    {
        return get_frame_record(g_physics_frame);
    }

    void static_init_networking();

    void consume_input_frame(const u32 frame);
}

namespace nw = network;