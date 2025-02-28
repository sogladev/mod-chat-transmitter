#include "RequestPositions.h"
#include "../ChatTransmitter.h"

namespace ModChatTransmitter::Requests
{
    Positions::Positions(const std::vector<nlohmann::json>& data)
        : data(data)
    { }

    Positions& Positions::operator=(const Positions& other)
    {
        data = other.data;
        return *this;
    }

    std::string Positions::GetContents()
    {
        nlohmann::json jsonObj;
        jsonObj["message"] = "all-maps";
        jsonObj["data"] = data;
        return jsonObj.dump();
    }
}
