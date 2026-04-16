#include "Snapshot.hpp"

// #include <xxhash.h>

#include <cereal/archives/binary.hpp>

#include "MatrixRobot.hpp"
#include "Stopwatch.hpp"

// #define PROFILING

void RobotSnapshot::serialize_to_bitstream(BitWriter &writer) const
{
    writer.write_u32(nid);
    writer.write_f32(x);
    writer.write_f32(y);
    writer.write_f32(health);
    writer.write_f32(maxhealth);
    writer.write_u8(chassis);
    writer.write_u8(hull);
    writer.write_u8(head);
    writer.write_u8(rotation);
    writer.write_u8(hull_rotation);
    writer.write_u8(weapon_cnt);

    for (int i = 0; i < weapon_cnt; i++)
        writer.write_u8(weapons[i]);
}

RobotSnapshot RobotSnapshot::deserialize_from_bitstream(BitReader &reader)
{
    RobotSnapshot result;

    result.nid = reader.read_u32();
    result.x = reader.read_f32();
    result.y = reader.read_f32();
    result.health = reader.read_f32();
    result.maxhealth = reader.read_f32();
    result.chassis = reader.read_u8();
    result.hull = reader.read_u8();
    result.head = reader.read_u8();
    result.rotation = reader.read_u8();
    result.hull_rotation = reader.read_u8();
    result.weapon_cnt = reader.read_u8();

    for (int i = 0; i < result.weapon_cnt; i++)
        result.weapons[i] = reader.read_u8();

    return result;
}

RobotSnapshot RobotSnapshot::from_robot(CMatrixRobotAI *robot)
{
    RobotSnapshot result;

    result.nid = robot->m_NID;
    result.x = robot->m_PosX;
    result.y = robot->m_PosY;
    result.health = robot->GetHitPoint();
    result.maxhealth = robot->GetMaxHitPoint();
    // result.weapons = {0};
    result.rotation = 0;
    result.hull_rotation = 0;

    result.weapon_cnt = 0;
    for (int i = 0; i < robot->m_UnitCnt; i++)
    {
        if (robot->m_Unit[i].m_Type == MRT_CHASSIS)
        {
            result.chassis = robot->m_Unit[i].u1.s1.m_Kind;
        }
        else if (robot->m_Unit[i].m_Type == MRT_ARMOR)
        {
            result.hull = robot->m_Unit[i].u1.s1.m_Kind;
        }
        else if (robot->m_Unit[i].m_Type == MRT_HEAD)
        {
            result.head = robot->m_Unit[i].u1.s1.m_Kind;
        }
        else if (robot->m_Unit[i].m_Type == MRT_WEAPON)
        {
            result.weapons[result.weapon_cnt++] = robot->m_Unit[i].u1.s1.m_Kind;
        }
    }

    return result;
}

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
            result.robots.emplace(
                robot->m_NID,
                RobotSnapshot::from_robot(robot)
            );
        }
    }

#ifdef PROFILING
    std::cout << "= Capture world snapshot elapsed time: " << cws_stopwatch.elapsed_ms() << "\n";
#endif
    return result;
}
