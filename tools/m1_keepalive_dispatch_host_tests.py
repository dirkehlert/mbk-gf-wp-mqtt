#!/usr/bin/env python3
"""Execute the actual KEEPALIVE/KEEPALIVEC dispatch before ordinary-message UI."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'examples/companion_radio/MyMesh.cpp').read_text()
a=s.index('static bool isExactTextCommand(');b=s.index('void MyMesh::formatKeepaliveReply',a)
exact=s[a:b]
a=s.index('void MyMesh::onMessageRecv(');b=s.index('  if (isMonitorHeatstripRequest(text))',a)
handler=s[a:b]+'  ordinary++;\n}\n'
fixture=r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <string>
#define MAX_TEXT_LEN 160
namespace mesh {struct Packet {};}
namespace StrHelper {void strncpy(char* d,const char* s,size_t n){std::strncpy(d,s,n-1);d[n-1]=0;}}
struct ContactInfo {};
class MyMesh {
public:
 bool keepalive_enabled=true,channel_ok=true,room_ok=true;
 int keepalive_received_count=0,keepalive_last_timestamp=0;
 int direct=0,channels=0,rooms=0,errors=0,ordinary=0,formats=0;
 char keepalive_last_scope[32]={},keepalive_last_repeater[32]={};
 char monitor_latest_scope[32]="bsmesh",monitor_latest_repeater[32]="AB";
 std::string channel_body,room_body;
 struct Clock{int getCurrentTime(){return 123;}int getCurrentTimeUnique(){return 124;}}clock;
 Clock* getRTCClock(){return &clock;}
 void markConnectionActive(const ContactInfo&){}
 void formatKeepaliveReply(const char* label,char* out,size_t){formats++;strcpy(out,label);strcat(out," M1 V:4.12 H:2 S:bsmesh R:AB");}
 bool sendKeepaliveReply(const ContactInfo&,const mesh::Packet*,const char*){direct++;return true;}
 bool sendKeepaliveChannel(const char* name,const mesh::Packet*,const char*,const char* text){assert(!strcmp(name,"MBKCHN1"));channels++;channel_body=text;return channel_ok;}
 bool sendKeepaliveRoomCopy(const char* name,const char* text){assert(!strcmp(name,"GF MBK Room"));rooms++;room_body=text;return room_ok;}
 int sendMessage(const ContactInfo&,int,int,const char*,uint32_t&,uint32_t&){errors++;return 1;}
 void onMessageRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*);
};
'''
cases=r'''
int main(){
 ContactInfo from;mesh::Packet packet;MyMesh m;
 m.onMessageRecv(from,&packet,0,"  KEEPALIVEC\r\n");
 assert(m.channels==1 && m.rooms==1 && m.direct==0 && m.formats==1);
 assert(m.channel_body==m.room_body && m.errors==0 && m.keepalive_received_count==1);
 // A room-delivered report cannot retrigger a command loop.
 m.onMessageRecv(from,&packet,0,m.room_body.c_str());assert(m.ordinary==1 && m.rooms==1);
 m.onMessageRecv(from,&packet,0,"KEEPALIVE");assert(m.direct==1 && m.rooms==1);
 m.keepalive_enabled=false;m.onMessageRecv(from,&packet,0,"KEEPALIVEC");
 assert(m.rooms==1 && m.channels==1 && m.keepalive_received_count==3);
 m.keepalive_enabled=true;m.channel_ok=false;m.onMessageRecv(from,&packet,0,"KEEPALIVEC");
 assert(m.channels==2 && m.rooms==2 && m.errors==1);
 m.channel_ok=true;m.room_ok=false;m.onMessageRecv(from,&packet,0,"KEEPALIVEC");
 assert(m.channels==3 && m.rooms==3 && m.errors==1);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture+exact+handler+cases)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: real KEEPALIVEC handler copies exact payload once; KEEPALIVE/disabled unchanged; report cannot loop; channel/room failures isolated')
