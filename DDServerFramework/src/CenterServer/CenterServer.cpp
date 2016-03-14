#include "socketlibfunction.h"
#include "WrapTCPService.h"
#include "NetSession.h"
#include "NetThreadSession.h"
#include "CenterServerSession.h"
#include "ox_file.h"
#include "WrapLog.h"
#include "timer.h"
#include "SSDBMultiClient.h"
#include "lua_readtable.h"
#include "UsePacketExtNetSession.h"
#include "HelpFunction.h"
#include "AutoConnectionServer.h"
#include "CenterServerPassword.h"
#include "app_status.h"

WrapLog::PTR            gDailyLogger;
SSDBMultiClient::PTR    gSSDBProcyClient;
WrapServer::PTR         gServer;
TimerMgr::PTR           logicTimerMgr;

extern void initCenterServerExt();

int main()
{
    int listenPort;
    string ssdbProxyIP;
    int ssdbProxyPort;
    
    try
    {
        struct msvalue_s config(true);
        struct lua_State* L = luaL_newstate();
        luaopen_base(L);
        luaL_openlibs(L);

        CenterServerPassword::getInstance().load(L);

        if (lua_tinker::dofile(L, "ServerConfig//CenterServerConfig.lua"))
        {
            aux_readluatable_byname(L, "CenterServerConfig", &config);
        }
        else
        {
            throw std::runtime_error("not found ServerConfig//CenterServerConfig.lua file");
        }

        map<string, msvalue_s*>& _submapvalue = *config._map;

        listenPort = atoi(map_at(_submapvalue, string("listenPort"))->_str.c_str());
        ssdbProxyIP = map_at(_submapvalue, string("ssdbProxyIP"))->_str;
        ssdbProxyPort = atoi(map_at(_submapvalue, string("ssdbProxyPort"))->_str.c_str());
        lua_close(L);
        L = nullptr;
    }
    catch (const std::exception& e)
    {
        errorExit(e.what());
    }

    logicTimerMgr = std::make_shared<TimerMgr>();
    gDailyLogger = std::make_shared<WrapLog>();
    gServer = std::make_shared<WrapServer>();
    gSSDBProcyClient = std::make_shared<SSDBMultiClient>();

    spdlog::set_level(spdlog::level::info);

    ox_dir_create("logs");
    ox_dir_create("logs/CenterServer");
    gDailyLogger->setFile("", "logs/CenterServer/daily");

    EventLoop mainLoop;

    gSSDBProcyClient->startNetThread([&](){
        mainLoop.wakeup();
    });

    gDailyLogger->info("try connect ssdb proxy server:{}:{}", ssdbProxyIP, ssdbProxyPort);
    gSSDBProcyClient->asyncConnectionProxy(ssdbProxyIP.c_str(), ssdbProxyPort, 5000, true);

    /*�����ڲ������������������߼�������������*/
    gDailyLogger->info("listen logic server port:{}", listenPort);
    ListenThread    logicServerListen;
    logicServerListen.startListen(listenPort, nullptr, nullptr, [&](int fd){
        WrapAddNetSession(gServer, fd, make_shared<UsePacketExtNetSession>(std::make_shared<CenterServerSession>()), 10000, 32*1024*1024);
    });

    gServer->startWorkThread(1, [&](EventLoop& el){
        syncNet2LogicMsgList(mainLoop);
    });

    gDailyLogger->info("server start!");

    initCenterServerExt();

    while (true)
    {
        if (app_kbhit())
        {
            string input;
            std::getline(std::cin, input);
            gDailyLogger->warn("console input {}", input);

            if (input == "quit")
            {
                break;
            }
        }

        mainLoop.loop(logicTimerMgr->IsEmpty() ? 1 : logicTimerMgr->NearEndMs());

        logicTimerMgr->Schedule();
        
        procNet2LogicMsgList();

        gSSDBProcyClient->pull();
        gSSDBProcyClient->forceSyncRequest();
    }

    logicServerListen.closeListenThread();
    gSSDBProcyClient->stopService();

    return 0;
}