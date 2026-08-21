#include <hooks.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <MarioKartWii/Item/Obj/Gesso.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <kamek.hpp>


namespace Pulsar {
namespace Race {
    static bool givenStar = false;

    void restartStar(){
        givenStar = false;
    }

    void setStarOnTT(){
        static bool hasSetStar;
            const u32 gamemode = Racedata::sInstance->racesScenario.settings.gamemode;
            const bool isTT = DriverMgr::isTT;
            if(isTT==true && givenStar == false){
                Item::Manager::sInstance->players[0].inventory.SetItem(STAR, true);
                givenStar = true;
        }
        /*if(gamemode == MODE_TIME_TRIAL && givenStar == true){
            OS::Report("[TEST LOG DIO CANE]PulsarEngine: Giving player 1 a star for TT", 0);
            Item::Manager::sInstance->players[0].playerObj.UseItem(true);
            Item::Manager::sInstance->players[0].inventory.RemoveItems(1);
            givenStar = false;
        }*/
    }
RaceLoadHook restart(restartStar);
RaceFrameHook star(setStarOnTT);
}
}