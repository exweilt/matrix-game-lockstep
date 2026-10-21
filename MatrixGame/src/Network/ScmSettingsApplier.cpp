// MatrixGame - SR2 Planetary battles engine
// Copyright (C) 2012, Elemental Games, Katauri Interactive, CHK-Games
// Licensed under GPLv2 or any later version
// Refer to the LICENSE file included

#include "ScmSettingsApplier.hpp"

#include "../MatrixGame.h"
#include "../MatrixMap.hpp"
#include "../MatrixObjectBuilding.hpp"
#include "../MatrixMapStatic.hpp"
#include "../MatrixSide.hpp"
#include "Network.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <toml++/toml.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <cmath>

ScmSettingsState g_ScmSettings;

namespace {

constexpr wchar_t SCM_SETTINGS_FILENAME[] = L"example.scm.toml";

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

int SecondsToMs(double seconds) {
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    return static_cast<int>(std::lround(seconds * 1000.0));
}

template<typename T>
void ReadOptional(const toml::table &table, const char *key, T &out) {
    const toml::node *node = table.get(key);
    if (!node) {
        return;
    }
    if (const auto value = node->value<T>()) {
        out = *value;
    }
}

void ApplyCaptureTimesMs(int total_full_capture_ms) {
    // Capture progress uses MAX_ZAHVAT_POINTS paint steps (see CMatrixBuilding::Capture).
    if (total_full_capture_ms < MAX_ZAHVAT_POINTS) {
        total_full_capture_ms = MAX_ZAHVAT_POINTS;
    }
    const int per_point_ms = total_full_capture_ms / MAX_ZAHVAT_POINTS;
    g_Config.m_CaptureTimePaint = per_point_ms;
    g_Config.m_CaptureTimeErase = per_point_ms;
    g_Config.m_CaptureTimeRolback = per_point_ms;
}

void CountSideBuildings(const CMatrixSideUnit *side, int &base_cnt, int &factory_cnt) {
    base_cnt = 0;
    factory_cnt = 0;
    if (!side || !g_MatrixMap) {
        return;
    }

    for (CMatrixMapStatic *ms = CMatrixMapStatic::GetFirstLogic(); ms; ms = ms->GetNextLogic()) {
        if (!ms->IsLiveBuilding() || ms->GetSide() != side->m_Id) {
            continue;
        }
        if (ms->IsBase()) {
            ++base_cnt;
        } else {
            ++factory_cnt;
        }
    }
}

}  // namespace

bool ScmSettingsActive() {
    return g_ScmSettings.file_present && g_ScmSettings.enabled;
}

int ScmGetFactoryIncome(ERes resource) {
    if (!ScmSettingsActive()) {
        return RESOURCES_INCOME;
    }
    switch (resource) {
        case TITAN:
            return g_ScmSettings.income_titanium;
        case ELECTRONICS:
            return g_ScmSettings.income_electronics;
        case ENERGY:
            return g_ScmSettings.income_energy;
        case PLASMA:
            return g_ScmSettings.income_plasma;
        default:
            return RESOURCES_INCOME;
    }
}

int ScmGetBaseIncome() {
    if (!ScmSettingsActive()) {
        return RESOURCES_INCOME_BASE;
    }
    return g_ScmSettings.income_base;
}

float ScmGetFriendlyFireDamageScale(int victim_side) {
    if (ScmSettingsActive()) {
        return g_ScmSettings.friendly_fire_modifier;
    }
    if (victim_side == static_cast<int>(g_Network.controllable_side_id)) {
        return g_MatrixMap->m_Difficulty.k_friendly_fire;
    }
    return 1.0f;
}

int ScmGetMaxBuildQueueForSide(const CMatrixSideUnit *side) {
    if (!ScmSettingsActive()) {
        return MAX_STACK_UNITS;
    }

    int base_cnt = 0;
    int factory_cnt = 0;
    CountSideBuildings(side, base_cnt, factory_cnt);

    int capacity = g_ScmSettings.base_robot_queue_capacity;
    capacity += factory_cnt * g_ScmSettings.robot_capacity_bonus_per_factory;
    capacity += base_cnt * g_ScmSettings.robot_capacity_bonus_per_base;
    if (capacity < 1) {
        capacity = 1;
    }
    return capacity;
}

void LoadScmSettingsFromExecutableDirectory() {
    g_ScmSettings = ScmSettingsState{};

    const std::wstring dir = GetExecutableDirectory();
    if (dir.empty()) {
        return;
    }

    const std::wstring path = dir + SCM_SETTINGS_FILENAME;

    toml::table root;
    try {
        root = toml::parse_file(path);
    } catch (const toml::parse_error &) {
        return;
    }

    g_ScmSettings.file_present = true;

    if (const toml::table *scm = root.get_as<toml::table>("scm")) {
        ReadOptional(*scm, "enabled", g_ScmSettings.enabled);
    }

    const toml::table *game = root.get_as<toml::table>("game_settings");
    if (!game) {
        return;
    }

    ReadOptional(*game, "robot_construction_time", g_ScmSettings.robot_construction_time_sec);
    ReadOptional(*game, "turret_construction_time", g_ScmSettings.turret_construction_time_sec);
    ReadOptional(*game, "factory_capture_time", g_ScmSettings.factory_capture_time_sec);

    ReadOptional(*game, "income_period_titanium", g_ScmSettings.income_period_titanium);
    ReadOptional(*game, "income_period_electronics", g_ScmSettings.income_period_electronics);
    ReadOptional(*game, "income_period_energy", g_ScmSettings.income_period_energy);
    ReadOptional(*game, "income_period_plasma", g_ScmSettings.income_period_plasma);
    ReadOptional(*game, "income_period_base", g_ScmSettings.income_period_base);

    ReadOptional(*game, "income_base", g_ScmSettings.income_base);
    ReadOptional(*game, "income_titanium", g_ScmSettings.income_titanium);
    ReadOptional(*game, "income_electronics", g_ScmSettings.income_electronics);
    ReadOptional(*game, "income_energy", g_ScmSettings.income_energy);
    ReadOptional(*game, "income_plasma", g_ScmSettings.income_plasma);

    ReadOptional(*game, "friendly_fire_modifier", g_ScmSettings.friendly_fire_modifier);

    ReadOptional(*game, "robot_capacity_bonus_per_factory", g_ScmSettings.robot_capacity_bonus_per_factory);
    ReadOptional(*game, "robot_capacity_bonus_per_base", g_ScmSettings.robot_capacity_bonus_per_base);

    ReadOptional(*game, "starting_titanium", g_ScmSettings.starting_titanium);
    ReadOptional(*game, "starting_electronics", g_ScmSettings.starting_electronics);
    ReadOptional(*game, "starting_energy", g_ScmSettings.starting_energy);
    ReadOptional(*game, "starting_plasma", g_ScmSettings.starting_plasma);

    ReadOptional(*game, "base_health", g_ScmSettings.base_health);
    ReadOptional(*game, "factory_health", g_ScmSettings.factory_health);
}

void ApplyBuildingHealthToConfig() {
    g_Config.m_BuildingHitPoints[BUILDING_BASE] = g_ScmSettings.base_health;
    for (int kind = BUILDING_TITAN; kind < BUILDING_TYPE_CNT; ++kind) {
        g_Config.m_BuildingHitPoints[kind] = g_ScmSettings.factory_health;
    }
}

void ApplyScmSettingsToConfig() {
    if (!ScmSettingsActive()) {
        return;
    }

    g_Config.m_Timings[UNIT_ROBOT] = SecondsToMs(g_ScmSettings.robot_construction_time_sec);
    g_Config.m_Timings[UNIT_TURRET] = SecondsToMs(g_ScmSettings.turret_construction_time_sec);

    g_Config.m_Timings[RESOURCE_TITAN] = SecondsToMs(g_ScmSettings.income_period_titanium);
    g_Config.m_Timings[RESOURCE_ELECTRONICS] = SecondsToMs(g_ScmSettings.income_period_electronics);
    g_Config.m_Timings[RESOURCE_ENERGY] = SecondsToMs(g_ScmSettings.income_period_energy);
    g_Config.m_Timings[RESOURCE_PLASMA] = SecondsToMs(g_ScmSettings.income_period_plasma);
    g_Config.m_Timings[RESOURCE_BASE] = SecondsToMs(g_ScmSettings.income_period_base);

    ApplyCaptureTimesMs(SecondsToMs(g_ScmSettings.factory_capture_time_sec));

    if (g_ScmSettings.friendly_fire_modifier < 0.0f) {
        g_ScmSettings.friendly_fire_modifier = 0.0f;
    }

    ApplyBuildingHealthToConfig();
}

void ApplyScmSettingsAfterMapLoad(CMatrixMapLogic *map) {
    if (!ScmSettingsActive() || !map) {
        return;
    }

    map->m_Difficulty.k_friendly_fire = g_ScmSettings.friendly_fire_modifier;

    for (CMatrixMapStatic *ms = CMatrixMapStatic::GetFirstLogic(); ms; ms = ms->GetNextLogic()) {
        if (!ms->IsLiveBuilding()) {
            continue;
        }
        CMatrixBuilding *building = ms->AsBuilding();
        const int hp = (building->m_Kind == BUILDING_BASE) ? g_ScmSettings.base_health : g_ScmSettings.factory_health;
        if (hp > 0) {
            building->InitMaxHitpoint(static_cast<float>(hp));
        }
    }

    for (int i = 0; i < map->m_SideCnt; ++i) {
        CMatrixSideUnit *side = map->m_Side + i;
        side->SetResourceAmount(TITAN, g_ScmSettings.starting_titanium);
        side->SetResourceAmount(ELECTRONICS, g_ScmSettings.starting_electronics);
        side->SetResourceAmount(ENERGY, g_ScmSettings.starting_energy);
        side->SetResourceAmount(PLASMA, g_ScmSettings.starting_plasma);
    }
}
