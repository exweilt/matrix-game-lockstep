#include "StateManager.hpp"

u8 controllable_side_id = static_cast<u8>(SideID::RED);
u32 g_graphics_tick = 0;
u32 g_physics_tick = 0;
u32 g_total_ms = 0;

bool next_frame_requested = false;

u32 g_next_nid = 0;