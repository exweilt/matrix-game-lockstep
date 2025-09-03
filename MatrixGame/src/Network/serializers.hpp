#pragma once

#include "MatrixRobot.hpp"

template <class Archive>
void serialize(Archive& ar, CMatrixRobotAI& robot) {
    ar(
        CEREAL_NVP(robot.m_PosX),
        CEREAL_NVP(robot.m_PosY),
        cereal::make_nvp("hitpoints", robot.GetHitPoint())
    );
}

void serialize_map_into_json();