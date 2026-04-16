#include "Message.hpp"

#include <cstring>

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

// } // namespace network
