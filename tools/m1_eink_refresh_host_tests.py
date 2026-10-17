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
fixture=r'''
#include <cassert>
#include <cstdint>
using ColorVal=uint16_t;
namespace UIColor {const ColorVal primary_txt=0;}
struct Panel {
 int full=0,partial=0,erases=0,poweroffs=0;
 void fillScreen(ColorVal) {} void setTextColor(ColorVal) {}
 void setFullWindow() {} int width(){return 200;} int height(){return 200;}
 void setPartialWindow(int,int,int,int) {}
 void clearScreen(int) {erases++;} void writeScreenBuffer(int) {erases++;}
 void display(bool part) {if(part)partial++;else full++;}
 void powerOff(){poweroffs++;} void hibernate(){poweroffs++;}
};
struct CRC {uint32_t value=1;void reset(){} uint32_t finalize(){return value;}};
class GxEPDDisplay {
public:
 Panel display;CRC display_crc;
 bool _isOn=true,_init=true;
 ColorVal _curr_color=0;
 int last_display_crc_value=0;
 void begin(){_init=true;}
 void turnOn();void turnOff();void startFrame(ColorVal);void endFrame();
};
'''
cases=r'''
int main(){
 GxEPDDisplay d;
 d.startFrame(1);d.endFrame();assert(d.display.partial==1 && d.display.full==0);
 d.startFrame(1);d.endFrame();assert(d.display.partial==1);
 for(int i=0;i<30;i++){d.display_crc.value++;d.startFrame(1);d.endFrame();}
 assert(d.display.partial==31 && d.display.full==0 && d.display.erases==0);
 d.turnOff();assert(!d._isOn && d.display.erases==0 && d.display.poweroffs==0);
 d.turnOn();assert(d._isOn);d.display_crc.value++;d.startFrame(1);d.endFrame();
 assert(d.display.partial==32 && d.display.poweroffs==0 && d.display.erases==0);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);f=p/'test.cpp';binary=p/'test'
 f.write_text(fixture+'\n'.join(method(n) for n in ['turnOn','turnOff','startFrame','endFrame'])+cases)
 subprocess.run(['clang++','-std=c++17','-DTHINKNODE_M1=1','-fsanitize=address,undefined',str(f),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
print('PASS: restored original partial refresh and unchanged CRC skip; idle/wake never clear or power down panel')
