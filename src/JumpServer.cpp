////
// Evsio0n <i@evsio0n.com> 2024/01/20
//
// Jump Server Plugin for MCPE Bedrock Edition.
////

#include <Nlohmann/json.hpp>

#include "ll/api/plugin/NativePlugin.h"
#include "nlohmann/json_fwd.hpp"
#include <ll/api/command/DynamicCommand.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/service/Bedrock.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/network/packet/TransferPacket.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/level/Level.h>
#include <vector>

using namespace std::string_view_literals;

namespace fs {
using namespace std::filesystem;
using namespace nlohmann;
using std::fstream;
using std::getline;
using std::ios;
} // namespace fs
ll::Logger                logger("JumpServer"sv);
ll::plugin::NativePlugin* plugin;
void                      registerCommand(ll::plugin::NativePlugin& self) { plugin = &self; };

// plugin instance
#include <Plugin.h>

struct Server {
    std::string text;
    std::string server_ip;
    int         server_port;
};

struct JumpServerConfig {
    std::string         motd_ip_port;
    std::vector<Server> servers;
};

fs::json JumpServerConfigToJson(JumpServerConfig& Config) {
    fs::json jsonConfig;
    jsonConfig["motd_ip_port"] = Config.motd_ip_port;
    for (const auto& server : Config.servers) {
        jsonConfig["servers"].push_back({
            {"text",        server.text       },
            {"server_ip",   server.server_ip  },
            {"server_port", server.server_port}
        });
    }
    return jsonConfig;
}

JumpServerConfig JumpServerConfigFromJson(fs::json& jsonConfig) {
    JumpServerConfig Config;
    if (jsonConfig.contains("motd_ip_port")) {
        Config.motd_ip_port = jsonConfig["motd_ip_port"].get<std::string>();
    }
    if (jsonConfig.contains("servers")) {
        for (const auto& server : jsonConfig["servers"]) {
            Config.servers.push_back(
                {server["text"].get<std::string>(),
                 server["server_ip"].get<std::string>(),
                 server["server_port"].get<int>()}
            );
        }
    }
    return Config;
}

std::vector<std::string> getServerList(const JumpServerConfig& config) {
    std::vector<std::string> serverList;
    for (const auto& server : config.servers) {
        serverList.push_back(server.text);
    }
    return serverList;
}


JumpServerConfig config;
#include <curl/curl.h>
#include <sstream>

size_t write_data(void* buffer, size_t size, size_t nmemb, void* userp) {
    return static_cast<std::ostringstream*>(userp)->write(static_cast<const char*>(buffer), size * nmemb).tellp();
}

fs::json getServerInfo(const Server& server) {
    std::ostringstream stream;
    CURLcode           result;
    CURL*              curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(
            curl,
            CURLOPT_URL,
            (config.motd_ip_port + "/api?host=" + server.server_ip + ":" + std::to_string(server.server_port)).c_str()
        );
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
        result = curl_easy_perform(curl);

        if (result != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(result));
        }

        curl_easy_cleanup(curl);
    }

    fs::json response = fs::json::parse(stream.str());
    return response;
}


void sendCustomForm(Player& player);
void ServerINFO(Player& player, const Server& chosenServer);

void ServerINFO(Player& player, const Server& chosenServer) {
    // 使用异步的get请求获取服务器状态
    // 注意，这里的url需要根据你的API路径进行更换，以下url假设你的API服务和游戏服务器在同一台设备上
    fs::json serverStatus = getServerInfo(chosenServer);

    // 'online' 和 'max' 字段需要根据实际API返回的数据进行调整
    std::string status = serverStatus["status"];
    int         online = serverStatus["online"];
    int         max    = serverStatus["max"];

    // 创建 ModalForm 来显示服务器状态
    auto modal_form = std::make_shared<ll::form::ModalForm>(
        "Server Status",
        "Server: " + chosenServer.text + "\nStatus: " + status + "\nOnline: " + std::to_string(online)
            + "\nMax: " + std::to_string(max),
        "Go",
        "Cancel"
    );

    // 向玩家发送 ModalForm
    modal_form->sendTo(player, [chosenServer](Player& player, bool response) {
        // 处理 ModalForm 的回调
        if (response) {
            // 如果玩家点击 "Go" 按钮，执行服务器转移
            TransferPacket pkt = TransferPacket(chosenServer.server_ip, chosenServer.server_port);
            player.sendNetworkPacket(pkt);
        } else {
            // 如果玩家点击 "Cancel" 按钮，重新发送自定义表单
            sendCustomForm(player);
        }
    });
}

void sendCustomForm(Player& player) {
    // 创建一个新的 CustomForm 对象
    auto form = std::make_shared<ll::form::CustomForm>("Server Select");
    logger.info("create form");
    form->appendDropdown("server-drop-down", "Server List", getServerList(config));
    // 向玩家发送该表单
    form->sendTo(player, [](Player& player, const ll::form::CustomFormResult& formData) {
        // 处理表单结果
        auto it = formData.find("server-drop-down"); // 查找特定表单元素的结果
        if (it != formData.end()) {
            auto& value = it->second;
            if (std::holds_alternative<std::string>(value)) { // 如果表单字段数据是 std::string 类型
                std::string selectedOption = std::get<std::string>(value);
                for (const auto& server : config.servers) {
                    if (server.text == selectedOption) {
                        ServerINFO(player, server);
                        return;
                    }
                }
            } else {
                // do nothing
            }
        }
    });
}


void loadConfig() {
    // if the config fire not exist, create it.
    logger.info("loading config...");
    const auto& configFilePath = plugin->getPluginDir().string() + "/config.json";
    logger.info("config file path: " + plugin->getPluginDir().string() + "/config.json");
    if (!std::filesystem::exists(configFilePath)) {
        std::ofstream(configFilePath) << R"({
                "motd_ip_port": "127.0.0.1:18800",
                "servers": [
                    {
                        "text": "servername",
                        "server_ip": "",
                        "server_port": 19132
                    }
                ]
            }
        )";
    }
    // load config
    auto theConfig = fs::json::parse(std::ifstream(configFilePath));

    config = JumpServerConfigFromJson(theConfig);
}


void registerCommand() {
    // Register commands.
    auto commandRegistry = ll::service::getCommandRegistry();
    if (!commandRegistry) {
        throw std::runtime_error("failed to get command registry");
    }
    // 创建一个 DynamicCommandInstance
    auto command = DynamicCommandInstance::create(commandRegistry, "js", "跨服传送", CommandPermissionLevel::Any);

    command->addOverload();
    // 设定执行回调
    command->setCallback([](DynamicCommand const&,
                            CommandOrigin const& origin,
                            CommandOutput&       output,
                            std::unordered_map<std::string, DynamicCommand::Result>&) {
        auto* entity = origin.getEntity();


        if (entity && entity->isType(ActorType::Player)) {
            auto* player = static_cast<Player*>(entity);
            sendCustomForm(*player);
            output.success("发送表单成功");
            return;
        } else {
            output.error("只有玩家可以使用这个命令");
            return;
        }
    });


    // 注册命令
    DynamicCommand::setup(commandRegistry, std::move(command));
}


void plugin_init() {
    // Code for loading the plugin goes here.
    logger.info("loading...");
    loadConfig();
    registerCommand();
    logger.info("loaded!");
}
