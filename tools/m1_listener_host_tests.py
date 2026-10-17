#!/usr/bin/env python3
"""Compile the actual UI listener adapters with small host fixtures.

Checks reply metadata and guards when MeshCore 1.18 dispatches text, CLI data,
and queue events. Hardware rendering and BLE still require an M1 smoke test.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
text = (ROOT / 'examples/companion_radio/ui-new/UITask.cpp').read_text()

def method(name):
    start = text.index('void UITask::' + name + '(')
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

fixture = r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <string>
constexpr int TXT_TYPE_PLAIN=0, TXT_TYPE_SIGNED_PLAIN=2, TXT_TYPE_CLI_DATA=1;
namespace mesh {
struct Packet {
  bool flood=true; uint8_t hashes=3;
  bool isRouteFlood() const { return flood; }
  uint8_t getPathHashCount() const { return hashes; }
};
struct GroupChannel { uint8_t index=7; };
}
struct ContactInfo { char name[32]="Alice"; struct { uint8_t pub_key[32]={42}; } id; };
struct ChannelDetails { mesh::GroupChannel channel; char name[32]="MBKCHN1"; };
struct MeshFixture {
  bool isDisplayableText(const char* s) const { return s && *s && *s != '\x01'; }
  void formatPacketScope(const mesh::Packet*, char* out, size_t n) { strncpy(out,"#de-mitte",n); }
  uint8_t getChannelIndex(const mesh::GroupChannel& c) { return c.index; }
} the_mesh;
enum class UIEventType { contactMessage, channelMessage };
class UITask {
public:
  int _msgcount=0, previews=0, clears=0, notifications=0;
  bool connected=false, reply_contact=false;
  uint8_t hops=0, channel=0;
  std::string scope, mention, body;
  bool hasConnection() { return connected; }
  void notify(UIEventType) { notifications++; }
  void msgRead(int n) { assert(n==0); clears++; }
  void newMsg(uint8_t h, const char*, const uint8_t* key, uint8_t c, const char* m, const char* s, const char* t, int) {
    previews++; hops=h; channel=c; reply_contact=key && key[0]==42;
    scope=s; mention=m ? m : ""; body=t;
  }
  void onMessageRecv(mesh::Packet*,const ContactInfo&,uint8_t,uint32_t,const char*);
  void onChannelMessageRecv(mesh::Packet*,ChannelDetails&,const char*);
  void onQueueSizeChanged(int);
};
'''
cases = r'''
int main() {
  mesh::Packet packet; ContactInfo from; ChannelDetails channel;
  UITask ui;
  ui.onMessageRecv(&packet,from,TXT_TYPE_PLAIN,0,"hello");
  assert(ui.previews==1 && ui.hops==3 && ui.reply_contact && ui.channel==0xFF);
  assert(ui.scope=="#de-mitte" && ui.notifications==1);
  ui.onQueueSizeChanged(4);
  assert(ui._msgcount==4 && ui.clears==0 && ui.previews==1);
  ui.onMessageRecv(&packet,from,TXT_TYPE_CLI_DATA,0,"CLI result");
  ui.onMessageRecv(&packet,from,3,0,"CLI request");
  ui.onMessageRecv(&packet,from,TXT_TYPE_PLAIN,0,"\x01");
  assert(ui.previews==1 && ui.notifications==1);
  packet.flood=false; ui.connected=true;
  ui.onMessageRecv(&packet,from,TXT_TYPE_SIGNED_PLAIN,0,"signed");
  assert(ui.previews==2 && ui.hops==0xFF && ui.notifications==1);
  ui.connected=false;
  ui.onChannelMessageRecv(&packet,channel,"Bob   : hello");
  assert(ui.previews==3 && ui.channel==7 && !ui.reply_contact && ui.mention=="Bob");
  assert(ui.scope=="#de-mitte" && ui.notifications==2);
  std::string long_name(80,'x'); long_name += ": hello";
  ui.onChannelMessageRecv(&packet,channel,long_name.c_str());
  assert(ui.mention.size()==31);
  ui.onQueueSizeChanged(0);
  assert(ui._msgcount==0 && ui.clears==1);
}
'''
with tempfile.TemporaryDirectory() as d:
    source = Path(d)/'test.cpp'
    source.write_text(fixture + '\n'.join(method(n) for n in ['onMessageRecv','onChannelMessageRecv','onQueueSizeChanged']) + cases)
    binary = Path(d)/'test'
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS: M1 listener bridge (reply metadata, CLI filtering, queue reset, long mentions)')
