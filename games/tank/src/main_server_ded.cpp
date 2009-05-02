

#include "ConsoleApp.h"
#include "NetworkServer.h"
#include "ParameterManager.h"
#include "PuppetMasterServer.h"
#include "VersionInfo.h"
#include "Scheduler.h"
#include "GameLogicServer.h"

#ifdef _WIN32
#include <tchar.h>
#endif


unsigned g_next_level_index = 0;
PuppetMasterServer * g_puppet_master = NULL;
RegisteredFpGroup g_fp_group;


void onGameFinished();
void onLevelLoaded();
void loadNextLevel(void*);



//------------------------------------------------------------------------------
/**
 *  Schedules loading the next level in our level list
 */
void onGameFinished()
{
    s_scheduler.addEvent(SingleEventCallback(&loadNextLevel),
                         s_params.get<float>("server.logic.game_won_delay"),
                         NULL,
                         "loadNextLevel",
                         &g_fp_group);
}


//------------------------------------------------------------------------------
/**
 *  If a level is loaded manually, stop autorotate level load
 */
void onLevelLoaded()
{
    g_fp_group.deregisterAllOfType(TaskFp());
}

//------------------------------------------------------------------------------
void loadNextLevel(void*)
{
    std::vector<std::vector<std::string> > level_list =
        s_params.get<std::vector<std::vector<std::string> > >("server.settings.map_names");

    if (level_list.empty())                         throw Exception("Level list is empty");
    if (level_list[g_next_level_index].size() != 2) throw Exception("Invalid level list");
    
    g_puppet_master->loadLevel(
        new HostOptions(level_list[g_next_level_index][0],
                        level_list[g_next_level_index][1]));


    if (++g_next_level_index == level_list.size()) g_next_level_index = 0;    
}

//------------------------------------------------------------------------------
#ifdef _WIN32
    int _tmain(int argc, _TCHAR* argv[])
    {     
#else
    int main( int argc, char **argv )
        {
#endif
    try
    {
        s_params.loadParameters("config_server.xml");
        s_params.loadParameters("config_common.xml");

        s_params.loadParameters("data/config/teams.xml");
        s_params.loadParameters("data/config/weapon_systems.xml");
        s_params.loadParameters("data/config/tanks.xml");
        s_params.loadParameters("data/config/upgrade_system.xml");

        s_params.mergeCommandLineParams(argc, argv);
        
        s_log.open("./", "server");
        s_log.appendCr(true);
        s_log << "Version " << g_version << "\n";
        
        NetworkServer server;
        server.start();

        g_puppet_master = server.getPuppetMaster();
        g_puppet_master->addObserver(ObserverCallbackFun0(&onGameFinished),
                                     PMOE_GAME_FINISHED,
                                     &g_fp_group);
        g_puppet_master->addObserver(ObserverCallbackFun0(&onLevelLoaded),
                                     PMOE_LEVEL_LOADED,
                                     &g_fp_group);
        loadNextLevel(NULL);
        
        ConsoleApp app;
        app.run();
        
    } catch (Exception & e)
    {
        e.addHistory("main()");
        s_log << Log::error << e << "\n";
    }
    
    // to avoid singleton longevity issues
    g_fp_group.deregisterAllOfType(TaskFp());
    g_fp_group.deregisterAllOfType(ObserverFp(NULL, NULL, 0));
    
    return 0;
}
