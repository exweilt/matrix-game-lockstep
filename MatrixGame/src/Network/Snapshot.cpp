#include "Snapshot.hpp"

// #include <xxhash.h>

#include <algorithm>
#include <cereal/archives/binary.hpp>

#include "MatrixRobot.hpp"
#include "MatrixObjectCannon.hpp"
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
    writer.write_u8(animation);
    writer.write_u8(robot_state);
    writer.write_u32(spawn_base_nid);
    writer.write_u8(weapon_cnt);

    for (int i = 0; i < weapon_cnt; i++)
        writer.write_u8(weapons[i]);

    // writer.write_u8(is_firing);
    // if (is_firing)
    // {
    //     writer.write_vec3(target);
    // }
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
    result.animation = reader.read_u8();
    result.robot_state = reader.read_u8();
    result.spawn_base_nid = reader.read_u32();
    result.weapon_cnt = reader.read_u8();

    for (int i = 0; i < result.weapon_cnt; i++)
        result.weapons[i] = reader.read_u8();

    // result.is_firing = reader.read_u8();
    // if (result.is_firing)
    // {
    //     result.target = reader.read_vec3();
    // }

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
    result.animation = robot->GetAnimation();
    result.robot_state = static_cast<u8>(robot->m_CurrState);
    result.spawn_base_nid = robot->GetBase() ? robot->GetBase()->m_NID : 0u;
    result.head = 0;

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

    // result.target = robot->m_WeaponDir;
    // result.is_firing = false;

    return result;
}

BuildStackEntrySnapshot BuildStackEntrySnapshot::from_robot(CMatrixRobotAI *robot)
{
    BuildStackEntrySnapshot result{};
    result.weapon_cnt = 0;

    for (int i = 0; i < robot->m_UnitCnt; i++) {
        if (robot->m_Unit[i].m_Type == MRT_CHASSIS)
            result.chassis = robot->m_Unit[i].u1.s1.m_Kind;
        else if (robot->m_Unit[i].m_Type == MRT_ARMOR)
            result.hull = robot->m_Unit[i].u1.s1.m_Kind;
        else if (robot->m_Unit[i].m_Type == MRT_HEAD)
            result.head = robot->m_Unit[i].u1.s1.m_Kind;
        else if (robot->m_Unit[i].m_Type == MRT_WEAPON)
            result.weapons[result.weapon_cnt++] = robot->m_Unit[i].u1.s1.m_Kind;
    }

    return result;
}

static void write_build_stack_entry(BitWriter &writer, const BuildStackEntrySnapshot &entry)
{
    writer.write_u8(entry.chassis);
    writer.write_u8(entry.hull);
    writer.write_u8(entry.head);
    writer.write_u8(entry.weapon_cnt);
    for (u8 i = 0; i < entry.weapon_cnt; i++)
        writer.write_u8(entry.weapons[i]);
}

static BuildStackEntrySnapshot read_build_stack_entry(BitReader &reader)
{
    BuildStackEntrySnapshot result{};
    result.chassis = reader.read_u8();
    result.hull = reader.read_u8();
    result.head = reader.read_u8();
    result.weapon_cnt = reader.read_u8();
    for (u8 i = 0; i < result.weapon_cnt; i++)
        result.weapons[i] = reader.read_u8();
    return result;
}

void TurretSnapshot::serialize_to_bitstream(BitWriter &writer) const
{
    writer.write_u32(nid);
    writer.write_f32(x);
    writer.write_f32(y);
    writer.write_f32(health);
    writer.write_f32(maxhealth);
    writer.write_u8(type);
    writer.write_u8(side);
    writer.write_u8(cannon_state);
    writer.write_u32(parent_building_nid);
    writer.write_u32(static_cast<u32>(place));
    writer.write_u8(rotation);
    writer.write_u8(turret_yaw);
    writer.write_u8(pitch);
}

TurretSnapshot TurretSnapshot::deserialize_from_bitstream(BitReader &reader)
{
    TurretSnapshot result{};
    result.nid = reader.read_u32();
    result.x = reader.read_f32();
    result.y = reader.read_f32();
    result.health = reader.read_f32();
    result.maxhealth = reader.read_f32();
    result.type = reader.read_u8();
    result.side = reader.read_u8();
    result.cannon_state = reader.read_u8();
    result.parent_building_nid = reader.read_u32();
    result.place = static_cast<i32>(reader.read_u32());
    result.rotation = reader.read_u8();
    result.turret_yaw = reader.read_u8();
    result.pitch = reader.read_u8();
    return result;
}

TurretSnapshot TurretSnapshot::from_cannon(CMatrixCannon *cannon)
{
    TurretSnapshot result{};
    result.nid = cannon->m_NID;
    result.x = cannon->m_Pos.x;
    result.y = cannon->m_Pos.y;
    result.health = cannon->GetHitPoint();
    result.maxhealth = cannon->GetMaxHitPoint();
    result.type = static_cast<u8>(cannon->m_Num);
    result.side = static_cast<u8>(cannon->GetSide());
    result.cannon_state = static_cast<u8>(cannon->m_CurrState);
    result.parent_building_nid = cannon->m_ParentBuilding ? cannon->m_ParentBuilding->m_NID : 0u;
    result.place = cannon->m_Place;
    result.rotation = pack_rotation(cannon->m_Angle);
    result.turret_yaw = pack_rotation(cannon->turret_yaw());
    result.pitch = pack_rotation(cannon->m_AngleX);
    return result;
}

void BuildingSnapshot::serialize_to_bitstream(BitWriter &writer) const
{
    writer.write_u32(nid);
    writer.write_u8(type);
    writer.write_u8(side);
    writer.write_f32(health);
    writer.write_u32(capturing_progress);
    writer.write_u32(capturing_color);
    writer.write_u8(base_state);
    writer.write_u8(base_floor_q);
    writer.write_u8(spawning_bot);
    writer.write_u8(build_stack_count);
    writer.write_u16(build_timer);
    for (u8 i = 0; i < build_stack_count; i++)
        write_build_stack_entry(writer, build_stack[i]);
}

BuildingSnapshot BuildingSnapshot::deserialize_from_bitstream(BitReader &reader)
{
    BuildingSnapshot result{};

    result.nid = reader.read_u32();
    result.type = static_cast<EBuildingType>(reader.read_u8());
    result.side = reader.read_u8();
    result.health = reader.read_f32();
    result.capturing_progress = reader.read_u32();
    result.capturing_color = reader.read_u32();
    result.base_state = reader.read_u8();
    result.base_floor_q = reader.read_u8();
    result.spawning_bot = reader.read_u8();
    result.build_stack_count = reader.read_u8();
    result.build_timer = reader.read_u16();
    for (u8 i = 0; i < result.build_stack_count && i < kMaxBuildStackSnapshot; i++)
        result.build_stack[i] = read_build_stack_entry(reader);

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

    writer.write_u32(buildings.size());
    for (const auto& [Id, b_snapshot] : buildings) {
        b_snapshot.serialize_to_bitstream(writer);
    }

    writer.write_u32(turrets.size());
    for (const auto& [Id, t_snapshot] : turrets) {
        t_snapshot.serialize_to_bitstream(writer);
    }
}

namespace {

constexpr u32 kMaxSnapshotSides = 16;
constexpr u32 kMaxSnapshotRobots = 512;
constexpr u32 kMaxSnapshotBuildings = 512;
constexpr u32 kMaxSnapshotTurrets = 512;

u32 clamp_snapshot_count(u32 count, u32 max_count) { return count > max_count ? max_count : count; }

}  // namespace

WorldSnapshot WorldSnapshot::deserialize_from_bitstream(BitReader &reader)
{
    WorldSnapshot result{};

    result.frame = reader.read_u32();

    const u32 sides_count = clamp_snapshot_count(reader.read_u32(), kMaxSnapshotSides);
    for (u32 i = 0; i < sides_count; i++) {
        SideSnapshot ss = SideSnapshot::deserialize_from_bitstream(reader);
        result.sides.emplace(ss.side, ss);
    }

    const u32 rob_count = clamp_snapshot_count(reader.read_u32(), kMaxSnapshotRobots);
    for (u32 i = 0; i < rob_count; i++) {
        RobotSnapshot rs = RobotSnapshot::deserialize_from_bitstream(reader);
        result.robots.emplace(rs.nid, rs);
    }

    const u32 b_count = clamp_snapshot_count(reader.read_u32(), kMaxSnapshotBuildings);
    for (u32 i = 0; i < b_count; i++) {
        BuildingSnapshot bs = BuildingSnapshot::deserialize_from_bitstream(reader);
        result.buildings.emplace(bs.nid, bs);
    }

    const u32 t_count = clamp_snapshot_count(reader.read_u32(), kMaxSnapshotTurrets);
    for (u32 i = 0; i < t_count; i++) {
        TurretSnapshot ts = TurretSnapshot::deserialize_from_bitstream(reader);
        result.turrets.emplace(ts.nid, ts);
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
            CMatrixBuilding *building = obj->AsBuilding();
            u8 base_floor_q = 0;
            u8 base_state = static_cast<u8>(BASE_CLOSED);
            u8 spawning_bot = 0;
            if (building->IsBase()) {
                base_state = static_cast<u8>(building->m_State);
                base_floor_q = static_cast<u8>(std::min(255.f, std::max(0.f, building->m_BaseFloor * 255.f)));
                spawning_bot = building->IsSpawningBot() ? 1u : 0u;
            }

            u8 build_stack_count = 0;
            u16 build_timer = 0;
            BuildStackEntrySnapshot build_stack[kMaxBuildStackSnapshot]{};
            if (building->IsBase()) {
                build_timer = static_cast<u16>(std::min(65535, building->m_BS.GetBuildTimerMs()));
                for (CMatrixMapStatic *item = building->m_BS.GetTopItem();
                     item && build_stack_count < kMaxBuildStackSnapshot;
                     item = item->m_NextStackItem) {
                    if (item->IsRobot())
                        build_stack[build_stack_count++] =
                            BuildStackEntrySnapshot::from_robot(item->AsRobot());
                }
            }

            BuildingSnapshot building_snap{
                building->m_NID,
                building->m_Kind,
                static_cast<u8>(building->m_Side),
                building->IsLiveBuilding() ? building->GetHitPoint() : 0.f,
                static_cast<u8>(building->m_TrueColor.m_ColoredCnt),
                building->m_TrueColor.m_Color,
                base_state,
                base_floor_q,
                spawning_bot,
                build_stack_count,
                build_timer,
            };
            for (u8 i = 0; i < build_stack_count; ++i)
                building_snap.build_stack[i] = build_stack[i];
            result.buildings.emplace(building->m_NID, building_snap);
        }
        else if (obj->IsLiveRobot())
        {
            CMatrixRobotAI *robot = obj->AsRobot();
            result.robots.emplace(
                robot->m_NID,
                RobotSnapshot::from_robot(robot)
            );
        }
        else if (obj->IsCannon())
        {
            CMatrixCannon *cannon = obj->AsCannon();
            if (cannon->m_CurrState != CANNON_DIP)
                result.turrets.emplace(cannon->m_NID, TurretSnapshot::from_cannon(cannon));
        }
    }

#ifdef PROFILING
    std::cout << "= Capture world snapshot elapsed time: " << cws_stopwatch.elapsed_ms() << "\n";
#endif
    return result;
}
