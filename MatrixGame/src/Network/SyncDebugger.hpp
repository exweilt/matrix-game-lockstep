#pragma once

// #define DEBUG_DESYNCS
constexpr bool DEBUG_DESYNCS = true;

#include "RingBuffer.hpp"
#include <cstddef>

#include <xxhash.h>
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>

#include "Network.hpp"


constexpr const char* basename(const char* path) {
    const char* lastSlash = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') lastSlash = p + 1;
    }
    return lastSlash;
}

struct SyncLogEntry
{
    u32 line;
    const char* file;
    u32 value;
    // u64 running_hash;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("file", std::string(basename(file))), CEREAL_NVP(line), CEREAL_NVP(value));
    }
};

struct SyncFrameLog
{
    u32 frame;
    std::vector<SyncLogEntry> frame_log;
    XXH64_state_t *hash_state;

    SyncFrameLog(u32 f = 0)
    {
        hash_state = XXH64_createState();
        reset(f);
    }

    void reset(u32 new_frame)
    {
        frame_log.clear();
        frame = new_frame;
        XXH64_reset(hash_state, 0);
    }

    void trace(const char* file, u32 line, u32 val, const char* varName = "");

    inline void hash_combine([[maybe_unused]] u32 val)
    {
        // XXH64_update(hash_state, &val, sizeof(val));
    }

    std::string to_json_string();

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(frame), CEREAL_NVP(frame_log));
    }
};

extern RingBuffer<SyncFrameLog, INPUT_BUFFER_SIZE> g_SyncLogs;

void sync_trace(const char * filename, u32 line, u32 val, const char * varname);

std::string sync_logs_to_json();


#define SYNC_TRACE() \
do { if constexpr(DEBUG_DESYNCS) sync_trace(__FILE__, __LINE__, 0, "TRACE"); } while(0) // this avoids bug if nested in external if else

#define SYNC_TRACE_VAR(varname) \
do { if constexpr(DEBUG_DESYNCS) sync_trace(__FILE__, __LINE__, varname, #varname); } while(0) // this avoids bug if nested in external if else

// extern SyncDebugger g_SyncDebug;

// class SyncLogger
// {
//     RingBuffer<SyncLogEntry, INPUT_BUFFER_SIZE> log;
//
//
// public:
//     SyncLogger() {};
// };


// // MACROS
// // 1. Trace just the execution path (Did we enter this if-statement?)
// #define SYNC_POINT() g_SyncDebug.Trace(__FILE__, __LINE__, 0, "POINT")
//
// // 2. Trace a specific integer/float value
// #define SYNC_VAR(x) g_SyncDebug.Trace(__FILE__, __LINE__, (uint32_t)(x), #x)