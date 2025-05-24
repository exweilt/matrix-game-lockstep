#pragma once

#include <winsock2.h>

#include <string>
#include <variant>
#include <vector>

#include "Command.hpp"

#include <cassert>


namespace network
{
    enum class MessageType : u8
    {
        NONE            = 0,
        COMMAND_BATCH    = 1,
        READY           = 2,
        INFO,
        SAY,
        JOIN,
        PING,
        PONG
    };

    struct MessageCommandBatchParams
    {
        u32 target_frame;
        u8 target_side; // SideID
        std::vector<Command> commands;

        MessageCommandBatchParams(const u32 frame, const u8 side) : target_frame(frame), target_side(side), commands() {}
        ~MessageCommandBatchParams() {}

        u32 get_serialized_size() const
        {
            u32 size = sizeof(target_frame) + sizeof(target_side) + sizeof(size_t);

            for (u32 i = 0; i < commands.size(); i++)
            {
                size += commands[i].get_serialized_size();
            }

            return size;
        }
        void serialize_to_buffer(u8* buffer);
        static MessageCommandBatchParams deserialize_from_buffer(u8* buffer);
    };

    struct MessageJoinParams
    {
        u8 player_side;
        std::string username;

        MessageJoinParams(u8 side = 0, std::string name = "Greph") : player_side(side), username(name) {}
        ~MessageJoinParams() {}

        u32 get_serialized_size() const { return sizeof(player_side) + get_string_serialized_size(username); }
        void serialize_to_buffer(u8* buffer) const;
        static MessageJoinParams deserialize_from_buffer(const u8* buffer);
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
            MessageCommandBatchParams command_batch;
            MessageJoinParams join;
        };

        Message()                               : type(MessageType::NONE) {};
        Message(MessageCommandBatchParams cb)   : type(MessageType::COMMAND_BATCH), command_batch(cb) {};
        Message(MessageJoinParams j)            : type(MessageType::JOIN),          join(j)     {};

        ~Message()
        {
            switch (type)
            {
                case MessageType::COMMAND_BATCH:
                    command_batch.~MessageCommandBatchParams();
                    break;
                case MessageType::JOIN:
                    join.~MessageJoinParams();
                    break;
                default:;
            }
        }

        u32 get_serialized_size() const
        {
            switch (type)
            {
                case MessageType::COMMAND_BATCH: return sizeof(u8) + command_batch.get_serialized_size();
                case MessageType::JOIN:         return sizeof(u8) + join.get_serialized_size();
                default:                        return 0;
            }
        }

        void serialize_to_buffer(u8* buffer)
        {
            // Write down the type tag
            buffer[0] = static_cast<u8>(type);

            // Write down the message itself
            switch (type)
            {
                case MessageType::COMMAND_BATCH: command_batch.serialize_to_buffer(buffer + 1); break;
                case MessageType::JOIN:         join.serialize_to_buffer(buffer + 1);           break;
                default:                        assert(false);
            }
        }

        static Message deserialize_from_buffer(u8* buffer /*, size_t size */)
        {
            switch (static_cast<MessageType>(buffer[0]))
            {
                case MessageType::COMMAND_BATCH:
                    return MessageCommandBatchParams::deserialize_from_buffer(buffer + 1);
                case MessageType::JOIN:
                    return MessageJoinParams::deserialize_from_buffer(buffer + 1);
                default:
                    assert(false);
            }
        }
    };
}

namespace nw = network;


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