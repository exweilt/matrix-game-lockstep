#pragma once
#include <winsock2.h>

#include "Command.hpp"
#include "MatrixRobot.hpp"

#include <d3dx9math.h>

#include "Types.hpp"

#include <cassert>
// #include <variant>


enum ERobotUnitKind : unsigned int;

namespace network
{
    // // WARNING: mind the order!
    enum class CommandType : u8
    {
        NONE    = 0,
        MOVE    = 1,
        ATTACK  = 2,
        CAPTURE = 3,
        BUILD   = 4
    };

    struct CommandMoveParams
    {
        u32 robot_nid;
        D3DXVECTOR3 target_pos;

        CommandMoveParams()                                         : robot_nid(0) {};
        CommandMoveParams(const u32 r_nid, const D3DXVECTOR3 &dest) : robot_nid(r_nid), target_pos(dest) {}

        u32 get_serialized_size() const
        {
            return sizeof(robot_nid) + sizeof(target_pos);
        }
        void serialize_to_buffer(u8* buffer) const;
        static CommandMoveParams deserialize_from_buffer(const u8 * buffer);
    };

    struct CommandCaptureParams
    {
        u32 robot_nid;
        u32 target_nid; // Target NID to capture

        CommandCaptureParams()            : robot_nid(0), target_nid(0) {};
        CommandCaptureParams(const u32 r_nid, const u32 target) : robot_nid(r_nid), target_nid(target) {}

        u32 get_serialized_size() const
        {
            return sizeof(robot_nid) + sizeof(target_nid);
        }
        void serialize_to_buffer(u8* buffer) const;
        static CommandCaptureParams deserialize_from_buffer(const u8* buffer);
    };

    struct CommandAttackParams
    {
        u32 robot_nid;
        u32 target_nid; // Target NID to attack

        CommandAttackParams() : robot_nid(0), target_nid(0) {};
        CommandAttackParams(const u32 r_nid, const u32 target) : robot_nid(r_nid), target_nid(target) {}

        u32 get_serialized_size() const
        {
            return sizeof(robot_nid) + sizeof(target_nid);
        }
        void serialize_to_buffer(u8* buffer) const;
        static CommandAttackParams deserialize_from_buffer(const u8* buffer);
    };

    struct CommandBuildParams
    {
        ERobotUnitKind chassis;
        ERobotUnitKind hull;
        ERobotUnitKind head;
        ERobotUnitKind weapons[5];
        u8 robot_count; // 0-...
        u32 target_base_nid; // Base NID to build at.

        CommandBuildParams() : chassis(), hull(), head(), weapons{}, robot_count(0), target_base_nid(0) {};
        CommandBuildParams(ERobotUnitKind ch, ERobotUnitKind hu, ERobotUnitKind he, ERobotUnitKind wp[5], u8 cnt, u32 base)
            : chassis(ch), hull(hu), head(he), robot_count(cnt), target_base_nid(base)
        {
            for (u32 i = 0; i < MAX_WEAPON_CNT; i++)
            {
                this->weapons[i] = wp[i];
            }
        };

        u32 get_serialized_size() const
        {
            return sizeof(chassis) + sizeof(hull) + sizeof(head) + sizeof(weapons) + sizeof(robot_count) + sizeof(target_base_nid);
        }
        void serialize_to_buffer(u8* buffer) const;
        static CommandBuildParams deserialize_from_buffer(const u8* buffer);
    };

    struct Command
    {
        CommandType type;
        union
        {
            CommandMoveParams move;
            CommandCaptureParams capture;
            CommandAttackParams attack;
            CommandBuildParams build;
        };

        Command()                           : type(CommandType::NONE) {};
        Command(CommandMoveParams mv)       : type(CommandType::MOVE),      move(mv) {};
        Command(CommandAttackParams atk)    : type(CommandType::ATTACK),    attack(atk) {};
        Command(CommandCaptureParams cpt)   : type(CommandType::CAPTURE),   capture(cpt) {};
        Command(CommandBuildParams bld)     : type(CommandType::BUILD),     build(bld) {};


        u32 get_serialized_size() const
        {
            switch (type)
            {
                case CommandType::MOVE:     return sizeof(u8) + move.get_serialized_size();
                case CommandType::CAPTURE:  return sizeof(u8) + capture.get_serialized_size();
                case CommandType::ATTACK:   return sizeof(u8) + attack.get_serialized_size();
                case CommandType::BUILD:    return sizeof(u8) + build.get_serialized_size();
                default:                    return 0;
            }
        }

        void serialize_to_buffer(u8* buffer) const;
        static Command deserialize_from_buffer(const u8* buffer);
    };

    inline void serialize_vector3_to_buffer(const D3DXVECTOR3 &vector, u8* buffer)
    {
        // f32 are reinterpreted as u32
        const u32 be_points[3]
        {
            htonl(std::bit_cast<u32>(vector.x)),
            htonl(std::bit_cast<u32>(vector.y)),
            htonl(std::bit_cast<u32>(vector.z))
        };

        memcpy(buffer, &be_points, sizeof(be_points));
    }

    /**
     *
     * @param buffer Should be 4 * 3 = 12 bytes long
     */
    inline D3DXVECTOR3 deserialize_vector3_from_buffer(const u8* buffer)
    {
        D3DXVECTOR3 result;

        memcpy(&result.x, buffer, sizeof(FLOAT));
        memcpy(&result.y, buffer + 4, sizeof(FLOAT));
        memcpy(&result.z, buffer + 8, sizeof(FLOAT));

        result.x = std::bit_cast<f32>( ntohl(std::bit_cast<u32>(result.x)) );
        result.y = std::bit_cast<f32>( ntohl(std::bit_cast<u32>(result.y)) );
        result.z = std::bit_cast<f32>( ntohl(std::bit_cast<u32>(result.z)) );

        return result;
    }

    /**
     * Important: buffer is MUTATED!
     */
    inline void serialize_string_to_buffer(const std::string &str, u8* &buffer)
    {
        const u32 be_len = htonl(str.size());
        memcpy(buffer, &be_len, sizeof(be_len));
        buffer += sizeof(be_len);

        if (!str.empty())
        {
            memcpy(buffer, &str[0], str.size());
        }
        buffer += str.size();
    }

    inline std::string deserialize_string_from_buffer(const u8* buffer)
    {
        std::string result;

        u32 host_len;
        memcpy(&host_len, buffer, sizeof(host_len));
        host_len = ntohl(host_len);
        buffer += sizeof(host_len);
        result.resize(host_len);

        memcpy(result.data(), buffer, sizeof(host_len));

        return result;
    }

    inline u32 get_string_serialized_size(const std::string &str)
    {
        return sizeof(u32) + str.size();
    }
}




// struct Command
// {
//     CommandType type;
//     union
//     {
//         CommandMoveParameters move;
//         CommandCaptureParameters capture;
//         CommandAttackParameters attack;
//         CommandBuildParameters build;
//     };
//
//     void serialize_to_buffer(u8 *buffer) const;
// };

// struct CommandMoveParameters
// {
//     u8 robot_count; // 0-32
//     u32 robot_nids[32];
//     D3DXVECTOR3 target_pos;
// };
//
// struct CommandCaptureParameters
// {
//     u8 robot_count; // 0-32
//     u32 robot_nids[32];
//     u32 target_nid; // Target NID to capture
// };
//
// struct CommandAttackParameters
// {
//     u8 robot_count; // 0-32
//     u32 robot_nids[32];
//     u32 target_nid; // Target NID to attack
// };
//
// struct CommandBuildParameters
// {
//     ERobotUnitKind chassis;
//     ERobotUnitKind hull;
//     ERobotUnitKind head;
//     ERobotUnitKind weapons[5];
//     u8 robot_count; // 0-...
//     u32 target_base_nid; // Base NID to build at.
// };

// struct CommandMoveGroup
// {
//     u8 robot_count; // 0-32
//     u32 robot_nids[32];
//     D3DXVECTOR3 target_pos;
// };

// struct CommandCapture
// {
//     u8 robot_count; // 0-32
//     u32 robot_nids[32];
//     u32 target_nid; // Target NID to capture
// };
//
// struct CommandAttack
// {
//     u8 robot_count; // 0-32
//     u32 robot_nids[32];
//     u32 target_nid; // Target NID to attack
// };

// // WARNING: mind the order!
// using Command = std::variant<
//     CommandMoveParams,
//     CommandCapture
//     // CommandAttack,
//     // CommandBuild
// >;

// inline u32 get_serialized_command_size(Command& command)
// {
//     // Type of command.index() 1 byte + command itself
//     return sizeof(u8) + std::visit(
//         [](auto &cmd) -> u32 { return cmd.get_serialized_size(); },
//         command
//     );
// }
//
// inline void serialize_command_to_buffer(Command& command, u8* buffer)
// {
//     // Write down the type
//     buffer[0] = command.index();
//
//     // Write down the command itself
//     std::visit(
//         [buffer](auto &cmd) -> void { cmd.serialize_to_buffer(buffer + 1); },
//         command
//     );
// }
//
// inline Command deserialize_command_from_buffer(u8* buffer /*, size_t size */)
// {
//     switch (buffer[0])
//     {
//     case 0: // CommandMove
//         return CommandMoveParams::deserialize_from_buffer(buffer + 1);
//     case 1: // CommandCapture
//         return CommandCapture::deserialize_from_buffer(buffer + 1);
//         // case 2: // CommandAttack
//         //     return CommandAttack::deserialize_from_buffer(buffer + 1);
//         // case 3: // CommandBuild
//         //     return CommandBuild::deserialize_from_buffer(buffer + 1);
//     default:
//         assert(false);
//     }
// }