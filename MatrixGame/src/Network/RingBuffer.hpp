#pragma once
#include <cassert>

#include "Types.hpp"


template <typename T, size_t N>
class RingBuffer
{
private:
    size_t head = 0; // Index for the next write
    size_t tail = 0; // Index for the next read
    size_t count = 0;

public:
    T data[N];

    // If full, it "pushes" the tail forward
    void push(const T& item)
    {
        data[head] = item;

        if (count == N) {
            tail = (tail + 1) % N;
            std::cout << "Out of space in ring buffer." << std::endl;
        } else {
            count++;
        }

        head = (head + 1) % N;
    }

    size_t size() const { return count; }

    size_t capacity() const { return N; }

    void clear()
    {
        head = 0;
        tail = 0;
        count = 0;
    }

    bool get_lerp_targets(T& outPast, T& outFuture)
    {
        if (count < 2) return false;

        outPast     = data[(tail) % N];
        outFuture   = data[(tail + 1) % N];
        return true;
    }

    void pop_front()
    {
        if (count == 0) return;

        tail = (tail + 1) % N;
        count--;
    }

    // Access the oldest item without removing it
    T& front()
    {
        return data[tail];
    }

    // Access the newest item without removing it
    T& back()
    {
        size_t lastIdx = (head + N - 1) % N;
        return data[lastIdx];
    }
};

// template <typename T, size_t N>
// class RingBuffer
// {
// public:
//     T data[N];
//     u32 next_free = 0;
//
//     // void put(u32 frame, const T &value);
//     // T get(u32 frame);
//     // bool has(u32 frame);
//     // void set_next_frame(u32 frame);
//
// void put(u32 frame, const T &value)
//     {
//         if (frame > next_free || frame < next_free)
//             assert(false);
//
//         data[frame % N] = value;
//         next_free = (frame + 1);
//     }
//
//     T &get(u32 frame)
//     {
//         assert(has(frame));
//         return data[frame % N];
//     }
//
//     bool has(u32 frame)
//     {
//         i32 diff = next_free - frame;
//         return diff > 0 && diff <= N;
//     }
//
//     void set_next_frame(u32 frame)
//     {
//         next_free = frame;
//     }
// };
