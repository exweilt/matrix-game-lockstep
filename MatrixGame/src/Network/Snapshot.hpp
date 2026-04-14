#pragma once
#include <map>

// #include "./../MatrixObjectBuilding.hpp"
// #include "./../MatrixSide.hpp"
#include "Types.hpp"
#include "cereal/cereal.hpp"
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>

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
};

struct RobotSnapshot
{
    f32 x;
    f32 y;
    f32 health;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(health));
    }
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
    std::map<u32, BuildingSnapshot>   buildings;

    std::string to_json_string();

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(frame), CEREAL_NVP(sides), CEREAL_NVP(robots), CEREAL_NVP(buildings));
    }

    u64 hash();
};

WorldSnapshot capture_world_snapshot();

