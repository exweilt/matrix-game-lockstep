#include "Snapshot.hpp"

// #include <xxhash.h>

#include <cereal/archives/binary.hpp>

#include "MatrixRobot.hpp"
#include "Stopwatch.hpp"

// #define PROFILING

std::string WorldSnapshot::to_json_string()
{
    std::ostringstream ss;

    {
        cereal::JSONOutputArchive oarchive(ss);
        oarchive(cereal::make_nvp("world_snapshot", *this));
    }
    return ss.str();
}

void WorldSnapshot::serialize_to_bitstream(BitWriter &writer) const
{
    writer.write_u32(this->frame);

    // for (const auto& pair : this->sides) {
    //     std::cout << pair.first << " is " << pair.second << " years old.\n";
    // }
    //
    // for (const auto& pair : ages) {
    //     std::cout << pair.first << " is " << pair.second << " years old.\n";
    // }
    // writer.write_u64(this->checksum);
}

WorldSnapshot WorldSnapshot::deserialize_from_bitstream(BitReader &reader)
{
    return WorldSnapshot();
}

// u64 WorldSnapshot::hash()
// {
// #ifdef PROFILING
//     Stopwatch stopwatch;
// #endif
//     // std::ostringstream ss(std::ios::binary);
//     // cereal::BinaryOutputArchive archive(os);
//     // archive(data);
//
//     std::ostringstream ss(std::ios::binary);
//     cereal::BinaryOutputArchive oarchive(ss);
//     oarchive(cereal::make_nvp("world_snapshot", *this));
//
//     u64 hash = XXH64(ss.str().c_str(), ss.str().length(), 0);
//
// #ifdef PROFILING
//     std::cout << "=== Hash world elapsed time: " << stopwatch.elapsed_ms() << "\n";
// #endif
//     return hash;
// }

WorldSnapshot capture_world_snapshot()
{

#ifdef PROFILING
Stopwatch cws_stopwatch;
#endif
    WorldSnapshot result;

    result.frame = g_Network.physics_frame;

    // ============= Serialize sides ===============
    for (u8 i = 0; i < g_MatrixMap->m_SideCnt; i++)
    {
        CMatrixSideUnit *side = g_MatrixMap->m_Side + i;

        result.sides.emplace(side->m_Id, SideSnapshot{
            static_cast<u8>(side->m_Id),
            side->GetStatus(),
            side->GetResourcesAmount(TITAN),
            side->GetResourcesAmount(ELECTRONICS),
            side->GetResourcesAmount(ENERGY),
            side->GetResourcesAmount(PLASMA)
        });
    }

    // ============= Serialize statics ===============
    for (CMatrixMapStatic *obj = CMatrixMapStatic::GetFirstLogic(); obj; obj = obj->GetNextLogic())
    {
        if (obj->IsBuilding())
        {
            // CMatrixBuilding *building = obj->AsBuilding();
            // result.buildings.emplace(building->m_NID, BuildingSnapshot{
            //     building->m_Kind,
            //     static_cast<u8>(building->m_Side),
            //     building->GetHitPoint()
            // });
        }
        else if (obj->IsLiveRobot())
        {
            CMatrixRobotAI *robot = obj->AsRobot();
            result.robots.emplace(robot->m_NID, RobotSnapshot{
                robot->m_PosX,
                robot->m_PosY,
                robot->GetHitPoint()
            });
        }
    }

#ifdef PROFILING
    std::cout << "= Capture world snapshot elapsed time: " << cws_stopwatch.elapsed_ms() << "\n";
#endif
    return result;
}
