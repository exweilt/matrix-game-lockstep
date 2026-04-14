#ifndef STOPWATCH_HPP
#define STOPWATCH_HPP
#include <chrono>

// #include <time.h>
#include <chrono>


class Stopwatch
{
public:
    Stopwatch(bool start_counting = true);
    ~Stopwatch() = default;

    void restart();
    // Stopwatch &stop();
    double elapsed_ms();
private:
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_end_time;
    double m_cached_elapsed;
    bool m_is_counting;
};



#endif //STOPWATCH_HPP
