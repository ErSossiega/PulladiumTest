#include <kamek.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/Item/PlayerObj.hpp>
#include <Settings/Settings.hpp>
#include <hooks.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>


namespace Pulsar{
    namespace Race{
        void MyMushroom(Kart::Movement& Movement){
        //bool isTT = DriverMgr::isTT; made before trying with the settings
        if (Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_TT, SETTINGTT_RADIO_ITEM) == TTSETTING_ITEM_DISABLED){
                Movement.ActivateBullet(0xFF);
            }else
            {
                Movement.ActivateMushroom();
            } 
        }
        kmCall(0x80798664, MyMushroom);  
    } 
}



