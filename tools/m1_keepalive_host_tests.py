#!/usr/bin/env python3
"""Exercise actual KEEPALIVE send methods against simulated routing/ACK state."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
text = (ROOT/'examples/companion_radio/MyMesh.cpp').read_text()

def method(name):
    start=text.index('bool MyMesh::'+name+'(')
    opening=text.index('{',start); end=opening+1; depth=1
    while depth:
        depth += (text[end]=='{')-(text[end]=='}'); end+=1
    return text[start:end]

fixture=r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <string>
#define ADV_TYPE_ROOM 3
namespace StrHelper {void strncpy(char* d,const char* s,size_t n){if(n){std::strncpy(d,s,n-1);d[n-1]=0;}}}
struct SerialLog {template<class... T>void printf(const char*,T...){}} Serial;
#define MAX_TEXT_LEN 160
#define MAX_GROUP_CHANNELS 1
#define PUB_KEY_SIZE 32
#define OUT_PATH_UNKNOWN 255
#define MSG_SEND_FAILED 0
#define EXPECTED_ACK_TABLE_SIZE 8
namespace mesh {
struct Packet { bool flood=false; bool isRouteFlood() const {return flood;} };
struct GroupChannel {};
}
struct TransportKey { uint8_t key[16]={}; };
struct ContactInfo { char name[32]={}; uint8_t type=0; uint8_t out_path_len=OUT_PATH_UNKNOWN; struct { uint8_t pub_key[32]={}; } id; };
struct ChannelDetails { char name[32]="MBKCHN1"; mesh::GroupChannel channel; };
class MyMesh {
public:
  struct { uint8_t default_scope_key[16]={}; char node_name[32]="M1"; } _prefs;
  struct Clock { uint32_t getCurrentTimeUnique() {return 123;} } clock;
  struct Millis { unsigned long getMillis() {return 1000;} } ms;
  Millis* _ms=&ms;
  TransportKey send_scope, recorded_scope;
  bool send_unscoped=true, recorded_unscoped=false, incoming_known=false, send_ok=true, channel_exists=true;
  uint8_t recorded_path=0;
  struct Ack { unsigned long msg_sent=0; uint32_t ack=0; ContactInfo* contact=nullptr; } expected_ack_table[8];
  int next_ack_idx=0;
  unsigned long keepalive_sent_millis=0, keepalive_ack_deadline=0, keepalive_ack_trip_millis=0;
  uint32_t keepalive_expected_ack=0; uint8_t keepalive_ack_state=0;
  ContactInfo stored, contacts[4]; int contact_count=0,message_sends=0;
  std::string recorded_text, channel_text;
  int getNumContacts(){return contact_count;}
  bool getContactByIdx(uint32_t i,ContactInfo& c){if(i>=unsigned(contact_count))return false;c=contacts[i];return true;}
  void formatKeepaliveReply(const char*,char* out,size_t) {strcpy(out,"KEEPALIVE M1");}
  bool resolvePacketScopeKey(const mesh::Packet*,TransportKey& key) const {key.key[0]=42;return incoming_known;}
  Clock* getRTCClock() {return &clock;}
  unsigned long futureMillis(uint32_t delta) {return 1000+delta;}
  ContactInfo* lookupContactByPubKey(const uint8_t* key,int) {for(int i=0;i<contact_count;i++)if(!memcmp(key,contacts[i].id.pub_key,32))return &contacts[i];return &stored;}
  int sendMessage(const ContactInfo& c,uint32_t,int,const char* text,uint32_t& ack,uint32_t& timeout) {
    message_sends++;recorded_text=text;
    recorded_scope=send_scope;recorded_unscoped=send_unscoped;recorded_path=c.out_path_len;
    ack=send_ok?7:0;timeout=500;return send_ok?1:MSG_SEND_FAILED;
  }
  bool getChannel(int,ChannelDetails&) {return channel_exists;}
  bool sendGroupMessage(uint32_t,mesh::GroupChannel&,const char*,const char* text,size_t) {
    channel_text=text;
    recorded_scope=send_scope;recorded_unscoped=send_unscoped;return send_ok;
  }
  bool sendKeepaliveReply(const ContactInfo&,const mesh::Packet*,const char*);
  bool sendKeepaliveChannel(const char*,const mesh::Packet*,const char*,const char* = nullptr);
  bool sendKeepaliveRoomCopy(const char*,const char*);
};
'''
cases=r'''
int main() {
  MyMesh m; ContactInfo c; mesh::Packet req;
  m._prefs.default_scope_key[0]=9; m.send_scope.key[0]=77;
  // Unknown direct return path must use default scope, not an unrelated UI send scope.
  assert(m.sendKeepaliveReply(c,&req,"KEEPALIVE"));
  assert(m.recorded_path==OUT_PATH_UNKNOWN && m.recorded_scope.key[0]==9 && !m.recorded_unscoped);
  assert(m.send_scope.key[0]==77 && m.send_unscoped);
  assert(m.keepalive_expected_ack==7 && m.keepalive_ack_deadline==1500 && m.keepalive_ack_state==1);
  assert(m.expected_ack_table[0].contact==&m.stored);
  // A known direct route remains direct; a flooded request mirrors its usable scope.
  c.out_path_len=2;
  assert(m.sendKeepaliveReply(c,&req,"KEEPALIVE"));
  assert(m.recorded_path==2);
  req.flood=true;m.incoming_known=true;
  assert(m.sendKeepaliveReply(c,&req,"KEEPALIVE"));
  assert(m.recorded_path==OUT_PATH_UNKNOWN && m.recorded_scope.key[0]==42);
  // A channel response mirrors known scope or falls back to default, restoring sender state.
  assert(m.sendKeepaliveChannel("MBKCHN1",&req,"KEEPALIVEC"));
  assert(m.recorded_scope.key[0]==42 && !m.recorded_unscoped);
  m.incoming_known=false;
  assert(m.sendKeepaliveChannel("MBKCHN1",&req,"KEEPALIVEC"));
  assert(m.recorded_scope.key[0]==9 && !m.recorded_unscoped);
  assert(m.send_scope.key[0]==77 && m.send_unscoped);
  assert(!m.sendKeepaliveChannel("absent",&req,"KEEPALIVEC"));
  m.send_ok=false;
  assert(!m.sendKeepaliveReply(c,&req,"KEEPALIVE"));
  assert(m.keepalive_expected_ack==0 && m.keepalive_ack_deadline==0 && m.keepalive_ack_state==4);
  assert(m.send_scope.key[0]==77 && m.send_unscoped);
  // Exact room match only, and no side effects on missing/ambiguous targets.
  const char* copy="KEEPALIVEC M1 V:4.12 NF:-118 H:2 S:bsmesh R:AB POS:52.1,10.2";
  m.send_ok=true;int sends=m.message_sends;
  assert(!m.sendKeepaliveRoomCopy("GF MBK Room",copy) && m.message_sends==sends);
  m.contact_count=1;strcpy(m.contacts[0].name,"GF MBK Room");m.contacts[0].type=1;
  assert(!m.sendKeepaliveRoomCopy("GF MBK Room",copy) && m.message_sends==sends);
  m.contacts[0].type=ADV_TYPE_ROOM;m.contacts[0].out_path_len=2;m.contacts[0].id.pub_key[0]=42;
  assert(!m.sendKeepaliveRoomCopy("GF MBK Roo",copy) && m.message_sends==sends);
  assert(!m.sendKeepaliveRoomCopy(nullptr,copy) && !m.sendKeepaliveRoomCopy("GF MBK Room",""));
  // Same preformatted content in channel and room; preserve route and ACK state.
  m.keepalive_ack_state=2;m.keepalive_expected_ack=99;m.keepalive_ack_deadline=12345;
  assert(m.sendKeepaliveChannel("MBKCHN1",&req,"KEEPALIVEC",copy));
  int idx=m.next_ack_idx;
  assert(m.sendKeepaliveRoomCopy("GF MBK Room",copy));
  assert(m.recorded_text==copy && m.channel_text==m.recorded_text);
  assert(m.recorded_path==2 && m.recorded_scope.key[0]==9 && !m.recorded_unscoped);
  assert(m.send_scope.key[0]==77 && m.send_unscoped);
  assert(m.keepalive_ack_state==2 && m.keepalive_expected_ack==99 && m.keepalive_ack_deadline==12345);
  assert(m.expected_ack_table[idx].contact==&m.contacts[0]);
  // Missing channel must not prevent room sends; unknown room route uses default scope.
  m.channel_exists=false;m.contacts[0].out_path_len=OUT_PATH_UNKNOWN;
  assert(!m.sendKeepaliveChannel("MBKCHN1",&req,"KEEPALIVEC",copy));
  assert(m.sendKeepaliveRoomCopy("GF MBK Room",copy));
  assert(m.recorded_path==OUT_PATH_UNKNOWN && m.recorded_scope.key[0]==9);
  m.channel_exists=true;m.contact_count=2;m.contacts[1]=m.contacts[0];sends=m.message_sends;
  assert(!m.sendKeepaliveRoomCopy("GF MBK Room",copy) && m.message_sends==sends);
  assert(m.sendKeepaliveChannel("MBKCHN1",&req,"KEEPALIVEC",copy));
  m.contact_count=1;m.send_ok=false;idx=m.next_ack_idx;
  assert(!m.sendKeepaliveRoomCopy("GF MBK Room",copy) && m.next_ack_idx==idx);
  assert(m.send_scope.key[0]==77 && m.send_unscoped && m.keepalive_expected_ack==99);
}
'''
with tempfile.TemporaryDirectory() as d:
    source=Path(d)/'test.cpp';binary=Path(d)/'test'
    source.write_text(fixture+'\n'.join(method(n) for n in ['sendKeepaliveReply','sendKeepaliveChannel','sendKeepaliveRoomCopy'])+cases)
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS: KEEPALIVE routes/ACK; identical KEEPALIVEC room copy, exact unique room target, route/default scope, failure isolation, sender/ACK restoration')
