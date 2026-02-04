// #pragma once
//
// // #include "MatrixRobot.hpp"
// #include <cereal/types/map.hpp>
//
//
// #include <cereal/cereal.hpp>
// // Include types you use so Cereal knows how to handle them
// #include <cereal/types/vector.hpp>
// #include <cereal/types/string.hpp>
// #include <cereal/types/map.hpp>
// #include <xxhash.h>
//
// #include "Types.hpp"
//
// #include <cereal/details/traits.hpp>
//
//
// class XXHashArchive : public cereal::OutputArchive<XXHashArchive> {
//     XXH3_state_t* state;
//
// public:
//     XXHashArchive(XXH3_state_t* s) : cereal::OutputArchive<XXHashArchive>(this), state(s) {}
//
//     // 1. Handle NVPs (This solves the CEREAL_NVP errors)
//     template <typename T>
//     void save(const cereal::NameValuePair<T>& nvp) {
//         // We ignore the string name and just process the value
//         this->process(nvp.value);
//     }
//
//     // 2. Handle primitive types (The actual hashing)
//     // You can use a template to catch all "simple" data
//     template <typename T>
//     void save(const T& data) {
//         static_assert(std::is_arithmetic_v<T>, "Only arithmetic types handled here");
//         XXH3_64bits_update(state, &data, sizeof(T));
//     }
//
//     // 3. Optional: Special handler for std::string if your snapshots use them
//     void save(const std::string& str) {
//         XXH3_64bits_update(state, str.data(), str.size());
//     }
// };
//
// // Tell Cereal this is an Output Archive
// // namespace cereal {
// //     namespace traits {
// //         template <>
// //         struct is_output_archive<XXHashArchive> : std::true_type {};
// //     }
// // }
//
//
// template<typename T>
// u64 compute_hash(const T &obj)
// {
//     XXH3_state_t* state = XXH3_createState();
//     XXH3_64bits_reset(state);
//
//     {
//         XXHashArchive archive(state);
//         archive(obj);
//     }
//
//     u64 result = XXH3_64bits_digest(state);
//     XXH3_freeState(state);
//
//     return result;
// }
//
// // template <class Archive>
// // void serialize(Archive& ar, CMatrixRobotAI& robot) {
// //     ar(
// //         CEREAL_NVP(robot.m_PosX),
// //         CEREAL_NVP(robot.m_PosY),
// //         cereal::make_nvp("hitpoints", robot.GetHitPoint())
// //     );
// // }
// //
// // template <class Archive>
// // void serialize(Archive& ar, CMatrixBuilding& building) {
// //     ar(
// //         CEREAL_NVP(building.m_Kind),
// //         CEREAL_NVP(building.m_Side),
// //         cereal::make_nvp("hitpoints", building.GetHitPoint())
// //     );
// // }
//
// // /**
// //  * Serializes some most relevant things of the game world for the current frame, calculates checksum of that state
// //  *      and saves that state as json file.
// //  *
// //  * @param calculate_checksum Should calculate checksum?
// //  * @param json_filename      Should write to file?
// //  * @return                   checksum, which is always 0 if the calculate_checksum is false.
// //  */
// // u64 serialize_map(bool calculate_checksum = true, std::optional<std::string> json_filename = std::nullopt);
//
// // u64 compute_snapshot_hash(std::string &snapshot);