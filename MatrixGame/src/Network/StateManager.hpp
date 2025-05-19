#pragma once

#include "Types.hpp"

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

extern u8 controllable_side_id;
extern u32 g_graphics_tick;
extern u32 g_physics_tick;
extern u32 g_total_ms;

extern bool next_frame_requested;