#include "Message.hpp"

#include <cstring>

namespace network
{
    // u32 MessageCommandBatchParams::get_size()
    // {
    //
    // }

    void MessageCommandBatchParams::serialize_to_buffer(u8 *buffer)
    {
        // target_frame
        const u32 be_frame = htonl(this->target_frame);
        std::memcpy(buffer, &be_frame, sizeof(be_frame));
        buffer += sizeof(be_frame);

        // target_side
        buffer[0] = this->target_side;
        buffer += sizeof(this->target_side);

        // number of commands
        const u32 be_command_count = htonl(this->commands.size());
        std::memcpy(buffer, &be_command_count, sizeof(be_command_count));
        buffer += sizeof(be_command_count);

        // the array of commands
        for (u32 i = 0; i < this->commands.size(); i++)
        {
            this->commands[i].serialize_to_buffer(buffer);
            buffer += this->commands[i].get_serialized_size();
        }
    }

    MessageCommandBatchParams MessageCommandBatchParams::deserialize_from_buffer(u8 *buffer)
    {
        MessageCommandBatchParams result{0, buffer[4]};
        memcpy(&result.target_frame, buffer, sizeof(result.target_frame));
        result.target_frame = ntohl(result.target_frame);
        buffer += 5;

        u32 command_count;
        memcpy(&command_count, buffer, sizeof(command_count));
        command_count = ntohl(command_count);
        buffer += sizeof(command_count);

        result.commands.resize(command_count);

        for (u32 i = 0; i < command_count; i++)
        {
            result.commands[i] = Command::deserialize_from_buffer(buffer);
            buffer += result.commands[i].get_serialized_size(); // In future could skip recalculation of size
        }

        return result; // :)
    }

    void MessageJoinParams::serialize_to_buffer(u8 *buffer) const
    {
        buffer[0] = this->player_side;
        buffer += 1;

        serialize_string_to_buffer(this->username, buffer);
    }

    MessageJoinParams MessageJoinParams::deserialize_from_buffer(const u8 *buffer)
    {
        const u8 host_player_side = buffer[0];

        const std::string username = deserialize_string_from_buffer(buffer + 1);

        return MessageJoinParams
        {
            host_player_side, username
        };
    }
} // namespace network