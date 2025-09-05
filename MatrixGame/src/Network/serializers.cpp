#include "serializers.hpp"

#include <fstream>

#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <xxhash.h>

#include "MatrixRobot.hpp"
#include "Network.hpp"
#include "MatrixGame.h"

#include <map>

// TODO: bug checksum depends on order of insertion, use std::map to avoid that.
u64 serialize_map(bool calculate_checksum, std::optional<std::string> json_filename)
{
    std::ostringstream os;
    cereal::JSONOutputArchive archive{os};

    // Form the map storage of all robots on the map
    std::map<u32, std::shared_ptr<CMatrixRobotAI>> robots; // strange isn't it?
    CMatrixMapStatic *obj = CMatrixMapStatic::GetFirstLogic();
    while (obj)
    {
        if (obj->IsLiveRobot())
        {
            robots.emplace( obj->AsRobot()->m_NID, std::shared_ptr<CMatrixRobotAI>(obj->AsRobot(),[](CMatrixRobotAI*) { /* no-op deleter: do not delete */ }));
            // archive( cereal::make_nvp("Robot " + std::to_string(obj->AsRobot()->m_NID),  *(obj->AsRobot())) );
        }
        obj = obj->GetNextLogic();
    }

    // Archive the robots map
    archive(cereal::make_nvp("robots", robots));

    u64 return_value = 0;
    if (calculate_checksum == true)
    {
        constexpr XXH64_hash_t hashing_seed = 0;
        return_value = XXH64(os.str().c_str(), os.str().length(), hashing_seed);
    }

    if (json_filename.has_value())
    {
        std::ofstream ofs{json_filename.value(), std::ofstream::binary};
        ofs.write(os.str().c_str(), static_cast<std::streamsize>(os.str().size()));
    }

    return return_value;
}
