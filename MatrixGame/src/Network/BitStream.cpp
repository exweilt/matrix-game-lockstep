#include "BitStream.hpp"

// void BitWriter::write_bit(bool bit)
// {
// }

void BitWriter::write_u8(u8 num)
{
    buffer.push_back(num);
}

void BitWriter::write_u16(u16 num)
{
    // Write High byte, then Low byte
    write_u8((num >> 8) & 0xFF);
    write_u8(num & 0xFF);
}

void BitWriter::write_u32(u32 num)
{
    // Write MSB to LSB
    write_u8((num >> 24) & 0xFF);
    write_u8((num >> 16) & 0xFF);
    write_u8((num >> 8) & 0xFF);
    write_u8(num & 0xFF);
}

void BitWriter::write_u64(u64 num)
{
    // Write MSB to LSB
    write_u8((num >> 56) & 0xFF);
    write_u8((num >> 48) & 0xFF);
    write_u8((num >> 40) & 0xFF);
    write_u8((num >> 32) & 0xFF);
    write_u8((num >> 24) & 0xFF);
    write_u8((num >> 16) & 0xFF);
    write_u8((num >> 8) & 0xFF);
    write_u8(num & 0xFF);
}

void BitWriter::write_vec3(const D3DXVECTOR3 &vec)
{
    write_u32(std::bit_cast<u32>(vec.x));
    write_u32(std::bit_cast<u32>(vec.y));
    write_u32(std::bit_cast<u32>(vec.z));
}

void BitWriter::write_string(const std::string &str)
{
    write_u32(str.size());

    for (u32 i = 0; i < str.size(); i++)
    {
        write_u8(str[i]);
    }
}

u8 * BitWriter::get_buffer()
{
    return buffer.data();
}

size_t BitWriter::get_buffer_size()
{
    return buffer.size();
}

// ====================== Reader ======================

u8 BitReader::read_u8()
{
    // TODO: Add bounds check here!
    // if (reading_head >= buffer_end)

    u8 read = *reading_head;
    reading_head++;
    return read;
}

u16 BitReader::read_u16()
{
    u16 b1 = read_u8();
    u16 b0 = read_u8();
    return (b1 << 8) | b0;
}

u32 BitReader::read_u32()
{
    u32 b3 = read_u8();
    u32 b2 = read_u8();
    u32 b1 = read_u8();
    u32 b0 = read_u8();
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

u64 BitReader::read_u64()
{
    u64 b7 = read_u8();
    u64 b6 = read_u8();
    u64 b5 = read_u8();
    u64 b4 = read_u8();
    u64 b3 = read_u8();
    u64 b2 = read_u8();
    u64 b1 = read_u8();
    u64 b0 = read_u8();
    return (b7 << 56) | (b6 << 48) | (b5 << 40) | (b4 << 32) |
           (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

D3DXVECTOR3 BitReader::read_vec3()
{
    D3DXVECTOR3 result;

    result.x = std::bit_cast<f32>(read_u32());
    result.y = std::bit_cast<f32>(read_u32());
    result.z = std::bit_cast<f32>(read_u32());

    return result;
}

std::string BitReader::read_string()
{
    std::string result;

    u32 len = read_u32();
    result.resize(len);

    memcpy(result.data(), reading_head, len);
    reading_head += len;

    return result;
}
