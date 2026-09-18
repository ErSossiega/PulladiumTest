#include <hooks.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <core/rvl/OS/OS.hpp>
#include <Settings/SettingsParam.hpp>
#include <Settings/Settings.hpp>


namespace Pulsar {
namespace Race {
    Item::Behavior copyItemArray[19];
    Item::Behavior randomItemArray[19];
    static bool isRandom = false;

    bool isVSRace;
    Item::Behavior helper;

    //void randomAHMoment(Item::Player& player);
    void randomItem(){
        for (int i = 0;i < 19; i++){
            copyItemArray[i] = Item::Behavior::behaviourTable[i];
        }
        isVSRace = DriverMgr::isVSRace;
        if(isVSRace==true && Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_MODE, SETTING_RADIO_RANDOM) == RANDDOMITEM_MODE_RANDOM){
            s32 seed = OS::GetTick(); 
            if(isRandom == false){    
                for (int i = 0; i<19; i++){
                    randomItemArray[i] = Item::Behavior::behaviourTable[i];
                    
                }
                OS::Report("[TEST LOG DIO CANE]PulsarEngine: copying the vanilla items as a security copy\n", 0);
                isRandom = true;
            }
            Random random(seed);
            for (int i = 18; i > 0; i --){
                    //Item::Behavior::behaviourTable[i] = copyItemArray[i]; items are the same as the copy with this line
                    int j = random.NextLimited(i + 1);
                        helper = randomItemArray[i];
                        randomItemArray[i] = randomItemArray[j];
                        randomItemArray[j] = helper;
                        OS::Report("[PULSAR LOG TEST DIO CANE]PulsarEngine: the number is %d:\n", j);
                }
            for (int i = 0; i < 19; i ++){
                Item::Behavior::behaviourTable[i] = randomItemArray[i];
            }
            OS::Report("[TEST LOG DIO CANE]PulsarEngine: items are the same as the copy for now\n", 0);
        }else{
            for (int i = 0; i<19; i++){
                Item::Behavior::behaviourTable[i] = copyItemArray[i];
            }
        }
}
RaceLoadHook randomItemHook(randomItem);


/*void randomAHMoment(Item::Player& player){ absurd value test, it HAS BEEN used for seeing if the table was working correctly
        OS::Report("[TEST LOG DIO CANE]PulsarEngine: Giving player 1 a random item for VS\n", 0);
        player.UseBullet();
}*/
}
}
