#ifndef _MOD_CHAT_TRANSMITTER_REQUESTS_REQUEST_POSITIONS_H_
#define _MOD_CHAT_TRANSMITTER_REQUESTS_REQUEST_POSITIONS_H_

#include "../../libs/nlohmann/json.hpp"

#include "../IRequest.h"
#include "../PlayerInfo.h"

namespace ModChatTransmitter::Requests
{
    class Positions : public IRequest
    {
    public:
        Positions(const std::vector<nlohmann::json>& data);

        Positions& operator=(const Positions& other);
        std::string GetContents() override;

    protected:
        std::vector<nlohmann::json> data;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Positions, data)
    };
}

#endif // _MOD_CHAT_TRANSMITTER_REQUESTS_REQUEST_POSITIONS_H_
