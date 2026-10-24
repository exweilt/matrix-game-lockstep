// MatrixGame - SR2 Planetary battles engine
// Copyright (C) 2012, Elemental Games, Katauri Interactive, CHK-Games
// Licensed under GPLv2 or any later version
// Refer to the LICENSE file included

#pragma once

#include "../MatrixConfig.hpp"

class CMatrixMapLogic;
class CMatrixSideUnit;

struct ScmSettingsState {
    bool file_present = false;
    bool enabled = true;

    double robot_construction_time_sec = 2.0;
    double turret_construction_time_sec = 15.0;
    double factory_capture_time_sec = 1.25;

    double income_period_titanium = 7.0;
    double income_period_electronics = 7.0;
    double income_period_energy = 7.0;
    double income_period_plasma = 7.0;
    double income_period_base = 7.0;

    int income_base = 3;
    int income_titanium = 10;
    int income_electronics = 10;
    int income_energy = 10;
    int income_plasma = 10;

    float friendly_fire_modifier = 1.0f;

    int robot_capacity_bonus_per_factory = 0;
    int robot_capacity_bonus_per_base = 0;
    int base_robot_queue_capacity = 6;

    int starting_titanium = 0;
    int starting_electronics = 0;
    int starting_energy = 0;
    int starting_plasma = 0;

    /** Internal hitpoint units (same as data.txt Building HITPOINT). Vanilla: base 50000, factory 30000. */
    int base_health = 50000;
    int factory_health = 30000;

    /** If false, map starting turrets (prop 0/1) are not spawned; building slots remain empty. Vanilla: true. */
    bool starting_turrets = true;
};

extern ScmSettingsState g_ScmSettings;

/** Load example.scm.toml from the executable directory (if present). */
void LoadScmSettingsFromExecutableDirectory();

/** Apply timings / capture to g_Config (call after g_Config.ReadParams). */
void ApplyScmSettingsToConfig();

/** Starting resources and friendly fire (call after map PrepareMap). */
void ApplyScmSettingsAfterMapLoad(CMatrixMapLogic *map);

bool ScmSettingsActive();

bool ScmShouldLoadStartingTurrets();

int ScmGetFactoryIncome(ERes resource);
int ScmGetBaseIncome();
int ScmGetMaxBuildQueueForSide(const CMatrixSideUnit *side);

/** Multiplier on friendly-fire damage (0 = none, 1 = default, >1 = amplified). */
float ScmGetFriendlyFireDamageScale(int victim_side);
