#include "StateManager.hpp"

u8 controllable_side_id = static_cast<u8>(SideID::RED);
u32 g_graphics_tick = 0;
u32 g_physics_tick = 0;
u32 g_total_ms = 0;

bool next_frame_requested = false;

u32 g_next_nid = 0;

namespace network
{
    std::list<network::CommandsFrameRecord> commands_journal;

    void static_init_networking()
    {
        commands_journal.push_back(network::CommandsFrameRecord(0));
    }

    void consume_input_frame(const u32 frame)
    {
        CommandsFrameRecord* record = get_frame_record(frame);

        if (record->is_side_input_ready(controllable_side_id))
        {
            std::vector<Command>* commands = record->get_side_inputs(controllable_side_id);
            for (u32 i = 0; i < commands->size(); ++i)
            {
                (*commands)[i].execute_for_side(controllable_side_id);
            }
        }
    }
}

