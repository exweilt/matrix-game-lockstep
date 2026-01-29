#include "Command.hpp"

#include "MatrixRobot.hpp"

#define SERIALIZE_U32_TO_BUFF(member_name, buffer_name)                 \
    {                                                                   \
        const u32 be_member_name = this->member_name;                   \
        memcpy(buffer_name, &be_member_name, sizeof(be_member_name));   \
        buffer_name += sizeof(be_member_name);                          \
    }

// namespace network
// {

void Command::serialize_to_bitstream(BitWriter &writer) const
{
    writer.write_u8(static_cast<u8>(this->type));

    switch (this->type)
    {
        case CommandType::MOVE:     move    .serialize_to_bitstream(writer);    break;
        case CommandType::CAPTURE:  capture .serialize_to_bitstream(writer);    break;
        case CommandType::ATTACK:   attack  .serialize_to_bitstream(writer);    break;
        case CommandType::BUILD:    build   .serialize_to_bitstream(writer);    break;
        default:                    assert(false);
    }
}

void Command::execute_for_side(u32 side_id)
{
    switch (type)
    {
    case CommandType::MOVE:     move    .execute_for_side(side_id);     return;
    case CommandType::CAPTURE:  capture .execute_for_side(side_id);     return;
    case CommandType::ATTACK:   attack  .execute_for_side(side_id);     return;
    case CommandType::BUILD:    build   .execute_for_side(side_id);     return;
    default:                    return;
    }
}

Command Command::deserialize_from_bitstream(BitReader &reader)
{
    Command result;

    result.type = static_cast<CommandType>( reader.read_u8() );

    switch (result.type)
    {
        case CommandType::MOVE:     result.move     = CommandMoveParams::   deserialize_from_bitstream(reader);break;
        case CommandType::CAPTURE:  result.capture  = CommandCaptureParams::deserialize_from_bitstream(reader);break;
        case CommandType::ATTACK:   result.attack   = CommandAttackParams:: deserialize_from_bitstream(reader);break;
        case CommandType::BUILD:    result.build    = CommandBuildParams::  deserialize_from_bitstream(reader);break;
        default:                    assert(false);
    }

    return result;
}

CommandMoveParams::CommandMoveParams(const u32 r_nid, const D3DXVECTOR3 &dest)
{
    this->number_of_robots = 1;
    this->robot_nid[0] = r_nid;
    this->target_pos = dest;
}

void CommandMoveParams::execute_for_side(u32 side_id)
{
    CMatrixSideUnit *side = g_MatrixMap->GetSideById(side_id);
    int no = side->GetNextFreeLogicGroup();

    side->m_PlayerGroup[no].Order(mpo_Stop);
    side->m_PlayerGroup[no].m_Obj = NULL;
    side->m_PlayerGroup[no].SetWar(false);
    side->m_PlayerGroup[no].m_RoadPath->Clear();

    for (i32 i = 0; i < this->number_of_robots; i++)
    {
        CMatrixMapStatic *obj = g_MatrixMap->find_static_with_nid(this->robot_nid[i]);
        assert(obj->IsLiveRobot());

        obj->AsRobot()->SetGroupLogic(no);
        side->m_PlayerGroup[no].m_RobotCnt++;
    }

    int mx = Float2Int(target_pos.x / GLOBAL_SCALE_MOVE);
    int my = Float2Int(target_pos.y / GLOBAL_SCALE_MOVE);
    side->PGOrderMoveTo(no, CPoint(mx - ROBOT_MOVECELLS_PER_SIZE / 2, my - ROBOT_MOVECELLS_PER_SIZE / 2));
}

void CommandMoveParams::serialize_to_bitstream(BitWriter &writer) const
{
    assert(this->number_of_robots <= MAX_ROBOTS_PER_COMMAND);
    writer.write_u8( this->number_of_robots );

    for (u32 i = 0; i < this->number_of_robots; i++)
    {
        writer.write_u32(this->robot_nid[i]);
    }

    writer.write_vec3(this->target_pos);
}

CommandMoveParams CommandMoveParams::deserialize_from_bitstream(BitReader &reader)
{
    CommandMoveParams result;

    result.number_of_robots = reader.read_u8();

    for (u32 i = 0; i < result.number_of_robots; i++)
    {
        result.robot_nid[i] =reader.read_u32();
    }

    result.target_pos = reader.read_vec3();

    return result;
}

void CommandCaptureParams::serialize_to_bitstream([[maybe_unused]]BitWriter &writer) const
{
    // // robot_nid
    // const u32 be_robot_nid = htonl(this->robot_nid); // Big Endian
    // memcpy(buffer, &be_robot_nid, sizeof(be_robot_nid));
    // buffer += sizeof(be_robot_nid);
    //
    // // target_nid
    // const u32 be_target_nid = htonl(this->target_nid); // Big Endian
    // memcpy(buffer, &be_target_nid, sizeof(be_target_nid));
    // // buffer += sizeof(be_target_nid);
}

CommandCaptureParams CommandCaptureParams::deserialize_from_bitstream([[maybe_unused]]BitReader &reader)
{
    // u32 host_robot_nid;
    // memcpy(&host_robot_nid, buffer, sizeof(host_robot_nid));
    // host_robot_nid = ntohl(host_robot_nid);
    // buffer += sizeof(host_robot_nid);
    //
    // u32 host_target_nid;
    // memcpy(&host_target_nid, buffer, sizeof(host_target_nid));
    // host_target_nid = ntohl(host_target_nid);
    // // buffer += sizeof(host_target_nid);
    //
    // return CommandCaptureParams{host_robot_nid, host_target_nid};
    return CommandCaptureParams{};
}

void CommandCaptureParams::execute_for_side([[maybe_unused]]u32 side_id)
{
}

void CommandAttackParams::serialize_to_bitstream([[maybe_unused]]BitWriter &writer) const
{
    // // robot_nid
    // const u32 be_robot_nid = htonl(this->robot_nid); // Big Endian
    // memcpy(buffer, &be_robot_nid, sizeof(be_robot_nid));
    // buffer += sizeof(be_robot_nid);
    //
    // // target_nid
    // const u32 be_target_nid = htonl(this->target_nid); // Big Endian
    // memcpy(buffer, &be_target_nid, sizeof(be_target_nid));
    // // buffer += sizeof(be_target_nid);
}

CommandAttackParams CommandAttackParams::deserialize_from_bitstream([[maybe_unused]]BitReader &reader)
{
    // u32 host_robot_nid;
    // memcpy(&host_robot_nid, buffer, sizeof(host_robot_nid));
    // host_robot_nid = ntohl(host_robot_nid);
    // buffer += sizeof(host_robot_nid);
    //
    // u32 host_target_nid;
    // memcpy(&host_target_nid, buffer, sizeof(host_target_nid));
    // host_target_nid = ntohl(host_target_nid);
    // // buffer += sizeof(host_target_nid);
    //
    // return CommandAttackParams{host_robot_nid, host_target_nid};
    return CommandAttackParams{};
}

void CommandAttackParams::execute_for_side([[maybe_unused]]u32 side_id)
{
}

void CommandBuildParams::serialize_to_bitstream([[maybe_unused]]BitWriter &writer) const
{
    // SERIALIZE_U32_TO_BUFF(chassis, buffer)
    // SERIALIZE_U32_TO_BUFF(hull, buffer)
    // SERIALIZE_U32_TO_BUFF(head, buffer)
    // buffer[0] = this->robot_count;
    // buffer += 1;
    // SERIALIZE_U32_TO_BUFF(target_base_nid, buffer)
    //
    // for (u32 i = 0; i < MAX_WEAPON_CNT; i++)
    // {
    //     const u32 be_weapon = htonl(this->weapons[i]);
    //     memcpy(buffer, &be_weapon, sizeof(be_weapon));
    //     buffer += sizeof(be_weapon);
    // }
}

CommandBuildParams CommandBuildParams::deserialize_from_bitstream([[maybe_unused]]BitReader &reader)
{
    // u32 host_chassis;
    // memcpy(&host_chassis, buffer, sizeof(host_chassis));
    // host_chassis = ntohl(host_chassis);
    // buffer += sizeof(host_chassis);
    //
    // u32 host_hull;
    // memcpy(&host_hull, buffer, sizeof(host_hull));
    // host_hull = ntohl(host_hull);
    // buffer += sizeof(host_hull);
    //
    // u32 host_head;
    // memcpy(&host_head, buffer, sizeof(host_head));
    // host_head = ntohl(host_head);
    // buffer += sizeof(host_head);
    //
    // const u8 host_robot_count = buffer[0];
    // buffer += 1;
    //
    // u32 host_target_base_nid;
    // memcpy(&host_target_base_nid, buffer, sizeof(host_target_base_nid));
    // host_target_base_nid = ntohl(host_target_base_nid);
    // buffer += sizeof(host_target_base_nid);
    //
    // ERobotUnitKind host_weapons[MAX_WEAPON_CNT];
    // for (u32 i = 0; i < MAX_WEAPON_CNT; i++)
    // {
    //     memcpy(host_weapons + i, buffer, sizeof(ERobotUnitKind));
    //     host_weapons[i] = static_cast<ERobotUnitKind>(ntohl(host_weapons[i]));
    //     buffer += sizeof(ERobotUnitKind);
    // }
    //
    // return CommandBuildParams
    // {
    //     static_cast<ERobotUnitKind>(host_chassis),
    //     static_cast<ERobotUnitKind>(host_hull),
    //     static_cast<ERobotUnitKind>(host_head),
    //     static_cast<ERobotUnitKind*>(host_weapons),
    //     host_robot_count,
    //     host_target_base_nid
    // };
    return CommandBuildParams{};
}

void CommandBuildParams::execute_for_side([[maybe_unused]]u32 side_id)
{

}

// } // namespace network

#undef SERIALIZE_U32_TO_BUFF