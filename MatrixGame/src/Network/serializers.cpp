#include "serializers.hpp"

#include <fstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include "MatrixRobot.hpp"

#include "Network.hpp"
#include "MatrixGame.h"

void serialize_map_into_json()
{
    std::ofstream ofs{g_Network.isClient2 ? "client2_map.json" : "client3_map.json"};
    cereal::JSONOutputArchive archive{ofs};

    CMatrixMapStatic *obj = CMatrixMapStatic::GetFirstLogic();
    while (obj)
    {
        if (obj->IsLiveRobot()) {
            archive( cereal::make_nvp("Robot " + std::to_string(obj->AsRobot()->m_NID),  *(obj->AsRobot())) );
        }
        obj = obj->GetNextLogic();
    }
}