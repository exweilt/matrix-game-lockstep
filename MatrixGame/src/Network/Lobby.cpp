#include "Lobby.hpp"

#include <cassert>
#include <iostream>

#include "CFile.hpp"
#include "CStorage.hpp"
#include "D3DControl.hpp"
#include "imgui.h"
#include "Pack.hpp"

bool Lobby::is_file_map(SFileRec * file)
{
    const size_t len = std::strlen(file->m_RealName);
    return len >= 5 && std::strcmp(file->m_RealName + len - 5, ".cmap") == 0;
}

Lobby::Lobby()
{
    strcpy(input_server_ip, "127.0.0.1");
    strcpy(input_username, "Gref");
    map_texture = nullptr;
}

Lobby::~Lobby()
{
}

void Lobby::refresh_list_of_maps()
{
    cached_list_of_maps.clear();

    // auto map_folder =reinterpret_cast<CHsFolder *>(
    //     reinterpret_cast<CHsFolder *>(CFile::m_Packs->m_PackFiles[0]->m_RootFolder->GetFileRec(0)->m_Extra)
    //         ->GetFileRec(8)->m_Extra
    // );
    // map_folder->m_Extra;

    CHsFolder * map_folder = CFile::m_Packs->m_PackFiles[0]->m_RootFolder->GetFolderEx("Matrix/Map");
    assert(map_folder);

    // cached_list_of_maps.reserve(map_folder->GetFolderRec()->m_Recnum);

    u32 number_of_files = map_folder->GetFolderRec()->m_Recnum;
    for (int i = 0; i < number_of_files; i++)
    {
        SFileRec * file = map_folder->GetFileRec(i);
        if (!is_file_map(file))
            continue;

        cached_list_of_maps.push_back(std::string(file->m_RealName));
    }
}

void Lobby::select_map(u32 map_index)
{
    selected_map_index = map_index;
    load_map_texture();
}

std::string Lobby::get_selected_map_name()
{
    return cached_list_of_maps[selected_map_index];
}

void Lobby::load_map_texture()
{
    std::string texturename =  get_selected_map_name();
    texturename.replace(texturename.size() - 5, 5, ".jpg");

    if (!CFile::m_Packs->m_PackFiles[0]->m_RootFolder->FileExists("Matrix/Map/" + texturename))
    {
        map_texture = nullptr;
        return;
    }

    texturename.replace(texturename.size() - 4, 4, "");
    std::wstring wfilename(texturename.begin(), texturename.end());
    std::wstring path = L"Matrix\\Map\\" + wfilename;

    CTextureManaged * tex = (CTextureManaged *)g_Cache->Get(CacheClass::TextureManaged, path);
    map_texture = tex->Tex();
    // std::cout << "";
    //
    // CStorage stor(g_CacheHeap);
    // stor.Load(m_Name.c_str());
    //
    // CHsFolder * map_folder = CFile::m_Packs->m_PackFiles[0]->m_RootFolder->GetFolderEx("Matrix/Map");
    // SFileRec *texture_file = map_folder->GetFileRec( texturename );
    //
    // // texture_file->m_Extra
    //
    // HRESULT hr = D3DXCreateTextureFromFileInMemory(
    //     g_D3DD,
    //     reinterpret_cast<void *>(texture_file->m_Extra),
    //     texture_file->m_Size,
    //     map_texture
    // );
    //
    // // HRESULT hr = D3DXCreateTextureFromFileA(
    // //     pDevice,                // Your DX9 Device
    // //     "DATA/my_image.png",    // Path to your image
    // //     &myTexture              // The pointer to output to
    // // );
    // //
    // if (FAILED(hr)) {
    //     std::cout << "";
    //     // Handle error: texture failed to load
    // }
}

void Lobby::draw()
{
    ImVec2 button_dimensions = ImVec2(150, 35);
    // 1. Get the size of the main OS window ImGui is rendering inside
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 screenSize = viewport->WorkSize;
    ImVec2 screenPos = viewport->WorkPos;

    // Calculate the size of exactly one half of the screen
    ImVec2 halfSize(screenSize.x / 2.0f, screenSize.y);

    // 2. Define the flags that lock the window in place
    ImGuiWindowFlags lockedFlags =
        ImGuiWindowFlags_NoMove |          // Prevent dragging
        ImGuiWindowFlags_NoResize |        // Prevent resizing
        ImGuiWindowFlags_NoCollapse |      // Prevent minimizing
        ImGuiWindowFlags_NoTitleBar |      // Hide the title bar (optional)
        ImGuiWindowFlags_NoSavedSettings;  // Don't save this layout to imgui.ini

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.01f, 0.01f, 0.05f, 0.95f));

    // ==========================================
    // LEFT WINDOW (SERVER)
    // ==========================================

    ImGui::SetNextWindowPos(screenPos);
    // Force the next window to take up exactly the left half
    ImGui::SetNextWindowSize(halfSize);
    if (ImGui::Begin("ServerPanel", nullptr, lockedFlags))
    {
        ImGui::Text("Select Map");
        const char * preview = get_selected_map_name().c_str();
        if (ImGui::BeginCombo("##Select Map", preview))
        {
            for (int i = 0; i < cached_list_of_maps.size(); i++)
            {
                // Check if this specific item is the currently selected one
                const bool isSelected = (selected_map_index == i);

                // Draw the selectable item
                if (ImGui::Selectable(cached_list_of_maps[i].c_str(), isSelected))
                {
                    select_map(i);
                }

                // Set the initial focus when opening the combo (helps with keyboard navigation)
                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::ArrowButton("##PrevMap", ImGuiDir_Left))
        {
            if (selected_map_index != 0)
                select_map(selected_map_index - 1);
            else
                select_map(cached_list_of_maps.size() - 1);
        }

        ImGui::SameLine();
        if (ImGui::ArrowButton("##NextMap", ImGuiDir_Right))
        {
            if (selected_map_index == cached_list_of_maps.size() - 1)
                select_map(0);
            else
                select_map(selected_map_index + 1);
        }

        // ImGui::Text("Selected Map: %s", get_selected_map_name().c_str());

        ImVec2 imageSize(170.0f, 170.0f);
        if (map_texture != nullptr)
        {
            // Draw the image
            ImGui::Image((void*)map_texture, imageSize);
        }
        else
        {
            // Texture is missing: draw a solid colored box as a fallback
            // 1. Reserve blank screen space in the layout for our box
            ImVec2 cursorPos = ImGui::GetCursorScreenPos(); // Top-left corner of the blank space
            ImGui::Dummy(imageSize);                             // Tells ImGui to advance layout cursor by 'size'

            // 2. Calculate the top-left and bottom-right points of the rectangle
            ImVec2 pMin = cursorPos;
            ImVec2 pMax = ImVec2(cursorPos.x + imageSize.x, cursorPos.y + imageSize.y);

            // 3. Define the color in 32-bit ABGR format (0xAABBGGRR)
            // IM_COL32(Red, Green, Blue, Alpha)
            ImU32 boxColor = IM_COL32(50, 50, 60, 255); // Dark greyish blue

            // 4. Draw the filled rectangle
            // AddRectFilled(pMin, pMax, color, rounding, flags)
            ImGui::GetWindowDrawList()->AddRectFilled(pMin, pMax, boxColor, 4.0f); // 4.0f rounding for subtle smooth corners

            // (Optional) You can overlay text directly on top of the fallback box
            // Put the cursor back to pMin so text draws inside the box
            ImGui::SetCursorScreenPos(ImVec2(pMin.x + 10.0f, pMin.y + 10.0f));
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No Preview Available");

            // Restore layout cursor position so next widgets sit below the box
            ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + imageSize.y + ImGui::GetStyle().ItemSpacing.y));
        }

        if (ImGui::Button("Game Settings", button_dimensions))
        {
            // ConnectToServer();
            // LaunchGame();
        }

        if (ImGui::Button("Start Game", button_dimensions))
        {
            // ConnectToServer();
            // LaunchGame();
        }
    }
    ImGui::End();

    // ==========================================
    // RIGHT WINDOW (CLIENT)
    // ==========================================
    ImVec2 rightPos(screenPos.x + halfSize.x, screenPos.y);
    ImGui::SetNextWindowPos(rightPos);
    // Force it to take up the remaining right half
    ImGui::SetNextWindowSize(halfSize);
    if (ImGui::Begin("ClientPanel", nullptr, lockedFlags))
    {
        if (ImGui::InputTextWithHint("Server IP", "127.0.0.1", input_server_ip, sizeof(input_server_ip)))
        {}

        if (ImGui::InputTextWithHint("Username", "YourNameHere", input_username, sizeof(input_username)))
        {}

        if (ImGui::Button("Join Game", button_dimensions))
        {

        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
