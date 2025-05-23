#pragma once

#include <fstream>
#include <chrono>
#include <iostream>
#include <source_location>
#include <string_view>

namespace logger
{
enum class level : uint8_t
{ 
    fatal,
    error,
    warning,
    info,
    debug,
    trace,
    nolog
}; 

/// Just a stupid logger class.
/// Not much optimized, not much pretty, but doing its job with no
/// questions and additional external deps.
/// Simple usage example is at the very end of this file.
template <logger::level type>
class stupid
{
public:
    template <logger::level _level>
    class entry
    {
        friend class stupid;

        entry(std::ostream& out, std::source_location caller, std::string_view format, size_t tick)
        : _out{out}
        {
            if constexpr (_level != logger::level::nolog)
            {
                _caller = caller;
                _log_line = format;
                _tick = tick;
            }
        }

    public:
        entry(entry&&) = default;
        ~entry()
        {
            if constexpr (_level != logger::level::nolog)
            {
                _out <<
                    std::format(
                        "{:%F %T} |{:05d}|{}| {} | {}:{}\n",
                        std::chrono::system_clock::now(),
                        _tick,
                        get_formatted_level(),
                        _log_line,
                        _caller.file_name(),
                        _caller.line());
                _out.flush();
            }
        }

        template<typename... Args>
        void operator() (Args&&... args)
        {
            if constexpr (_level != logger::level::nolog)
            {
                _log_line = std::vformat(_log_line, std::make_format_args(args...));
            }
        }
    
    private:
        static consteval std::string_view get_formatted_level()
        {
            switch(_level)
            {
                case level::fatal:   return "FATAL";
                case level::error:   return "ERROR";
                case level::warning: return " WARN";
                case level::info:    return " INFO";
                case level::debug:   return "DEBUG";
                case level::trace:   return "TRACE";
                case level::nolog:   return "NOLOG"; // shouldn't happen
            }

            return ""; // unreachable
        }

        std::ostream& _out;
        std::source_location _caller;
        std::string _log_line;
        size_t _tick;
    };

    stupid(const std::string& path)
    : _out_file{path, std::ios::app}
    , _out{_out_file}
    {
        if (!_out_file.is_open())
        {
            throw std::runtime_error("Failed to open log file: " + path);
        }
    }

    stupid(std::ostream& out)
    : _out{out}
    {
    }

    void add_ticks(size_t ticks)
    {
        _tick += ticks;
    }

    auto fatal(std::string_view format, std::source_location caller = std::source_location::current())
    {
        return create_entry<level::fatal>(caller, format);
    }

    auto error(std::string_view format, std::source_location caller = std::source_location::current())
    {
        return create_entry<level::error>(caller, format);
    }

    auto warning(std::string_view format, std::source_location caller = std::source_location::current())
    {
        return create_entry<level::warning>(caller, format);
    }

    auto info(std::string_view format, std::source_location caller = std::source_location::current())
    {
        return create_entry<level::info>(caller, format);
    }

    auto debug(std::string_view format, std::source_location caller = std::source_location::current())
    {
        return create_entry<level::debug>(caller, format);
    }

    auto trace(std::string_view format, std::source_location caller = std::source_location::current())
    {
        return create_entry<level::trace>(caller, format);
    }

private:
    template <logger::level value>
    auto create_entry(std::source_location caller, std::string_view format)
    {
        if constexpr (value <= type)
        {
            return entry<value>(_out, caller, format, _tick);
        }
        else
        {
            return entry<logger::level::nolog>(_out, caller, format, _tick);
        }
    }

    std::ofstream _out_file;
    std::ostream& _out;
    size_t _tick{0};
};

} // namespace logger

// Logger object should be defined somewhere else,
// probably somewhere close to main()
#ifdef _DEBUG
    using logger_type = logger::stupid<logger::level::trace>;
#else
    using logger_type = logger::stupid<logger::level::warning>;
#endif

extern logger_type lgr;

// Usage example:
// int main()
// {
//     logger_type lgr("./test.log");
//     lgr.fatal("{} - {}")("FATAL TEST", 42);
//     lgr.error("{} - {}")("ERROR TEST", 42);
//     lgr.warning("{} - {}")("WARNING TEST", 43);
//     lgr.info("{} - {}")("INFO TEST", 44);
//     lgr.debug("{} - {}")("DEBUG TEST", 45);
//     lgr.trace("{} - {}")("TRACE TEST", 46);

//     return 0;
// }
