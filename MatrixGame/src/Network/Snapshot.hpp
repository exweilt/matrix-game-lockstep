#pragma once
#include <map>

// #include "./../MatrixObjectBuilding.hpp"
// #include "./../MatrixSide.hpp"
#include "Types.hpp"
#include "cereal/cereal.hpp"
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>

#include "BitStream.hpp"

class CMatrixRobotAI;
enum ERobotUnitKind : unsigned int;
enum ESideStatus : u32;

struct SideSnapshot
{
    u8 side;
    ESideStatus status;
    i32 titanium;
    i32 electronics;
    i32 energy;
    i32 plasma;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(side), CEREAL_NVP(status), CEREAL_NVP(titanium), CEREAL_NVP(electronics),
            CEREAL_NVP(energy), CEREAL_NVP(plasma));
    }

    void serialize_to_bitstream(BitWriter &writer) const;
    static SideSnapshot deserialize_from_bitstream(BitReader &reader);
};

struct RobotSnapshot
{
    u32 nid;
    f32 x;
    f32 y;
    f32 health;
    f32 maxhealth;
    u8 chassis;
    u8 hull;
    u8 head;
    u8 weapon_cnt;
    u8 weapons[5]{};
    u8 rotation;
    u8 hull_rotation;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(nid), CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(health), CEREAL_NVP(maxhealth),
            CEREAL_NVP(chassis), CEREAL_NVP(hull), CEREAL_NVP(weapon_cnt), CEREAL_NVP(weapons), CEREAL_NVP(rotation), CEREAL_NVP(hull_rotation));
    }

    void serialize_to_bitstream(BitWriter &writer) const;
    static RobotSnapshot deserialize_from_bitstream(BitReader &reader);

    static RobotSnapshot from_robot(CMatrixRobotAI *robot);
};

enum EBuildingType : u32;
struct BuildingSnapshot
{
    EBuildingType type;
    u8 side;
    f32 health;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(type), CEREAL_NVP(side), CEREAL_NVP(health));
    }
};

// Stores the
struct WorldSnapshot
{
    u32 frame;
    std::map<u8, SideSnapshot>        sides;
    std::map<u32, RobotSnapshot>      robots;

    std::string to_json_string() const;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(frame), CEREAL_NVP(sides), CEREAL_NVP(robots));
    }

    void serialize_to_bitstream(BitWriter &writer) const;
    static WorldSnapshot deserialize_from_bitstream(BitReader &reader);
};

WorldSnapshot capture_world_snapshot();




// // Stores the
// struct WorldSnapshot
// {
//     u32 frame;
//     std::map<u8, SideSnapshot>        sides;
//     std::map<u32, RobotSnapshot>      robots;
//     std::map<u32, BuildingSnapshot>   buildings;
//
//     std::string to_json_string();
//
//     template <class Archive>
//     void serialize(Archive& ar) {
//         ar(CEREAL_NVP(frame), CEREAL_NVP(sides), CEREAL_NVP(robots), CEREAL_NVP(buildings));
//     }
//
//     // u64 hash();
// };

// WorldSnapshot capture_world_snapshot();

