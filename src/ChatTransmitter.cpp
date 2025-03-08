#include "Config.h"

#include "ChatTransmitter.h"
#include "ChatTransmitterScripts.h"
#include "Requests/RequestChat.h"
#include "Requests/RequestChatChannel.h"
#include "Requests/RequestQueryResult.h"
#include "Requests/RequestCommandResult.h"
#include "Requests/RequestAnticheatReport.h"
#include "Requests/RequestPositions.h"

#include "WorldSessionMgr.h"
#include "MapMgr.h"

#if __has_include("mod-anticheat/src/AnticheatMgr.h")
#include "mod-anticheat/src/AnticheatMgr.h"
#endif

namespace ModChatTransmitter
{
    Command::Command(const std::string& id)
        : id(id),
        output("")
    { }

    ChatTransmitter& ChatTransmitter::Instance()
    {
        static ChatTransmitter instance;
        return instance;
    }

    ChatTransmitter::ChatTransmitter()
        : wsClient(nullptr),
        dbManager(nullptr)
    {
        if (!IsEnabled())
        {
            return;
        }

        AddScripts();
    }

    bool ChatTransmitter::IsEnabled() const
    {
        return sConfigMgr->GetOption<bool>("ChatTransmitter.Enabled", false);
    }

    std::string ChatTransmitter::GetDiscordGuildId() const
    {
        return sConfigMgr->GetOption<std::string>("ChatTransmitter.DiscordGuildId", "");
    }

    std::string ChatTransmitter::GetBotWsHost() const
    {
        return sConfigMgr->GetOption<std::string>("ChatTransmitter.BotWsHost", "127.0.0.1");
    }

    std::string ChatTransmitter::GetBotWsKey() const
    {
        return sConfigMgr->GetOption<std::string>("ChatTransmitter.BotWsKey", "");
    }

    int ChatTransmitter::GetBotWsPort() const
    {
        return sConfigMgr->GetOption<int32>("ChatTransmitter.BotWsPort", 22141);
    }

    void ChatTransmitter::QueueChat(Player* player, uint32 type, std::string& msg)
    {
        if (wsClient && IsEnabled())
        {
            QueueRequest(new Requests::Chat(player, type, msg));
        }
    }

    void ChatTransmitter::QueueChat(Player* player, uint32 type, std::string& msg, Channel* channel)
    {
        if (wsClient && IsEnabled())
        {
            QueueRequest(new Requests::ChatChannel(player, type, msg, channel));
        }
    }

    void ChatTransmitter::QueuePositions(std::vector<nlohmann::json> &mapData)
    {
        if (wsClient && IsEnabled())
        {
            QueueRequest(new Requests::Positions(mapData));
        }
    }

    void ChatTransmitter::Update()
    {
        if (wsClient && IsEnabled())
        {
            std::vector<nlohmann::json> mapData;

            sMapMgr->DoForAllMaps([&mapData](Map* map)
            {
                nlohmann::json mapEntry;
                std::vector<nlohmann::json> players;

                map->DoForAllPlayers([&players](Player* player) {
                    Position pos = player->GetPosition();
                    uint32 zoneId, areaId;
                    player->GetZoneAndAreaId(zoneId, areaId);

                    nlohmann::json playerData;
                    playerData["guid"] = player->GetGUID().ToString();
                    playerData["name"] = player->GetName();
                    playerData["position"] = {
                        {"x", pos.GetPositionX()},
                        {"y", pos.GetPositionY()},
                        {"z", pos.GetPositionZ()},
                        {"o", pos.GetOrientation()}
                    };
                    playerData["alive"] = player->IsAlive();
                    playerData["zone"] = zoneId;
                    playerData["area"] = areaId;

                    players.push_back(playerData);
                });

                if (!players.empty())
                {
                    mapEntry["mapId"] = map->GetId();
                    mapEntry["players"] = players;
                    mapData.push_back(mapEntry);
                }
            });

            if (!mapData.empty())
            {
                QueuePositions(mapData);
            }

            // Receiving and responding to messages
            std::string json;
            while (wsClient && wsClient->GetReceivedMessage(json))
            {
            //     nlohmann::json item = nlohmann::json::parse(json);
            //     std::string message = item["message"].get<std::string>();
            //     nlohmann::json data = item["data"];
            //     std::string id = data["id"].get<std::string>();

            //     if (message == "command")
            //     {
            //         std::string command = data["command"].get<std::string>();
                    // HandleCommand(id, command);
            //     }
            //     else if (message == "query")
            //     {
            //         std::string query = data["query"].get<std::string>();
            //         int db = data["database"].get<int>();
            //         HandleQuery(id, query, (QueryDatabase)db);
            //     }
            // }

            // Requests::QueryResult* queryResult;
            // while (dbManager && dbManager->GetResult(queryResult))
            // {
                // QueueRequest(queryResult);
            // }
            }
        }
    }

    void ChatTransmitter::Stop()
    {
        if (wsClient)
        {
            wsClient->Close();
        }
        wsThread.join();

        if (dbManager)
        {
            dbManager->Stop();
        }
        dbThread.join();
    }

    void ChatTransmitter::AddScripts() const
    {
        ModChatTransmitter::AddPlayerScripts();
        ModChatTransmitter::AddWorldScripts();
    }

    void ChatTransmitter::Start()
    {
        if (!IsEnabled() || wsClient || dbManager)
        {
            return;
        }

        wsThread = std::thread(&ChatTransmitter::WebSocketThread, this);
        dbThread = std::thread(&ChatTransmitter::DatabaseThread, this);

#ifdef sAnticheatMgr
        sAnticheatMgr->OnReport += [this](Player* player, uint16 reportType)
        {
            QueueRequest(new Requests::AnticheatReport(player, reportType));
        };
#endif
    }

    void ChatTransmitter::DatabaseThread()
    {
        dbManager = new DatabaseManager();
        dbManager->Start(); // This will block until stopped

        delete dbManager;
        dbManager = nullptr;
    }

    void ChatTransmitter::WebSocketThread()
    {
        // The io_context is required for all I/O
        net::io_context ioc;

        // Launch the asynchronous operation
        wsClient = new WebSocketClient(ioc);
        std::string key = GetBotWsKey();
        wsClient->Run(GetBotWsHost(), GetBotWsPort(), "/?key=" + key);

        // Run the I/O service. The call will return when
        // the socket is closed.
        ioc.run();

        delete wsClient;
        wsClient = nullptr;
    }

    void ChatTransmitter::QueueRequest(IRequest* req)
    {
        if (!req)
        {
            return;
        }

        if (IsEnabled() && wsClient)
        {
            wsClient->QueueRequest(req);
        }
        else
        {
            delete req;
        }
    }

    void ChatTransmitter::HandleCommand(const std::string& id, const std::string& command)
    {
        Command* cmdObj = new Command(id);
        sWorld->QueueCliCommand(new CliCommandHolder(cmdObj, command.c_str(), ChatTransmitter::OnCommandOutput, ChatTransmitter::OnCommandFinished));
    }

    void ChatTransmitter::HandleQuery(const std::string& id, const std::string& query, QueryDatabase dbType)
    {
        if (dbManager)
        {
            dbManager->QueueQuery(id, query, dbType);
        }
        else
        {
            QueueRequest(new Requests::QueryResult(id, false));
        }
    }

    void ChatTransmitter::OnCommandOutput(void* arg, std::string_view text)
    {
        if (!text.empty() && arg)
        {
            Command* command = static_cast<Command*>(arg);
            command->output.append(text);
        }
    }

    void ChatTransmitter::OnCommandFinished(void* arg, bool success)
    {
        if (!arg)
        {
            return;
        }

        Command* command = static_cast<Command*>(arg);
        Instance().QueueRequest(new Requests::CommandResult(command->id, command->output, success));
        delete command;
    }
}
