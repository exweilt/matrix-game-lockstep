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
class CMatrixCannon;
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
    u8 robot_state;
    u32 spawn_base_nid;
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

inline constexpr u8 kMaxBuildStackSnapshot = 6;

struct BuildStackEntrySnapshot
{
    u8 chassis{};
    u8 hull{};
    u8 head{};
    u8 weapon_cnt{};
    u8 weapons[5]{};

    static BuildStackEntrySnapshot from_robot(CMatrixRobotAI *robot);
};

struct TurretSnapshot
{
    u32 nid;
    f32 x;
    f32 y;
    f32 health;
    f32 maxhealth;
    u8 type;
    u8 side;
    u8 cannon_state;
    u32 parent_building_nid;
    i32 place;
    u8 rotation;
    u8 turret_yaw;
    u8 pitch;

    template <class Archive>
    void serialize(Archive &ar) {
        ar(CEREAL_NVP(nid), CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(health), CEREAL_NVP(maxhealth),
           CEREAL_NVP(type), CEREAL_NVP(side), CEREAL_NVP(cannon_state), CEREAL_NVP(parent_building_nid),
           CEREAL_NVP(place), CEREAL_NVP(rotation), CEREAL_NVP(turret_yaw), CEREAL_NVP(pitch));
    }

    void serialize_to_bitstream(BitWriter &writer) const;
    static TurretSnapshot deserialize_from_bitstream(BitReader &reader);

    static TurretSnapshot from_cannon(CMatrixCannon *cannon);
};

struct BuildingSnapshot
{
    u32 nid;
    EBuildingType type;
    u8 side;
    f32 health;
    u8 capturing_progress;
    u32 capturing_color;
    u8 base_state;
    u8 base_floor_q;
    u8 spawning_bot;
    u8 build_stack_count;
    u16 build_timer;
    BuildStackEntrySnapshot build_stack[kMaxBuildStackSnapshot]{};

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(type), CEREAL_NVP(side), CEREAL_NVP(health));
    }

    void serialize_to_bitstream(BitWriter &writer) const;
    static BuildingSnapshot deserialize_from_bitstream(BitReader &reader);
};

// Stores the
struct WorldSnapshot
{
    u32 frame;
    std::map<u8, SideSnapshot>        sides;
    std::map<u32, RobotSnapshot>      robots;
    std::map<u32, BuildingSnapshot>   buildings;
    std::map<u32, TurretSnapshot>     turrets;

    std::string to_json_string() const;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(frame), CEREAL_NVP(sides), CEREAL_NVP(robots), CEREAL_NVP(buildings), CEREAL_NVP(turrets));
    }

    void serialize_to_bitstream(BitWriter &writer) const;
    static WorldSnapshot deserialize_from_bitstream(BitReader &reader);
};

WorldSnapshot capture_world_snapshot();

uint8_t pack_rotation(float radians);

f32 unpack_rotation(uint8_t quantized);

enum class EventType : u8
{
    NONE = 0,
    FIRE = 1,
    SOUND = 2,
};

// Shot payload. Frame and precise_time live on Event, which is what the queue orders by.
struct EventFire
{
    u32 nid;
    D3DXVECTOR3 target_pos;
    u8 weapons;

    EventFire() : nid(0), target_pos(0, 0, 0), weapons(0) {}
    EventFire(u32 n, const D3DXVECTOR3 &d, u8 w) : nid(n), target_pos(d), weapons(w) {}

    void serialize_to_bitstream(BitWriter &writer) const;
    static EventFire deserialize_from_bitstream(BitReader &reader);
};

// sound is an ESound stored as u16. positional == 0 plays it as a global stinger.
struct SoundEvent
{
    u16 sound;
    u8 positional;
    u8 layer;
    D3DXVECTOR3 pos;

    SoundEvent() : sound(0), positional(0), layer(0), pos(0, 0, 0) {}
    SoundEvent(u16 s, u8 l = 0) : sound(s), positional(0), layer(l), pos(0, 0, 0) {}
    SoundEvent(u16 s, const D3DXVECTOR3 &p, u8 l = 0) : sound(s), positional(1), layer(l), pos(p) {}

    void serialize_to_bitstream(BitWriter &writer) const;
    static SoundEvent deserialize_from_bitstream(BitReader &reader);
};

struct Event
{
    EventType type;
    u32 frame;
    u8 precise_time; // exact moment between 2 frames the event happened
    union
    {
        EventFire fire;
        SoundEvent sound;
    };

    Event() : type(EventType::NONE), frame(0), precise_time(0), fire() {}
    Event(const EventFire &payload, u32 f, u8 p) : type(EventType::FIRE), frame(f), precise_time(p), fire(payload) {}
    Event(const SoundEvent &payload, u32 f, u8 p)
        : type(EventType::SOUND), frame(f), precise_time(p), sound(payload) {}

    static Event Fire(u32 nid, const D3DXVECTOR3 &target, u8 weapons, u32 frame, u8 precise_time)
    {
        return Event(EventFire(nid, target, weapons), frame, precise_time);
    }

    static Event Sound(u16 sound, u32 frame, u8 precise_time, u8 layer = 0)
    {
        return Event(SoundEvent(sound, layer), frame, precise_time);
    }

    static Event SoundAt(u16 sound, const D3DXVECTOR3 &pos, u32 frame, u8 precise_time, u8 layer = 0)
    {
        return Event(SoundEvent(sound, pos, layer), frame, precise_time);
    }

    void serialize_to_bitstream(BitWriter &writer) const;
    static Event deserialize_from_bitstream(BitReader &reader);
};

struct EventComparator
{
    bool operator()(const Event &a, const Event &b) const
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

