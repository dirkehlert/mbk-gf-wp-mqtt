#!/usr/bin/env python3
"""Execute the actual M1 reference driver against a register-recording base."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
base=r'''
#pragma once
#include <cstdint>
#include <vector>
#include <utility>
#define HIGH 1
#define pgm_read_byte(p) (*(p))
inline void delay(int){}
namespace GxEPD2 {enum Panel {GDEH0154D67};}
class GxEPD2_EPD {
public:
 std::vector<std::pair<uint8_t,std::vector<uint8_t>>> writes;
 GxEPD2_EPD(int16_t,int16_t,int16_t,int16_t,int16_t,uint32_t,uint16_t,uint16_t,GxEPD2::Panel,bool,bool,bool){}
 void init(){_initial_write=_initial_refresh=true;_using_partial_mode=_power_is_on=_hibernating=false;}
protected:
 bool _initial_write=true,_initial_refresh=true,_using_partial_mode=false,_power_is_on=false,_hibernating=false;
 int _rst=2;
 void _reset(){}
 void _waitWhileBusy(const char*,uint16_t){}
 void _writeCommand(uint8_t c){writes.push_back({c,{}});}
 void _writeData(uint8_t v){writes.back().second.push_back(v);}
};
'''
cases=r'''
#include <cassert>
#include <algorithm>
#include "ThinkNodeM1EPD.h"
int count(ThinkNodeM1EPD& d,int cmd){return std::count_if(d.writes.begin(),d.writes.end(),[&](auto& w){return w.first==cmd;});}
bool control(ThinkNodeM1EPD& d,int value){return std::any_of(d.writes.begin(),d.writes.end(),[&](auto& w){return w.first==0x22 && w.second.size()==1 && w.second[0]==value;});}
int main(){
 ThinkNodeM1EPD d(30,28,2,3);d.init();
 uint8_t image[5000];std::fill_n(image,5000,0xA5);
 d.writeImageForFullRefresh(image,0,0,200,200);
 // Both image planes populated before full refresh, as in the pinned driver.
 assert(count(d,0x24)>=1 && count(d,0x26)>=1 && control(d,0xF8));
 d.writes.clear();d.refresh(false);assert(control(d,0xF4));assert(!control(d,0xF7));
 d.writes.clear();d.writeImageAgain(image,0,0,200,200);
 // Crucial reference behavior: post-refresh write does not touch previous RAM.
 assert(count(d,0x24)==1 && count(d,0x26)==0);
 assert(d.writes.back().second.size()==5000);
 assert(std::all_of(d.writes.back().second.begin(),d.writes.back().second.end(),[](uint8_t x){return x==0xA5;}));
 d.writes.clear();d.powerOff();assert(control(d,0x83));assert(count(d,0x24)==0 && count(d,0x26)==0);
 d.writes.clear();d.writeImage(image,0,0,200,200);
 assert(control(d,0xF8) && count(d,0x24)==1 && count(d,0x26)==0);
 d.writes.clear();d.refresh(true);assert(control(d,0xFC));
 d.writes.clear();d.writeScreenBufferAgain(0xFF);
 assert(count(d,0x24)==1 && count(d,0x26)==0);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'GxEPD2_EPD.h').write_text(base);(p/'test.cpp').write_text(cases)
 driver=root/'lib/ThinkNodeM1EPD/src'
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(p),'-I'+str(driver),str(p/'test.cpp'),str(driver/'ThinkNodeM1EPD.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual M1 driver full F8/F4, current-only post-refresh RAM write, power-off 83, wake power-on, partial FC')
