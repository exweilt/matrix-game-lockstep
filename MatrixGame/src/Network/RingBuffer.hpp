#pragma once
#include <cassert>

#include "Types.hpp"


template <typename T, size_t N>
class RingBuffer
{
    T data[N];
    u32 next_free = 0;

public:
    // void put(u32 frame, const T &value);
    // T get(u32 frame);
    // bool has(u32 frame);
    // void set_next_frame(u32 frame);

void put(u32 frame, const T &value)
    {
        if (frame > next_free || frame < next_free)
            assert(false);

        data[frame % N] = value;
        next_free = (frame + 1);
    }

    T get(u32 frame)
    {
        assert(has(frame));
        return data[frame % N];
    }

    bool has(u32 frame)
    {
        u32 diff = next_free - frame;
        return diff > 0 && diff <= N;
    }

    void set_next_frame(u32 frame)
    {
        next_free = frame;
    }
};
