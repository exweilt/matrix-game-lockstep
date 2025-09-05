#pragma once

#include "MatrixRobot.hpp"
#include <cereal/types/map.hpp>


template <class Archive>
void serialize(Archive& ar, CMatrixRobotAI& robot) {
    ar(
        CEREAL_NVP(robot.m_PosX),
        CEREAL_NVP(robot.m_PosY),
        cereal::make_nvp("hitpoints", robot.GetHitPoint())
    );
}

/**
 * Serializes some most relevant things of the game world for the current frame, calculates checksum of that state
 *      and saves that state as json file.
 *
 * @param calculate_checksum Should calculate checksum?
 * @param json_filename      Should write to file?
 * @return                   checksum, which is always 0 if the calculate_checksum is false.
 */
u64 serialize_map(bool calculate_checksum = true, std::optional<std::string> json_filename = std::nullopt);