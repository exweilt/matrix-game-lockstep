#include "Snapshot.hpp"

// #include <xxhash.h>

#include <cereal/archives/binary.hpp>

#include "MatrixRobot.hpp"
#include "Stopwatch.hpp"

// #define PROFILING

void SideSnapshot::serialize_to_bitstream(BitWriter &writer) const
{
    writer.write_u8(side);
    writer.write_u32(status);
    writer.write_u32(titanium);
    writer.write_u32(electronics);
    writer.write_u32(energy);
    writer.write_u32(plasma);
}

SideSnapshot SideSnapshot::deserialize_from_bitstream(BitReader &reader)
{
    SideSnapshot result{};

    result.side = reader.read_u8();
    result.status = static_cast<ESideStatus>(reader.read_u32());
    result.titanium = reader.read_u32();
    result.electronics = reader.read_u32();
    result.energy = reader.read_u32();
    result.plasma = reader.read_u32();

    return result;
}

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
    writer.write_u8(pack_rotation(rotation));
    writer.write_u8(pack_rotation(hull_rotation));
    writer.write_u8(side);
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
    result.rotation = unpack_rotation(reader.read_u8());
    result.hull_rotation = unpack_rotation(reader.read_u8());
    result.side = reader.read_u8();
    result.weapon_cnt = reader.read_u8();

    for (int i = 0; i < result.weapon_cnt; i++)
        result.weapons[i] = reader.read_u8();

    return result;
}

uint8_t pack_rotation(float radians) {
    float shifted = radians + M_PI;

    float scaled = shifted * (256.0f / (2.0f * M_PI));

    return static_cast<uint8_t>(scaled);
}

f32 unpack_rotation(uint8_t quantized) {
    float radians = static_cast<float>(quantized) * (2.0f * M_PI / 256.0f);
    return radians - M_PI;
}

void EventFire::serialize_to_bitstream(BitWriter &writer) const
{
    writer.write_u32(nid);
    writer.write_vec3(target_pos);
    writer.write_u32(frame);
    writer.write_u8(weapons);
    writer.write_u8(precise_time);
}

EventFire EventFire::deserialize_from_bitstream(BitReader &reader)
{
    EventFire result{};
    result.nid = reader.read_u32();
    result.target_pos = reader.read_vec3();
    result.frame = reader.read_u32();
    result.weapons = reader.read_u8();
    result.precise_time = reader.read_u8();
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
    result.rotation = robot->GetRotationZ();
    result.hull_rotation = robot->GetHullRotationZ();
    result.side = robot->GetSide();

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

std::string WorldSnapshot::to_json_string() const
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

    writer.write_u32(sides.size());
    for (const auto& [sideId, side_snapshot] : sides) {
        side_snapshot.serialize_to_bitstream(writer);
    }

    writer.write_u32(robots.size());
    for (const auto& [Id, rob_snapshot] : robots) {
        rob_snapshot.serialize_to_bitstream(writer);
    }
}

WorldSnapshot WorldSnapshot::deserialize_from_bitstream(BitReader &reader)
{
    WorldSnapshot result{};

    result.frame = reader.read_u32();

    int sides_count = reader.read_u32();
    for (int i = 0; i < sides_count; i++)
    {
        SideSnapshot ss = SideSnapshot::deserialize_from_bitstream(reader);
        result.sides.emplace(ss.side, ss);
    }

    int rob_count = reader.read_u32();
    for (int i = 0; i < rob_count; i++)
    {
        RobotSnapshot rs = RobotSnapshot::deserialize_from_bitstream(reader);
        result.robots.emplace(rs.nid, rs);
    }

    return result;
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
