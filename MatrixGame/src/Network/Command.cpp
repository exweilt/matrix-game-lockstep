#include "Command.hpp"

#include "MatrixRobot.hpp"

#include <MatrixSide.hpp>

#include "Interface/CConstructor.h"

#define SERIALIZE_U32_TO_BUFF(member_name, buffer_name)                 \
    {                                                                   \
        const u32 be_member_name = this->member_name;                   \
        memcpy(buffer_name, &be_member_name, sizeof(be_member_name));   \
        buffer_name += sizeof(be_member_name);                          \
    }


// bundles robots together, slow. TODO: optimize
// dangerous, changes the world!
i32 robots_to_logic_group(CMatrixSideUnit *side, u32 *robot_nid, size_t number_of_robots)
{
    // TODO: add asserts
    int no = side->GetNextFreeLogicGroup();

    side->m_PlayerGroup[no].Order(mpo_Stop);
    side->m_PlayerGroup[no].m_Obj = NULL;
    side->m_PlayerGroup[no].SetWar(false);
    side->m_PlayerGroup[no].m_RoadPath->Clear();

    for (i32 i = 0; i < number_of_robots; i++)
    {
        CMatrixMapStatic *obj = g_MatrixMap->find_static_with_nid(robot_nid[i]);
        // assert(obj->IsLiveRobot()); // bad
        if (obj->IsLiveRobot())
        {
            obj->AsRobot()->SetGroupLogic(no);
            side->m_PlayerGroup[no].m_RobotCnt++;
        }
    }

    return no;
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
    int no = robots_to_logic_group(side, robot_nid, number_of_robots);

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

    memset(result.robot_nid, 0, MAX_ROBOTS_PER_COMMAND * sizeof(u32)); //
    for (u32 i = 0; i < result.number_of_robots; i++)
    {
        result.robot_nid[i] =reader.read_u32();
    }

    result.target_pos = reader.read_vec3();

    return result;
}

CommandCaptureParams::CommandCaptureParams(const u32 r_nid, u32 target)
{
    number_of_robots = 1;
    robot_nid[0] = r_nid;
    target_nid = target;
};

void CommandCaptureParams::serialize_to_bitstream([[maybe_unused]]BitWriter &writer) const
{
    assert(this->number_of_robots <= MAX_ROBOTS_PER_COMMAND);
    writer.write_u8( this->number_of_robots );

    for (u32 i = 0; i < this->number_of_robots; i++)
    {
        writer.write_u32(this->robot_nid[i]);
    }

    writer.write_u32(target_nid);
}

CommandCaptureParams CommandCaptureParams::deserialize_from_bitstream([[maybe_unused]]BitReader &reader)
{
    CommandCaptureParams result;

    result.number_of_robots = reader.read_u8();

    memset(result.robot_nid, 0, MAX_ROBOTS_PER_COMMAND * sizeof(u32));
    for (u32 i = 0; i < result.number_of_robots; i++)
    {
        result.robot_nid[i] = reader.read_u32();
    }

    result.target_nid = reader.read_u32();

    return result;
}

void CommandCaptureParams::execute_for_side([[maybe_unused]]u32 side_id)
{
    CMatrixSideUnit *side = g_MatrixMap->GetSideById(side_id);
    int no = robots_to_logic_group(side, robot_nid, number_of_robots);

    // TODO: add check? assert(bui)
    CMatrixBuilding *building = g_MatrixMap->find_static_with_nid(target_nid)->AsBuilding();
    side->PGOrderCapture(no, building);
}

CommandAttackParams::CommandAttackParams(std::vector<u32> robots_nid, u32 target)
{
    assert(robots_nid.size() <= MAX_ROBOTS_PER_COMMAND);
    number_of_robots = robots_nid.size();
    memset(robot_nid, 0, MAX_ROBOTS_PER_COMMAND * sizeof(u32));
    memcpy(robot_nid, robots_nid.data(), robots_nid.size() * sizeof(u32));
    target_nid = target;
    target_pos = GetMapPos(g_MatrixMap->find_static_with_nid(target));
    is_attacking_position = false;
};


void CommandAttackParams::serialize_to_bitstream([[maybe_unused]]BitWriter &writer) const
{
    assert(this->number_of_robots <= MAX_ROBOTS_PER_COMMAND);
    writer.write_u8( this->number_of_robots );

    for (u32 i = 0; i < this->number_of_robots; i++)
    {
        writer.write_u32(this->robot_nid[i]);
    }

    writer.write_u32(target_nid);
    writer.write_u32(std::bit_cast<u32>(target_pos.x));
    writer.write_u32(std::bit_cast<u32>(target_pos.y));
    writer.write_u8(is_attacking_position);
}

CommandAttackParams CommandAttackParams::deserialize_from_bitstream([[maybe_unused]]BitReader &reader)
{
    CommandAttackParams result;

    result.number_of_robots = reader.read_u8();

    memset(result.robot_nid, 0, MAX_ROBOTS_PER_COMMAND * sizeof(u32));
    for (u32 i = 0; i < result.number_of_robots; i++)
    {
        result.robot_nid[i] = reader.read_u32();
    }

    result.target_nid = reader.read_u32();

    result.target_pos.x = std::bit_cast<int>(reader.read_u32());
    result.target_pos.y = std::bit_cast<int>(reader.read_u32());

    result.is_attacking_position = reader.read_u8();

    return result;
}

void CommandAttackParams::execute_for_side([[maybe_unused]]u32 side_id)
{
    CMatrixSideUnit *side = g_MatrixMap->GetSideById(side_id);
    int no = robots_to_logic_group(side, robot_nid, number_of_robots);

    // TODO: add check? assert(bui)
    CMatrixMapStatic *tgt = is_attacking_position ? nullptr : g_MatrixMap->find_static_with_nid(target_nid);
    side->PGOrderAttack(no, target_pos, tgt);
}

void CommandBuildParams::serialize_to_bitstream([[maybe_unused]]BitWriter &writer) const
{
    writer.write_u8(chassis);
    writer.write_u8(hull);
    writer.write_u8(head);

    for (int i = 0; i < MAX_WEAPON_CNT; i++)
    {
        writer.write_u8(weapons[i]);
    }

    writer.write_u8(robot_count);
    writer.write_u32(target_base_nid);
}

CommandBuildParams CommandBuildParams::deserialize_from_bitstream([[maybe_unused]]BitReader &reader)
{
    CommandBuildParams result;

    result.chassis  = static_cast<ERobotUnitKind>(reader.read_u8());
    result.hull     = static_cast<ERobotUnitKind>(reader.read_u8());
    result.head     = static_cast<ERobotUnitKind>(reader.read_u8());

    for (int i = 0; i < MAX_WEAPON_CNT; i++)
    {
        result.weapons[i] = static_cast<ERobotUnitKind>(reader.read_u8());
    }

    result.robot_count = reader.read_u8();
    result.target_base_nid = reader.read_u32();

    return result;
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
    // return CommandBuildParams{};
}

void CommandBuildParams::execute_for_side([[maybe_unused]]u32 side_id)
{
    DTRACE();
    CMatrixSideUnit *side = g_MatrixMap->GetSideById(side_id);
    CMatrixMapStatic *base_static = g_MatrixMap->find_static_with_nid(target_base_nid);
    if (base_static == nullptr || !base_static->IsBase())
        return; // some scheisse

    CMatrixBuilding *base = base_static->AsBuilding();

    if (base->m_Side != side_id)
        return;

    DCP();

    side->m_Constructor->SetBase(base);
    DCP();
    side->m_Constructor->OperateUnit(MRT_CHASSIS, chassis);
    side->m_Constructor->OperateUnit(MRT_ARMOR, hull);
    side->m_Constructor->OperateUnit(MRT_HEAD, head);

    DCP();

    for (i32 i = 0; i < MAX_WEAPON_CNT; i++)
    {
        side->m_Constructor->OperateUnit(MRT_WEAPON, weapons[i]);
    }

    DCP();
    for (i32 i = 0; i < robot_count; i++)
    {
        side->m_Constructor->StackRobot(nullptr);
    }

    DCP();
    int res[MAX_RESOURCES];
    side->m_Constructor->GetConstructionPrice(res);
    side->AddResourceAmount(TITAN, -res[TITAN] * robot_count);
    side->AddResourceAmount(ENERGY, -res[ENERGY] * robot_count);
    side->AddResourceAmount(ELECTRONICS, -res[ELECTRONICS] * robot_count);
    side->AddResourceAmount(PLASMA, -res[PLASMA] * robot_count);
    //
    // if (player_side && player_side->m_ConstructPanel) {
    //     player_side->m_ConstructPanel->ResetGroupNClose();
    // }
    // g_IFaceList->m_RCountControl->Reset();
    // g_IFaceList->m_RCountControl->CheckUp();
}

// } // namespace network

#undef SERIALIZE_U32_TO_BUFF