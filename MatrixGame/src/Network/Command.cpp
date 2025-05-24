#include "Command.hpp"

#include "MatrixRobot.hpp"

#define SERIALIZE_U32_TO_BUFF(member_name, buffer_name)                 \
    {                                                                   \
        const u32 be_member_name = htonl(this->member_name);            \
        memcpy(buffer_name, &be_member_name, sizeof(be_member_name));   \
        buffer_name += sizeof(be_member_name);                          \
    }

namespace network
{
    void Command::serialize_to_buffer(u8 *buffer) const
    {
        buffer[0] = static_cast<u8>(this->type);

        switch (this->type)
        {
            case CommandType::MOVE:     move.serialize_to_buffer(buffer + 1);       break;
            case CommandType::CAPTURE:  capture.serialize_to_buffer(buffer + 1);    break;
            case CommandType::ATTACK:   attack.serialize_to_buffer(buffer + 1);     break;
            case CommandType::BUILD:    build.serialize_to_buffer(buffer + 1);      break;
            default:                    assert(false);
        }
    }

    Command Command::deserialize_from_buffer(const u8 *buffer)
    {
        Command result;

        result.type = static_cast<CommandType>(buffer[0]);

        switch (result.type)
        {
            case CommandType::MOVE:     result.move     = CommandMoveParams::deserialize_from_buffer(buffer + 1);       break;
            case CommandType::CAPTURE:  result.capture  = CommandCaptureParams::deserialize_from_buffer(buffer + 1);    break;
            case CommandType::ATTACK:   result.attack   = CommandAttackParams::deserialize_from_buffer(buffer + 1);     break;
            case CommandType::BUILD:    result.build    = CommandBuildParams::deserialize_from_buffer(buffer + 1);      break;
            default:                    assert(false);
        }

        return result;
    }

    void CommandMoveParams::serialize_to_buffer(u8 *buffer) const
    {
        // robot_nid
        const u32 be_robot_nid = htonl(this->robot_nid); // Big Endian
        memcpy(buffer, &be_robot_nid, sizeof(be_robot_nid));
        buffer += sizeof(be_robot_nid);

        // target_pos
        serialize_vector3_to_buffer(this->target_pos, buffer);
    }

    CommandMoveParams CommandMoveParams::deserialize_from_buffer(const u8 *buffer)
    {
        u32 host_target_nid;
        memcpy(&host_target_nid, buffer, sizeof(host_target_nid));
        host_target_nid = ntohl(host_target_nid);
        buffer += sizeof(host_target_nid);

        return CommandMoveParams
        {
            host_target_nid, deserialize_vector3_from_buffer(buffer)
        };
    }

    void CommandCaptureParams::serialize_to_buffer(u8 *buffer) const
    {
        // robot_nid
        const u32 be_robot_nid = htonl(this->robot_nid); // Big Endian
        memcpy(buffer, &be_robot_nid, sizeof(be_robot_nid));
        buffer += sizeof(be_robot_nid);

        // target_nid
        const u32 be_target_nid = htonl(this->target_nid); // Big Endian
        memcpy(buffer, &be_target_nid, sizeof(be_target_nid));
        // buffer += sizeof(be_target_nid);
    }

    CommandCaptureParams CommandCaptureParams::deserialize_from_buffer([[maybe_unused]] const u8 *buffer)
    {
        u32 host_robot_nid;
        memcpy(&host_robot_nid, buffer, sizeof(host_robot_nid));
        host_robot_nid = ntohl(host_robot_nid);
        buffer += sizeof(host_robot_nid);

        u32 host_target_nid;
        memcpy(&host_target_nid, buffer, sizeof(host_target_nid));
        host_target_nid = ntohl(host_target_nid);
        // buffer += sizeof(host_target_nid);

        return CommandCaptureParams{host_robot_nid, host_target_nid};
    }

    void CommandAttackParams::serialize_to_buffer(u8 *buffer) const
    {
        // robot_nid
        const u32 be_robot_nid = htonl(this->robot_nid); // Big Endian
        memcpy(buffer, &be_robot_nid, sizeof(be_robot_nid));
        buffer += sizeof(be_robot_nid);

        // target_nid
        const u32 be_target_nid = htonl(this->target_nid); // Big Endian
        memcpy(buffer, &be_target_nid, sizeof(be_target_nid));
        // buffer += sizeof(be_target_nid);
    }

    CommandAttackParams CommandAttackParams::deserialize_from_buffer(const u8 *buffer)
    {
        u32 host_robot_nid;
        memcpy(&host_robot_nid, buffer, sizeof(host_robot_nid));
        host_robot_nid = ntohl(host_robot_nid);
        buffer += sizeof(host_robot_nid);

        u32 host_target_nid;
        memcpy(&host_target_nid, buffer, sizeof(host_target_nid));
        host_target_nid = ntohl(host_target_nid);
        // buffer += sizeof(host_target_nid);

        return CommandAttackParams{host_robot_nid, host_target_nid};
    }

    void CommandBuildParams::serialize_to_buffer(u8 *buffer) const
    {
        SERIALIZE_U32_TO_BUFF(chassis, buffer)
        SERIALIZE_U32_TO_BUFF(hull, buffer)
        SERIALIZE_U32_TO_BUFF(head, buffer)
        buffer[0] = this->robot_count;
        buffer += 1;
        SERIALIZE_U32_TO_BUFF(target_base_nid, buffer)

        for (u32 i = 0; i < MAX_WEAPON_CNT; i++)
        {
            const u32 be_weapon = htonl(this->weapons[i]);
            memcpy(buffer, &be_weapon, sizeof(be_weapon));
            buffer += sizeof(be_weapon);
        }
    }

    CommandBuildParams CommandBuildParams::deserialize_from_buffer(const u8 *buffer)
    {
        u32 host_chassis;
        memcpy(&host_chassis, buffer, sizeof(host_chassis));
        host_chassis = ntohl(host_chassis);
        buffer += sizeof(host_chassis);

        u32 host_hull;
        memcpy(&host_hull, buffer, sizeof(host_hull));
        host_hull = ntohl(host_hull);
        buffer += sizeof(host_hull);

        u32 host_head;
        memcpy(&host_head, buffer, sizeof(host_head));
        host_head = ntohl(host_head);
        buffer += sizeof(host_head);

        const u8 host_robot_count = buffer[0];
        buffer += 1;

        u32 host_target_base_nid;
        memcpy(&host_target_base_nid, buffer, sizeof(host_target_base_nid));
        host_target_base_nid = ntohl(host_target_base_nid);
        buffer += sizeof(host_target_base_nid);

        ERobotUnitKind host_weapons[MAX_WEAPON_CNT];
        for (u32 i = 0; i < MAX_WEAPON_CNT; i++)
        {
            memcpy(host_weapons + i, buffer, sizeof(ERobotUnitKind));
            host_weapons[i] = static_cast<ERobotUnitKind>(ntohl(host_weapons[i]));
            buffer += sizeof(ERobotUnitKind);
        }

        return CommandBuildParams
        {
            static_cast<ERobotUnitKind>(host_chassis),
            static_cast<ERobotUnitKind>(host_hull),
            static_cast<ERobotUnitKind>(host_head),
            static_cast<ERobotUnitKind*>(host_weapons),
            host_robot_count,
            host_target_base_nid
        };
    }
} // namespace network

#undef SERIALIZE_U32_TO_BUFF