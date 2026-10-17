#!/usr/bin/env python3
"""Exercise real M1 display state transitions with a simulated EPD controller."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
text=(root/'src/helpers/ui/GxEPDDisplay.cpp').read_text()
def method(name):
 s=text.index('void GxEPDDisplay::'+name+'(');i=text.index('{',s)+1;depth=1
 while depth:depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[s:i]
header=(root/'src/helpers/ui/GxEPDDisplay.h').read_text()
s=header.index('#ifndef EINK_MAX_PARTIAL_REFRESH');e=header.index('class GxEPDDisplay',s)
fixture=r'''
#include <cassert>
#include <cstdint>
using ColorVal=uint16_t;
namespace UIColor {const ColorVal primary_txt=0;}
struct Panel {
 struct Controller {
 int inits=0,cleans=0,syncs=0;uint8_t current=0xFF,previous=0xFF,visible=0xFF;
 void init(int,bool,int,bool){inits++;}
 void writeScreenBuffer(uint8_t value){current=value;}
 uint8_t erase_previous=0xFF;
 void refresh(bool part){
 assert(part && current==0xFF);
 if(cleans%2==0){assert(previous==visible);erase_previous=previous;}
 else {assert(visible==0xFF && previous==erase_previous);}
 visible=current;cleans++;
 }
 void writeScreenBufferAgain(uint8_t value){current=previous=value;syncs++;}
 } epd2;
 uint8_t buffer=0xFF;
 int full=0,partial=0,erases=0,poweroffs=0;
 void fillScreen(ColorVal) {buffer=0xFF;} void setTextColor(ColorVal) {}
 void setFullWindow() {} int width(){return 200;} int height(){return 200;}
 void setPartialWindow(int,int,int,int) {}
 void clearScreen(int) {erases++;full++;} void writeScreenBuffer(int) {erases++;}
 void display(bool part) {
 assert(epd2.visible==0xFF && epd2.previous==0xFF);
 assert(epd2.cleans==2*(partial+1) && epd2.syncs==partial+1);
 if(part)partial++;else full++;
 epd2.current=epd2.previous=epd2.visible=buffer;
 }
 void powerOff(){poweroffs++;} void hibernate(){poweroffs++;}
};
struct CRC {uint32_t value=1;void reset(){} uint32_t finalize(){return value;}};
class GxEPDDisplay {
public:
 Panel display;CRC display_crc;
 bool _isOn=true,_init=true;
 ColorVal _curr_color=0;
 int last_display_crc_value=0;
 int max_partial_refresh=EINK_MAX_PARTIAL_REFRESH;
 int _cycles_before_full_refresh=EINK_MAX_PARTIAL_REFRESH;
 void resetPartialRefreshCounter(){_cycles_before_full_refresh=max_partial_refresh;}
 void forceFullRefresh(){_cycles_before_full_refresh=0;}
 void begin(){_init=true;_isOn=true;forceFullRefresh();}
 void turnOn();void turnOff();void startFrame(ColorVal);void endFrame();
};
'''
cases=r'''
int main(){
 GxEPDDisplay d;assert(d.max_partial_refresh==-1);
 d.startFrame(1);d.display.buffer=0x12;assert(d.display.erases==0);d.endFrame();
 assert(d.display.epd2.visible==0x12);
 assert(d.display.full==0 && d.display.partial==1);
 d.startFrame(1);d.endFrame();assert(d.display.partial==1);
 for(int i=0;i<30;i++){d.display_crc.value++;d.startFrame(1);d.display.buffer=uint8_t(i+1);d.endFrame();assert(d.display.epd2.visible==uint8_t(i+1));}
 assert(d.display.full==0 && d.display.partial==31 && d.display.erases==0);
 d.turnOff();assert(!d._isOn && d.display.erases==0 && d.display.poweroffs==1);
 d.endFrame();assert(d.display.partial==31);
 d.turnOn();d.display_crc.value++;d.startFrame(1);d.endFrame();
 assert(d._isOn && d.display.partial==32 && d.display.full==0);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);f=p/'test.cpp';binary=p/'test'
 f.write_text(header[s:e]+fixture+'\n'.join(method(n) for n in ['turnOn','turnOff','startFrame','endFrame'])+cases)
 subprocess.run(['clang++','-std=c++17','-DTHINKNODE_M1=1','-fsanitize=address,undefined',str(f),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
print('PASS: two white erase pulses retain old history -> history sync -> buffered frame; 30 changes retain new pixels; unchanged/idle skip erase; no reset/full refresh')
