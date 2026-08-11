#pragma once
#include <d3d9.h>
#include <string>
#include <vector>

#include "Types.hpp"


namespace Base {
    struct SFileRec;
}

class Lobby
{
    u32 selected_map_index = 0;
    char input_server_ip[32] = {};
    char input_username[32] = {};

    bool is_file_map(Base::SFileRec * file);
public:
    Lobby();
    ~Lobby();

    void refresh_list_of_maps();
    std::vector<std::string> cached_list_of_maps;
    void select_map(u32 map_index);
        LPDIRECT3DTEXTURE9 map_texture;

    std::string get_selected_map_name();
    void load_map_texture();

    void draw();

};
