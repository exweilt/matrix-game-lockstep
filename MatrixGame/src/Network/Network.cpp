#include "Network.hpp"

u8 controllable_side_id = static_cast<u8>(nw::SideID::RED);
u32 g_graphics_frame = 0;
u32 g_physics_frame = 0;
u32 g_total_ms = 0;
bool isClient2 = std::getenv("CLIENT2") != nullptr;

bool next_frame_requested = false;

u32 g_next_nid = 0;

namespace network
{
    std::list<network::CommandsFrameRecord> commands_journal;

    void static_init_networking()
    {
        controllable_side_id = static_cast<u8>(isClient2 ? SideID::BLUE : SideID::RED);
        commands_journal.push_back(network::CommandsFrameRecord(0));
    }

    void consume_input_frame(const u32 frame)
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
}

