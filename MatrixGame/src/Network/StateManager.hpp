#pragma once

#include "Command.hpp"
#include "Types.hpp"

// #include <bitset>
#include <list>
#include <memory>
#include <vector>

enum class SideID : u8
{
    YELLOW = 1,
    RED = 2,
    BLUE = 3,
    GREEN = 4,
};

constexpr u32 PHYSICS_TICK_RATE = 10;
static_assert(PHYSICS_TICK_RATE >= 1 && PHYSICS_TICK_RATE < 200);

constexpr u32 PHYSICS_TICK_PERIOD_MS = static_cast<u32>(1000.0 / PHYSICS_TICK_RATE) + 1;

extern u8 controllable_side_id; // SideID
extern u32 g_graphics_tick;
extern u32 g_physics_tick;
extern u32 g_total_ms;

extern bool next_frame_requested;

// The next "free" networkable ID.
// Used for robots, turrets, factories and bases.
extern u32 g_next_nid;

namespace network
{
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
            return commands[side_id - 1] != nullptr;
        }

        std::vector<Command>* get_side_inputs(const u32 side_id) const
        {
            return commands[side_id - 1].get();
        }

        void set_side_inputs(const u32 side_id, const std::vector<Command>& side_inputs)
        {
            commands[side_id - 1] = std::make_unique<std::vector<Command>>(side_inputs);
        }
    };

    extern std::list<CommandsFrameRecord> commands_journal;

    inline CommandsFrameRecord* get_frame_record(const u32 frame)
    {
        auto it = commands_journal.rbegin();
        std::advance(it, commands_journal.size() - frame - 1);
        return &*it;
    }

    void static_init_networking();

    void consume_input_frame(const u32 frame);
}