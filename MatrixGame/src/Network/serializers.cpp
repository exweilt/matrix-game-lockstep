#include "serializers.hpp"

#include <fstream>

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <xxhash.h>

#include "MatrixRobot.hpp"
#include "Network.hpp"
#include "MatrixGame.h"

#include <map>

struct SideData // for serialization only
{
    ESideStatus status;
    int titanium;
    int electronics;
    int energy;
    int plasma;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(status), CEREAL_NVP(titanium), CEREAL_NVP(electronics), CEREAL_NVP(energy), CEREAL_NVP(plasma));
    }
};

// TODO: bug checksum depends on order of insertion, use std::map to avoid that.
// TODO: serialize enums using names, not int!
u64 serialize_map(bool calculate_checksum, std::optional<std::string> json_filename)
{
    std::ostringstream os;
    cereal::JSONOutputArchive archive{os};

    // ============= Serialize sides ===============
    std::map<u8, SideData> sides;
    for (u8 i = 0; i < g_MatrixMap->m_SideCnt; i++)
    {
        CMatrixSideUnit *side = g_MatrixMap->m_Side + i;
        SideData data
        {
            side->GetStatus(),
            side->GetResourcesAmount(TITAN),
        side->GetResourcesAmount(TITAN),
        side->GetResourcesAmount(TITAN),
        side->GetResourcesAmount(TITAN)
        };

        sides.emplace(side->m_Id, data);
    }
    archive(cereal::make_nvp("sides", sides));


    // ============= Serialize buildings ===============
    std::map<u32, std::shared_ptr<CMatrixBuilding>> buildings;
    CMatrixMapStatic *obj = CMatrixMapStatic::GetFirstLogic();
    while (obj)
    {
        if (obj->IsBuilding())
        {
            buildings.emplace(
                obj->AsRobot()->m_NID,
                std::shared_ptr<CMatrixBuilding>(obj->AsBuilding(),[](CMatrixBuilding*) { /* no-op deleter: do not delete */ })
            );
        }
        obj = obj->GetNextLogic();
    }
    archive(cereal::make_nvp("buildings", buildings));



    // Form the map storage of all robots on the map
    std::map<u32, std::shared_ptr<CMatrixRobotAI>> robots; // strange isn't it?
    obj = CMatrixMapStatic::GetFirstLogic();
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
