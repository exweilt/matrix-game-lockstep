// MatrixGame - SR2 Planetary battles engine
// Copyright (C) 2012, Elemental Games, Katauri Interactive, CHK-Games
// Licensed under GPLv2 or any later version
// Refer to the LICENSE file included

#include "RobotPalette.hpp"

#include "CHistory.h"
#include "../MatrixGame.h"
#include "../MatrixMap.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <toml++/toml.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <cctype>
#include <string>
#include <vector>

namespace {

constexpr wchar_t ROBOT_PALETTE_FILENAME[] = L"robots.palette.toml";

std::wstring GetExecutableDirectory() {
    wchar_t path[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    std::wstring dir(path, len);
    const auto slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return {};
    }
    return dir.substr(0, slash + 1);
}

std::string ToLowerAscii(std::string_view s) {
    std::string out(s);
    for (char &c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

int CheckWeaponLegality(SWeaponUnit *weapons, int weaponKind, int armorKind) {
    if (weaponKind != 5 && weaponKind != 7) {
        const int weapon_num = g_MatrixMap->m_RobotWeaponMatrix[armorKind - 1].cnt;
        for (int t = 0; t < weapon_num; t++) {
            if (!(g_MatrixMap->m_RobotWeaponMatrix[armorKind - 1].list[t].access_invert & (1 << (6))) &&
                !(g_MatrixMap->m_RobotWeaponMatrix[armorKind - 1].list[t].access_invert & (1 << (4))) &&
                weapons[t].m_Unit.m_nKind == 0) {
                return t;
            }
        }
    } else {
        const int common_weapon = g_MatrixMap->m_RobotWeaponMatrix[armorKind - 1].cnt;
        for (int t = 0; t < common_weapon; t++) {
            if ((g_MatrixMap->m_RobotWeaponMatrix[armorKind - 1].list[t].access_invert & (1 << (6))) ||
                (g_MatrixMap->m_RobotWeaponMatrix[armorKind - 1].list[t].access_invert & (1 << (4)))) {
                return t;
            }
        }
    }
    return -1;
}

bool ParseChassis(std::string_view name, ERobotUnitKind &out) {
    const std::string key = ToLowerAscii(name);
    if (key == "pneumatics" || key == "pneumatic") {
        out = RUK_CHASSIS_PNEUMATIC;
        return true;
    }
    if (key == "wheels" || key == "wheel") {
        out = RUK_CHASSIS_WHEEL;
        return true;
    }
    if (key == "tracks" || key == "track") {
        out = RUK_CHASSIS_TRACK;
        return true;
    }
    if (key == "hovercraft") {
        out = RUK_CHASSIS_HOVERCRAFT;
        return true;
    }
    if (key == "antigravs" || key == "antigravity") {
        out = RUK_CHASSIS_ANTIGRAVITY;
        return true;
    }
    return false;
}

bool ParseHull(std::string_view name, ERobotUnitKind &out) {
    const std::string key = ToLowerAscii(name);
    if (key == "monostack") {
        out = RUK_ARMOR_MONOSTACK;
        return true;
    }
    if (key == "birex") {
        out = RUK_ARMOR_BIREX;
        return true;
    }
    if (key == "diploid") {
        out = RUK_ARMOR_DIPLOID;
        return true;
    }
    if (key == "paragon") {
        out = RUK_ARMOR_PARAGON;
        return true;
    }
    if (key == "trident") {
        out = RUK_ARMOR_TRIDENT;
        return true;
    }
    if (key == "fullstack") {
        out = RUK_ARMOR_FULLSTACK;
        return true;
    }
    return false;
}

bool ParseHead(std::string_view name, ERobotUnitKind &out) {
    const std::string key = ToLowerAscii(name);
    if (key == "blocker") {
        out = RUK_HEAD_BLOCKER;
        return true;
    }
    if (key == "dynamo") {
        out = RUK_HEAD_DYNAMO;
        return true;
    }
    if (key == "locator" || key == "lockator") {
        out = RUK_HEAD_LOCKATOR;
        return true;
    }
    if (key == "firewall") {
        out = RUK_HEAD_FIREWALL;
        return true;
    }
    return false;
}

bool ParseWeapon(std::string_view name, ERobotUnitKind &out) {
    const std::string key = ToLowerAscii(name);
    if (key == "machinegun") {
        out = RUK_WEAPON_MACHINEGUN;
        return true;
    }
    if (key == "cannon") {
        out = RUK_WEAPON_CANNON;
        return true;
    }
    if (key == "rocket_launcher" || key == "missile") {
        out = RUK_WEAPON_MISSILE;
        return true;
    }
    if (key == "firegun" || key == "flamethrower") {
        out = RUK_WEAPON_FLAMETHROWER;
        return true;
    }
    if (key == "grenade_launcher" || key == "mortar") {
        out = RUK_WEAPON_MORTAR;
        return true;
    }
    if (key == "laser") {
        out = RUK_WEAPON_LASER;
        return true;
    }
    if (key == "bomb") {
        out = RUK_WEAPON_BOMB;
        return true;
    }
    if (key == "plasmagun" || key == "plasma") {
        out = RUK_WEAPON_PLASMA;
        return true;
    }
    if (key == "discharger" || key == "electric") {
        out = RUK_WEAPON_ELECTRIC;
        return true;
    }
    if (key == "repair_tool" || key == "repair") {
        out = RUK_WEAPON_REPAIR;
        return true;
    }
    return false;
}

bool IsExtraWeapon(ERobotUnitKind kind) {
    return kind == RUK_WEAPON_BOMB || kind == RUK_WEAPON_MORTAR;
}

void InitRobotConfigTypes(SRobotConfig &cfg) {
    cfg.m_Chassis.m_nType = MRT_CHASSIS;
    cfg.m_Hull.m_Unit.m_nType = MRT_ARMOR;
    cfg.m_Head.m_nType = MRT_HEAD;
    for (int i = 0; i < MAX_WEAPON_CNT; i++) {
        cfg.m_Weapon[i].m_nType = MRT_WEAPON;
    }
}

bool AssignWeaponsToConfig(int armorKind, const std::vector<ERobotUnitKind> &weapons, SRobotConfig &cfg) {
    const int maxCommon = g_MatrixMap->m_RobotWeaponMatrix[armorKind - 1].common;
    const int maxExtra = g_MatrixMap->m_RobotWeaponMatrix[armorKind - 1].extra;

    int commonUsed = 0;
    int extraUsed = 0;
    int nextCommonSlot = 0;

    for (ERobotUnitKind kind : weapons) {
        if (kind == RUK_UNKNOWN) {
            return false;
        }
        if (IsExtraWeapon(kind)) {
            if (extraUsed >= maxExtra || cfg.m_Weapon[4].m_nKind != RUK_UNKNOWN) {
                return false;
            }
            cfg.m_Weapon[4].m_nKind = kind;
            extraUsed++;
        } else {
            if (commonUsed >= maxCommon) {
                return false;
            }
            while (nextCommonSlot < 4 && cfg.m_Weapon[nextCommonSlot].m_nKind != RUK_UNKNOWN) {
                nextCommonSlot++;
            }
            if (nextCommonSlot >= 4) {
                return false;
            }
            cfg.m_Weapon[nextCommonSlot].m_nKind = kind;
            commonUsed++;
            nextCommonSlot++;
        }
    }

    SWeaponUnit built[MAX_WEAPON_CNT]{};
    for (int i = 0; i < MAX_WEAPON_CNT; i++) {
        const ERobotUnitKind kind = cfg.m_Weapon[i].m_nKind;
        if (kind == RUK_UNKNOWN || kind == 0) {
            continue;
        }
        const int pos = CheckWeaponLegality(built, static_cast<int>(kind), armorKind);
        if (pos < 0) {
            return false;
        }
        built[pos].m_Unit.m_nKind = kind;
        built[pos].m_Unit.m_nType = MRT_WEAPON;
        built[pos].m_Pos = pos + 1;
    }

    return true;
}

void CollectWeaponNames(const toml::table &preset, std::vector<std::string> &out) {
    auto collect_node = [&](const toml::node &node) {
        if (const auto str = node.value<std::string>()) {
            out.push_back(*str);
        }
    };

    if (const toml::node *weapons = preset.get("weapons")) {
        if (const auto *arr = weapons->as_array()) {
            for (const auto &el : *arr) {
                collect_node(el);
            }
        } else {
            collect_node(*weapons);
        }
    }

    if (const toml::node *weapon = preset.get("weapon")) {
        if (const auto *arr = weapon->as_array()) {
            for (const auto &el : *arr) {
                collect_node(el);
            }
        } else {
            collect_node(*weapon);
        }
    }
}

bool ParsePresetTable(const toml::table &preset, SRobotConfig &cfg) {
    ZeroMemory(&cfg, sizeof(cfg));
    InitRobotConfigTypes(cfg);

    const auto chassisNode = preset.get("chassis");
    const auto hullNode = preset.get("hull");
    if (!chassisNode || !hullNode) {
        return false;
    }
    const auto chassisStr = chassisNode->value<std::string>();
    const auto hullStr = hullNode->value<std::string>();
    if (!chassisStr || !hullStr) {
        return false;
    }
    if (!ParseChassis(*chassisStr, cfg.m_Chassis.m_nKind) || !ParseHull(*hullStr, cfg.m_Hull.m_Unit.m_nKind)) {
        return false;
    }

    if (const toml::node *headNode = preset.get("head")) {
        if (const auto headStr = headNode->value<std::string>()) {
            if (!ParseHead(*headStr, cfg.m_Head.m_nKind)) {
                return false;
            }
        }
    }

    std::vector<std::string> weaponNames;
    CollectWeaponNames(preset, weaponNames);

    std::vector<ERobotUnitKind> weaponKinds;
    weaponKinds.reserve(weaponNames.size());
    for (const std::string &name : weaponNames) {
        ERobotUnitKind kind = RUK_UNKNOWN;
        if (!ParseWeapon(name, kind)) {
            return false;
        }
        weaponKinds.push_back(kind);
    }

    return AssignWeaponsToConfig(static_cast<int>(cfg.m_Hull.m_Unit.m_nKind), weaponKinds, cfg);
}

}  // namespace

void LoadRobotPaletteFromExecutableDirectory(CHistory *history) {
    if (!history || !g_MatrixMap) {
        return;
    }

    const std::wstring dir = GetExecutableDirectory();
    if (dir.empty()) {
        return;
    }

    const std::wstring palettePath = dir + ROBOT_PALETTE_FILENAME;

    toml::table root;
    try {
        root = toml::parse_file(palettePath);
    } catch (const toml::parse_error &) {
        return;
    }

    const toml::array *presets = root.get_as<toml::array>("robot_preset");
    if (!presets) {
        return;
    }

    for (const toml::node &node : *presets) {
        const toml::table *preset = node.as_table();
        if (!preset) {
            continue;
        }

        SRobotConfig cfg{};
        if (!ParsePresetTable(*preset, cfg)) {
            continue;
        }

        history->AddConfig(&cfg);
    }
}
