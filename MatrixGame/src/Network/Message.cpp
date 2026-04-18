#include "Message.hpp"

#include <cstring>

#include "Math3D.hpp"
#include "MatrixMap.hpp"
#include "MatrixRobot.hpp"
#include "MatrixSide.hpp"

// namespace network
// {

// u32 MessageCommandBatchParams::get_size()
// {
//
// }

// void MessageCommandBatchParams::serialize_to_bitstream(BitWriter &writer) const
// {
//     writer.write_u32(this->target_frame);            // target_frame
//     writer.write_u8(this->target_side);                     // target_side
//     writer.write_u32(this->commands.size()); // number of commands
//
//     // the array of commands
//     for (i32 i = 0; i < this->commands.size(); i++)
//     {
//         this->commands[i].serialize_to_bitstream(writer);
//     }
// }
//
// MessageCommandBatchParams MessageCommandBatchParams::deserialize_from_bitstream(BitReader &reader)
// {
//     MessageCommandBatchParams result;
//
//     result.target_frame = reader.read_u32();
//     result.target_side  = reader.read_u8();
//
//     u32 command_count = reader.read_u32();
//
//     result.commands.resize(command_count);
//     for (u32 i = 0; i < command_count; i++)
//     {
//         result.commands[i] = Command::deserialize_from_bitstream(reader);
//     }
//
//     return result; // :)
// }

void MessageWorldSnapshotParams::serialize_to_bitstream(BitWriter &writer) const
{
    ws.serialize_to_bitstream(writer);
}

MessageWorldSnapshotParams MessageWorldSnapshotParams::deserialize_from_bitstream(BitReader &reader)
{
    return MessageWorldSnapshotParams( WorldSnapshot::deserialize_from_bitstream(reader) );
}

void MessageJoinParams::serialize_to_bitstream(BitWriter &writer) const
{
    writer.write_u8(this->player_side);
    writer.write_string(this->username);
}

MessageJoinParams MessageJoinParams::deserialize_from_bitstream(BitReader &reader)
{
    const u8 host_player_side = reader.read_u8();
    const std::string username = reader.read_string();

    return MessageJoinParams
    {
        host_player_side, username
    };
}

void MessageChecksumParams::serialize_to_bitstream(BitWriter &writer) const
{
    writer.write_u32(this->target_frame);
    writer.write_u64(this->checksum);
}

MessageChecksumParams MessageChecksumParams::deserialize_from_bitstream(BitReader &reader)
{
    u32 frame = reader.read_u32();
    u64 checksum = reader.read_u64();

    return MessageChecksumParams
    {
        frame, checksum
    };
}

void MessageReportParams::serialize_to_bitstream(BitWriter &writer) const
{
    writer.write_u8(this->player_side);
    writer.write_u8(static_cast<u8>(this->type));
    writer.write_string(this->data);
}

MessageReportParams MessageReportParams::deserialize_from_bitstream(BitReader &reader)
{
    u8 side = reader.read_u8();
    u8 type = reader.read_u8();
    std::string data = reader.read_string();

    return MessageReportParams
    {
        side, data, static_cast<ReportType>(type)
    };
}

void MessageDesyncParams::serialize_to_bitstream(BitWriter &writer) const
{
    writer.write_u32(this->target_frame);
}

MessageDesyncParams MessageDesyncParams::deserialize_from_bitstream(BitReader &reader)
{
    return MessageDesyncParams { reader.read_u32() };
}

MessageCommandMoveParams::MessageCommandMoveParams(const u32 r_nid, const D3DXVECTOR3 &dest)
{
    this->number_of_robots = 1;
    this->robot_nid[0] = r_nid;
    this->target_pos = dest;
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

void MessageCommandMoveParams::execute_for_side(u32 side_id)
{
    CMatrixSideUnit *side = g_MatrixMap->GetSideById(side_id);
    int no = robots_to_logic_group(side, robot_nid, number_of_robots);

    int mx = Float2Int(target_pos.x / GLOBAL_SCALE_MOVE);
    int my = Float2Int(target_pos.y / GLOBAL_SCALE_MOVE);
    side->PGOrderMoveTo(no, CPoint(mx - ROBOT_MOVECELLS_PER_SIZE / 2, my - ROBOT_MOVECELLS_PER_SIZE / 2));
}

void MessageCommandMoveParams::execute()
{
    CMatrixSideUnit *side = g_MatrixMap->GetSideById(g_MatrixMap->find_static_with_nid(robot_nid[0])->AsRobot()->GetSide());
    int no = robots_to_logic_group(side, robot_nid, number_of_robots);

    int mx = Float2Int(target_pos.x / GLOBAL_SCALE_MOVE);
    int my = Float2Int(target_pos.y / GLOBAL_SCALE_MOVE);
    side->PGOrderMoveTo(no, CPoint(mx - ROBOT_MOVECELLS_PER_SIZE / 2, my - ROBOT_MOVECELLS_PER_SIZE / 2));
}

void MessageCommandMoveParams::serialize_to_bitstream(BitWriter &writer) const
{
    assert(this->number_of_robots <= MAX_ROBOTS_PER_COMMAND);
    writer.write_u8( this->number_of_robots );

    for (u32 i = 0; i < this->number_of_robots; i++)
    {
        writer.write_u32(this->robot_nid[i]);
    }

    writer.write_vec3(this->target_pos);
}

MessageCommandMoveParams MessageCommandMoveParams::deserialize_from_bitstream(BitReader &reader)
{
    MessageCommandMoveParams result;

    result.number_of_robots = reader.read_u8();

    memset(result.robot_nid, 0, MAX_ROBOTS_PER_COMMAND * sizeof(u32)); //
    for (u32 i = 0; i < result.number_of_robots; i++)
    {
        result.robot_nid[i] = reader.read_u32();
    }

    result.target_pos = reader.read_vec3();

    return result;
}

// } // namespace network
