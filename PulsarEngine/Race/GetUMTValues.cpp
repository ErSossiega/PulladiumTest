/*first i'll try to add the code in UltraMiniTurbos.cpp, then making it's own file*/
#include <Race/UltraMiniTurbos.hpp>
#include <MarioKartWii/Kart/KartMovement.hpp>
#include <MarioKartWii/Kart/KartLink.hpp>
/*int umt100() {
    return 1;
};
kmBranch(0x8057efe0, umt100);*/
//first hook that does nothing actually. but i had fun doing this.

//kmWrite16(0x8057efe0,1); this crash the game when doing a mt
//kmWrite16(80591208,1); this crash the game instantly at boot. and i'm dumb cuz i forgot the 0x at the start.
kmWrite16(0x808B5CC2,1); //with this the mt will charge in 1 single frame.