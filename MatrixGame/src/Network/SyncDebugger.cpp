#include "SyncDebugger.hpp"

RingBuffer<SyncFrameLog, INPUT_BUFFER_SIZE> g_SyncLogs;

void sync_trace(const char * filename, u32 line, u32 val, const char * varname)
{
    // we are logging what is gonna affect the next frame, not current, so we log the next
    g_SyncLogs.get(g_Network.physics_frame + 1).trace(filename, line, val, varname);
}

std::string SyncFrameLog::to_json_string()
{
    std::ostringstream ss;
    cereal::JSONOutputArchive oarchive(ss);
    oarchive(cereal::make_nvp("traces_led_to_frame", frame));
    oarchive(cereal::make_nvp("traces", frame_log));

    return ss.str();
}

void SyncFrameLog::trace(const char *file, u32 line, u32 val, const char *varName)
{
    hash_combine(line);
    hash_combine(val);

    frame_log.push_back({line, file, val});
}

std::string sync_logs_to_json()
{
    std::ostringstream ss;
    cereal::JSONOutputArchive oarchive(ss);

    for (int i = 0; i < INPUT_BUFFER_SIZE; i++)
    {

        // oarchive(cereal::make_nvp("traces_led_to_frame", g_SyncLogs.f));
        // std::string name =
        oarchive(cereal::make_nvp("traces" + std::to_string(g_SyncLogs.data[i].frame), g_SyncLogs.data[i]));
    }

    return ss.str();
}