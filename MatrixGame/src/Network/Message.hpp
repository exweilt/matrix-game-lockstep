#pragma once

#include <winsock2.h>

#include <string>
#include <variant>
#include <vector>

#include "BitStream.hpp"
#include "Command.hpp"

#include <cassert>

#include "Snapshot.hpp"

constexpr int MAX_ROBOTS_PER_COMMAND = 16;

// namespace network
// {

// TODO: consider making different message types the same if they carry the same memory layout. e.g. some both have only one int.
//          remove hell a lot of boilerplate
enum class MessageType : u8
{
    NONE            = 0,
    // COMMAND_BATCH    = 1,
    READY,
    START,
    WORLD_SNAPSHOT,
    COMMAND_MOVE,
    COMMAND_BUILD,
    COMMAND_CAPTURE,
    INFO,
    SAY,
    JOIN,
    PING,
    PONG,
    DESYNC, //happened
    CHECKSUM,
    STATE_REPORT,
    EVENTS,
};

struct MessageEventsParams
{
    // u32 target_frame;
    std::vector<EventFire> events;
    // std::vector<RobotSnapshot> robots;

    MessageEventsParams(): events() {}
    MessageEventsParams(const std::vector<EventFire> &e) : events(e) {}
    ~MessageEventsParams() {}


    void serialize_to_bitstream(BitWriter &writer) const;
    static MessageEventsParams deserialize_from_bitstream(BitReader &reader);
};

struct MessageWorldSnapshotParams
{
    // u32 target_frame;
    WorldSnapshot ws;
    // std::vector<RobotSnapshot> robots;

    MessageWorldSnapshotParams(): ws() {}
    MessageWorldSnapshotParams(WorldSnapshot _ws) : ws(_ws) {}
    ~MessageWorldSnapshotParams() {}


    void serialize_to_bitstream(BitWriter &writer) const;
    static MessageWorldSnapshotParams deserialize_from_bitstream(BitReader &reader);
};

struct MessageJoinParams
{
    u8 player_side;
    std::string username;

    MessageJoinParams(u8 side = 0, std::string name = "Greph") : player_side(side), username(name) {}
    ~MessageJoinParams() {}

    void serialize_to_bitstream(BitWriter &writer) const;
    static MessageJoinParams deserialize_from_bitstream(BitReader &reader);
};

struct MessageCommandMoveParams
{
    u8 number_of_robots;
    u32 robot_nid[MAX_ROBOTS_PER_COMMAND];
    D3DXVECTOR3 target_pos;

    MessageCommandMoveParams() : number_of_robots(0), robot_nid(0) {};
    MessageCommandMoveParams(const u32 r_nid, const D3DXVECTOR3 &dest);
    MessageCommandMoveParams(std::vector<u32> robots_nid, const D3DXVECTOR3 &dest)
    {
        assert(robots_nid.size() <= MAX_ROBOTS_PER_COMMAND);
        number_of_robots = robots_nid.size();
        memset(robot_nid, 0, MAX_ROBOTS_PER_COMMAND * sizeof(u32));
        memcpy(robot_nid, robots_nid.data(), robots_nid.size() * sizeof(u32));
        target_pos = dest;
    };

    // u32 get_serialized_size() const
    // {
    //     return sizeof(number_of_robots) + sizeof(robot_nid) + sizeof(target_pos);
    // }
    void serialize_to_bitstream(BitWriter &writer) const;
    static MessageCommandMoveParams deserialize_from_bitstream(BitReader &reader);

    void execute_for_side(u32 side_id);
    void execute();

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(number_of_robots), CEREAL_NVP(robot_nid), CEREAL_NVP(target_pos));
    }
};

struct MessageCommandBuildParams
{
    ERobotUnitKind chassis;
    ERobotUnitKind hull;
    ERobotUnitKind head;
    ERobotUnitKind weapons[5];
    u8 robot_count; // 0-...
    u32 target_base_nid; // Base NID to build at.

    MessageCommandBuildParams() : chassis(), hull(), head(), weapons{}, robot_count(0), target_base_nid(0) {};
    // CommandBuildParams(ERobotUnitKind ch, ERobotUnitKind hu, ERobotUnitKind he, ERobotUnitKind *wp, u8 cnt, u32 base)
    //     : chassis(ch), hull(hu), head(he), robot_count(cnt), target_base_nid(base)
    // {
    //     for (u32 i = 0; i < MAX_WEAPON_CNT; i++)
    //     {
    //         this->weapons[i] = wp[i];
    //     }
    // };

#define MAX_WEAPON_CNT   5
    MessageCommandBuildParams(ERobotUnitKind ch, ERobotUnitKind hu, ERobotUnitKind he, const std::vector<ERobotUnitKind> &wp, u8 cnt, u32 base)
    : chassis(ch), hull(hu), head(he), robot_count(cnt), target_base_nid(base)
    {
        for (u32 i = 0; i < MAX_WEAPON_CNT; i++)
        {
            this->weapons[i] = wp[i];
        }
    };
#undef MAX_WEAPON_CNT

    void serialize_to_bitstream(BitWriter &writer) const;
    static MessageCommandBuildParams deserialize_from_bitstream(BitReader &reader);

    void execute_for_side();
    // void execute();

    // template <class Archive>
    // void serialize(Archive& ar) {
    //     ar(CEREAL_NVP(number_of_robots), CEREAL_NVP(robot_nid), CEREAL_NVP(target_pos));
    // }
};

struct MessageCommandCaptureParams
{
    u8 number_of_robots;
    u32 robot_nid[MAX_ROBOTS_PER_COMMAND];
    u32 target_nid;

    MessageCommandCaptureParams() : number_of_robots(0), robot_nid(0), target_nid(0) {};
    // MessageCommandCaptureParams(const u32 r_nid, const D3DXVECTOR3 &dest);
    MessageCommandCaptureParams(const std::vector<u32> &robots_nid, u32 target_NID)
    {
        assert(robots_nid.size() <= MAX_ROBOTS_PER_COMMAND);
        number_of_robots = robots_nid.size();
        memset(robot_nid, 0, MAX_ROBOTS_PER_COMMAND * sizeof(u32));
        memcpy(robot_nid, robots_nid.data(), robots_nid.size() * sizeof(u32));
        target_nid = target_NID;
    };

    void serialize_to_bitstream(BitWriter &writer) const;
    static MessageCommandCaptureParams deserialize_from_bitstream(BitReader &reader);

    void execute_for_side(u32 side_id);
    void execute();

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(number_of_robots), CEREAL_NVP(robot_nid), CEREAL_NVP(target_nid));
    }
};

struct MessageChecksumParams
{
    u32 target_frame;
    u64 checksum;

    MessageChecksumParams(u32 target_frame, u64 checksum) : target_frame(target_frame), checksum(checksum) {}
    ~MessageChecksumParams() {}

    // u32 get_serialized_size() const { return sizeof(u32) + sizeof(u64); }
    void serialize_to_bitstream(BitWriter &writer) const;
    static MessageChecksumParams deserialize_from_bitstream(BitReader &reader);
};

enum class ReportType {
    DEFAULT = 0,
    WORLD_STATE = 1,
    CODE_TRACE_DESYNC = 2,
    COMMAND_BATCH = 3,
};
struct MessageReportParams
{
    u8 player_side;
    ReportType type;    // optional
    std::string data;

    MessageReportParams(u8 side, std::string dat = "") : player_side(side), type(ReportType::DEFAULT), data(dat) {}
    MessageReportParams(u8 side, std::string dat, ReportType typ) : player_side(side), type(typ), data(dat) {}
    ~MessageReportParams() {}

    void serialize_to_bitstream(BitWriter &writer) const;
    static MessageReportParams deserialize_from_bitstream(BitReader &reader);
};

struct MessageDesyncParams
{
    u32 target_frame;

    MessageDesyncParams(u32 target_frame) : target_frame(target_frame) {}
    ~MessageDesyncParams() {}

    void serialize_to_bitstream(BitWriter &writer) const;
    static MessageDesyncParams deserialize_from_bitstream(BitReader &reader);
};

/**
 * @brief The content of a packet to be transmitted using ENet.
 *
 * P.S. The reason why polymorphism using virtual methods and overrides was not chosen is because this introduces
 *      an extra overhead removing the POD status and bloating up with the VTables. In C++20 we could use "concepts"
 *      maybe as a more concise zero-cost abstraction workaround. For pre C++20 void_t/SFINAE can be considered,
 *      but I doubt if it is better :p Probably this is how it would be implemented in pure C. I considered using
 *      std::variant but didn't like it.
 */
struct Message
{
    MessageType type;
    union
    {
        MessageWorldSnapshotParams world_snapshot;
        MessageJoinParams join;
        MessageChecksumParams checksum;
        MessageReportParams report;
        MessageDesyncParams desync;
        MessageCommandMoveParams command_move;
        MessageCommandBuildParams command_build;
        MessageEventsParams events;
        MessageCommandCaptureParams capture;
    };

    Message()                               : type(MessageType::NONE) {};
    Message(MessageType type)               : type(type) {}
    Message(MessageWorldSnapshotParams cb)   : type(MessageType::WORLD_SNAPSHOT), world_snapshot(cb) {};
    Message(MessageJoinParams j)            : type(MessageType::JOIN),          join(j)     {};
    Message(MessageChecksumParams ch)       : type(MessageType::CHECKSUM),      checksum(ch)     {};
    Message(MessageReportParams r)         : type(MessageType::STATE_REPORT),      report(r)     {};
    Message(MessageDesyncParams d)         : type(MessageType::DESYNC),      desync(d)     {};
    Message(MessageCommandMoveParams m)         : type(MessageType::COMMAND_MOVE),      command_move(m)     {};
    Message(MessageCommandBuildParams m)         : type(MessageType::COMMAND_BUILD),      command_build(m)     {};
    Message(MessageEventsParams e)         : type(MessageType::EVENTS),      events(e)     {};
    Message(MessageCommandCaptureParams x)         : type(MessageType::COMMAND_CAPTURE),      capture(x)     {};

    ~Message()
    {
        switch (type)
        {
            case MessageType::WORLD_SNAPSHOT:
                world_snapshot.~MessageWorldSnapshotParams();
                break;
            case MessageType::COMMAND_MOVE:
                command_move.~MessageCommandMoveParams();
                break;
            case MessageType::COMMAND_BUILD:
                command_build.~MessageCommandBuildParams();
                break;
            case MessageType::JOIN:
                join.~MessageJoinParams();
                break;
            case MessageType::CHECKSUM:
                checksum.~MessageChecksumParams();
                break;
            case MessageType::STATE_REPORT:
                report.~MessageReportParams();
                break;
            case MessageType::DESYNC:
                desync.~MessageDesyncParams();
                break;
            case MessageType::EVENTS:
                events.~MessageEventsParams();
                break;
            case MessageType::COMMAND_CAPTURE:
                capture.~MessageCommandCaptureParams();
                break;
            default:;
        }
    }

    void serialize_to_bitstream(BitWriter &writer)
    {
        writer.write_u8(static_cast<u8>(type)); // Write down the type tag

        // Write down the message itself
        switch (type)
        {
            case MessageType::WORLD_SNAPSHOT:   world_snapshot   .serialize_to_bitstream(writer); break;
            case MessageType::COMMAND_MOVE:     command_move     .serialize_to_bitstream(writer); break;
            case MessageType::COMMAND_BUILD:     command_build     .serialize_to_bitstream(writer); break;
            case MessageType::JOIN:             join            .serialize_to_bitstream(writer); break;
            case MessageType::CHECKSUM:         checksum        .serialize_to_bitstream(writer); break;
            case MessageType::START:            break;
            case MessageType::DESYNC:           desync          .serialize_to_bitstream(writer); break;
            case MessageType::STATE_REPORT:     report          .serialize_to_bitstream(writer); break;
            case MessageType::EVENTS:           events          .serialize_to_bitstream(writer); break;
            case MessageType::COMMAND_CAPTURE:  capture          .serialize_to_bitstream(writer); break;
            default:                            assert(false);
        }
    }

    static Message deserialize_from_bitstream(BitReader &reader)
    {
        switch (static_cast<MessageType>(reader.read_u8()))
        {
            case MessageType::WORLD_SNAPSHOT:
                return MessageWorldSnapshotParams::  deserialize_from_bitstream(reader);
            case MessageType::COMMAND_MOVE:
                return MessageCommandMoveParams     ::  deserialize_from_bitstream(reader);
            case MessageType::COMMAND_BUILD:
                return MessageCommandBuildParams     ::  deserialize_from_bitstream(reader);
            case MessageType::JOIN:
                return MessageJoinParams::          deserialize_from_bitstream(reader);
            case MessageType::START:
                return Message(MessageType::START);
            case MessageType::DESYNC:
                return MessageDesyncParams::        deserialize_from_bitstream(reader);
            case MessageType::STATE_REPORT:
                return MessageReportParams::        deserialize_from_bitstream(reader);
            case MessageType::CHECKSUM:
                return MessageChecksumParams::      deserialize_from_bitstream(reader);
            case MessageType::EVENTS:
                return MessageEventsParams::        deserialize_from_bitstream(reader);
            case MessageType::COMMAND_CAPTURE:
                return MessageCommandCaptureParams::deserialize_from_bitstream(reader);
            default:
                assert(false);
        }
    }

    // void serialize_to_buffer(u8* buffer)
    // {
    //     // Write down the type tag
    //     buffer[0] = static_cast<u8>(type);
    //
    //     // Write down the message itself
    //     switch (type)
    //     {
    //         case MessageType::COMMAND_BATCH: command_batch.serialize_to_buffer(buffer + 1); break;
    //         case MessageType::JOIN:         join.serialize_to_buffer(buffer + 1);           break;
    //         case MessageType::CHECKSUM:     checksum.serialize_to_buffer(buffer + 1);       break;
    //         case MessageType::START:        break;
    //         case MessageType::DESYNC:       break;
    //         default:                        assert(false);
    //     }
    // }

    // static Message deserialize_from_buffer(u8* buffer /*, size_t size */)
    // {
    //     switch (static_cast<MessageType>(buffer[0]))
    //     {
    //         case MessageType::COMMAND_BATCH:
    //             return MessageCommandBatchParams::deserialize_from_buffer(buffer + 1);
    //         case MessageType::JOIN:
    //             return MessageJoinParams::deserialize_from_buffer(buffer + 1);
    //         case MessageType::START:
    //                 return Message(MessageType::START);
    //         case MessageType::DESYNC:
    //             return Message(MessageType::DESYNC);
    //         case MessageType::CHECKSUM:
    //             return MessageChecksumParams::deserialize_from_buffer(buffer + 1);
    //         default:
    //             assert(false);
    //     }
    // }
};
// }









// // WARNING: must be the same order as enum "MessageType" entries!
// using Message = std::variant<
//     MessageCommandBatchParams
//     // MessageJoinParams
// >;

// u32 get_serialized_size()
// {
//     // Type of message.index() 1 byte + message itself
//     return sizeof(u8) + std::visit(
//         [](auto &msg) -> u32 { return msg.get_size(); },
//         message
//     );
// }
//
// void serialize_message_to_buffer(Message message, u8* buffer)
// {
//     // Write down the type
//     buffer[0] = static_cast<u8>(message.index());
//
//     // Write down the message itself
//     std::visit(
//         [buffer](auto &msg) -> void { msg.serialize_to_buffer(buffer); },
//         message
//     );
// }
//
// static Message deserialize_message_from_buffer(u8* buffer /*, size_t size */)
// {
//     switch (static_cast<MessageType>(buffer[0]))
//     {
//     case MessageType::CommandBatch:
//         return MessageCommandBatchParams::deserialize_from_buffer(static_cast<u8*>(buffer + 1) /*, size - 1 */);
//     case MessageType::Join:
//         return MessageJoinParams::deserialize_from_buffer(static_cast<u8*>(buffer + 1) /*, size - 1 */);
//     default:
//         assert(false);
//     }
// }

// {
//     network::CommandMoveParams m1 = network::CommandMoveParams(10, D3DXVECTOR3 {100, 5, 100});
//     network::CommandMoveParams m2 = network::CommandMoveParams(11, D3DXVECTOR3 {100, 5, 100});
//     network::MessageCommandBatchParams msg {0, 1};
//     msg.commands.push_back(m1);
//     msg.commands.push_back(m2);
//
//     u32 sz = network::get_serialized_message_size(msg);
//
//     void *from = malloc(sz);
//     network::serialize_message_to_buffer(msg, static_cast<u8*>(from));
//
//     void *to = malloc(sz);
//     memcpy(to, from, sz);
//
//     network::Message msg2 = network::deserialize_message_from_buffer(to);
//     network::MessageCommandBatchParams com_batch2 = std::get<network::MessageCommandBatchParams>(msg2);
//     return;
// }