#!/usr/bin/env python3
"""Regression: real GFX font/bounds + DisplayDriver ellipsis for long M1 paths."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
lib=root/'.pio/libdeps/ThinkNode_M1_companion_radio_ble/Adafruit GFX Library'
gfx=(lib/'Adafruit_GFX.cpp').read_text()
def extract(text,signature):
 s=text.index(signature);i=text.index('{',s)+1;depth=1
 while depth:
  depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[s:i]
methods='\n'.join(extract(gfx,n) for n in ['void Adafruit_GFX::charBounds','void Adafruit_GFX::getTextBounds(const char'])
source=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
struct GFXglyph {uint16_t bitmapOffset;uint8_t width,height,xAdvance;int8_t xOffset,yOffset;};
struct GFXfont {uint8_t* bitmap;GFXglyph* glyph;uint16_t first,last;uint8_t yAdvance;};
#define pgm_read_glyph_ptr(f,i) (&(f)->glyph[i])
#include "Fonts/FreeSans9pt7b.h"
#include "DisplayDriver.h"
class Adafruit_GFX {
public:
 const GFXfont* gfxFont=&FreeSans9pt7b;
 uint8_t textsize_x=1,textsize_y=1;int16_t _width=200;bool wrap=true;
 void setTextWrap(bool value){wrap=value;}
 void charBounds(unsigned char,int16_t*,int16_t*,int16_t*,int16_t*,int16_t*,int16_t*);
 void getTextBounds(const char*,int16_t,int16_t,int16_t*,int16_t*,uint16_t*,uint16_t*);
};
'''
source=source.replace('#include "Fonts/FreeSans9pt7b.h"', (lib/'Fonts/FreeSans9pt7b.h').read_text().replace('#include <Adafruit_GFX.h>', '').replace('#pragma once', ''))
source+=methods+r'''
class Driver:public DisplayDriver {
public:
 Adafruit_GFX display;std::string printed;uint16_t printed_height=0;
 struct CRC{template<class T>void update(T){}}display_crc;
 void printWordWrap(const char*,int);
 Driver():DisplayDriver(128,128){}
 bool isOn(){return true;}void turnOn(){}void turnOff(){}void clear(){}
 void startFrame(ColorVal){}void endFrame(){}void setTextSize(int){}
 void setColor(ColorVal){}void setCursor(int,int){}
 void print(const char* s){printed=s;int16_t x,y;uint16_t w;display.getTextBounds(s,0,0,&x,&y,&w,&printed_height);}
 void fillRect(int,int,int,int){}void drawRect(int,int,int,int){}
 void drawXbm(int,int,const uint8_t*,int,int){}
 uint16_t getTextWidth(const char* s){
  int16_t x,y;uint16_t w,h;display.getTextBounds(s,0,0,&x,&y,&w,&h);
  return ceil((w+1)/1.5625f);
 }
};
void Driver::printWordWrap(const char* str, int max_width) PLACEHOLDER
int main(){
 Driver d;std::string path="0s 8 1A2B3C4D5E6F778899AABBCCDDEEFF00";
 // Reproduce the bug: wrapped bounds hide the true length from ellipsis.
 d.drawTextEllipsized(0,100,128,path.c_str());assert(d.printed==path);
 d.display.wrap=false;
 assert(d.getTextWidth(path.c_str())>128);
 d.drawTextEllipsized(0,100,128,path.c_str());
 assert(d.printed!=path && d.printed.find("...")!=std::string::npos);
 assert(d.getTextWidth(d.printed.c_str())<=128);
 for(const char* short_path:{"-","0s 0 -","1s 1 AB"}){
  d.drawTextEllipsized(0,100,128,short_path);assert(d.printed==short_path);
 }
 for(int n=1;n<70;n++){
  std::string p="0s "+std::string(n,'B');
  d.drawTextEllipsized(0,100,128,p.c_str());
  assert(d.getTextWidth(d.printed.c_str())<=128);
  assert(d.printed.find('\n')==std::string::npos);
 }
}
'''
assert 'display.setTextWrap(false);' in (root/'src/helpers/ui/GxEPDDisplay.cpp').read_text()
body=extract((root/'src/helpers/ui/GxEPDDisplay.cpp').read_text(),'void GxEPDDisplay::printWordWrap')
source=source.replace('void Driver::printWordWrap(const char* str, int max_width) PLACEHOLDER',body.replace('GxEPDDisplay::','Driver::'))
source=source.replace('for(const char* short_path:', 'd.printWordWrap(path.c_str(),128);assert(d.printed==path && d.printed_height>22 && !d.display.wrap);\n d.printWordWrap("one\\ntwo",128);assert(d.printed_height>22 && !d.display.wrap);\n d.drawTextEllipsized(0,100,128,path.c_str());assert(d.getTextWidth(d.printed.c_str())<=128);\n for(const char* short_path:')
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(lib),'-I'+str(root/'src/helpers/ui'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: real GFX message/newline wrap enabled only for body; path ellipsis stays single-line; 69 path lengths fit')
