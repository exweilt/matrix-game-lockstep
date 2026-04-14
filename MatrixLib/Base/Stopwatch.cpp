#include "Stopwatch.hpp"

Stopwatch::Stopwatch(const bool start_counting)
{
    m_cached_elapsed = 0.0;
    if (start_counting)
    {
        restart();
    }
    else
    {
        m_is_counting = false;
    }
}

void Stopwatch::restart()
{
    m_start_time = std::chrono::steady_clock::now();
    m_is_counting = true;
}

// Stopwatch& Stopwatch::stop()
// {
//     return *this;
// }

double Stopwatch::elapsed_ms()
{
    if (m_is_counting)
    {
        m_end_time = std::chrono::steady_clock::now();
        m_cached_elapsed = std::chrono::duration<double, std::milli>(m_end_time - m_start_time).count();
        m_is_counting = false;
    }
    return m_cached_elapsed;
}