#pragma once

#include <d3dx9math.h>
#include <string>
#include <vector>

#include "Types.hpp"

// TODO: optimize?
// TODO: add bit support?
// Note, this thing does some woodoo magic here, playing with BigEndian, LittleEndian,
//      this is not a general writer (probably this is bad). Also, the name ByteWriter would
//      fit better, but maybe we'll support bits later on (this would tank perfomance a bit)
//      - exweilt 29/01/2026
class BitWriter {
private:
    std::vector<u8> buffer;
    // u8 free_bits_count; // the last unfilled bits count

public:

    BitWriter() : buffer() {};
    BitWriter(size_t size) : buffer(size) {};

    // TODO: Introduce overloaded general write???
    // void write_bit(bool bit);
    void write_u8(u8 num);
    void write_u16(u16 num);
    void write_u32(u32 num);
    void write_u64(u64 num);
    void write_vec3(const D3DXVECTOR3 &vec);
    void write_string(const std::string &str);

    u8 *get_buffer();
    size_t get_buffer_size();
};

// TODO: buffer overflow risk
class BitReader {
private:
    u8 *buffer;
    u8 *reading_head;
public:

    BitReader() : buffer(nullptr) {};
    BitReader(u8 *buf) : buffer(buf), reading_head(buf) {};

    // bool read_bit();
    u8 read_u8();
    u16 read_u16();
    u32 read_u32();
    u64 read_u64();
    D3DXVECTOR3 read_vec3();
    std::string read_string();
};