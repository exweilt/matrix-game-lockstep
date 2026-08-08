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
    f32 rotation;
    f32 hull_rotation;
    u8 animation;
    u8 side;
    // D3DXVECTOR3 target;
    // bool is_firing;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(nid), CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(health), CEREAL_NVP(maxhealth),
            CEREAL_NVP(chassis), CEREAL_NVP(hull),CEREAL_NVP(head), CEREAL_NVP(weapon_cnt), CEREAL_NVP(weapons), CEREAL_NVP(rotation), CEREAL_NVP(hull_rotation), CEREAL_NVP(side));
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

uint8_t pack_rotation(float radians);

f32 unpack_rotation(uint8_t quantized);

struct EventFire
{
    u32 nid;
    D3DXVECTOR3 target_pos;
    u32 frame;
    u8 weapons;
    u8 precise_time; // exact moment between 2 frames event happened.

    EventFire(): nid(0), frame(0), weapons(0), precise_time(0) {}

    EventFire(u32 n, D3DXVECTOR3 d, u8 w, u32 f, u8 p) : nid(n), target_pos(d), weapons(w), frame(f), precise_time(p) {}

    void serialize_to_bitstream(BitWriter &writer) const;
    static EventFire deserialize_from_bitstream(BitReader &reader);
};

struct EventFireComparator
{
    bool operator()(const EventFire& a, const EventFire& b) const
    {
        if (a.frame != b.frame)
            return a.frame > b.frame;

        return a.precise_time > b.precise_time;
    }
};

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

