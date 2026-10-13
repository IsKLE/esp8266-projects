#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <lwip/napt.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <gluedebug.h>
#include "Firewall.h"
#include "FlashButton.h"
#include "TrafficMeter.h"
#if !LWIP_FEATURES || LWIP_IPV6
#error This sketch requires IPv4 lwIP with NAPT (lm2f).
#endif

struct __attribute__((packed)) Config {
  uint32_t magic;
  char ssid[33], password[65], ap[33], apPassword[65];
  uint32_t checksum;
};
static_assert(sizeof(Config) == 204, "Provisioning layout changed");
Config config{};
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool screen = false, configured = false, natReady = false, natEnabled = false;
volatile uint32_t upstreamPackets = 0, blockedPackets = 0, childPackets = 0;
IPAddress apIP(192,168,4,1);
uint32_t wanAddress = 0;
char command[520]; size_t commandLength = 0; bool commandOverflow = false;
unsigned long lastScreen = 0, lastStatus = 0;
IPAddress lastWan;
constexpr uint8_t FLASH_BUTTON_GPIO = 0;
FlashButton flashButton;
TrafficMeter traffic;
uint8_t displayPage = 0;
uint32_t buttonEvents = 0;
volatile uint32_t apReceivedBytes = 0, apSentBytes = 0;

// Capture only lengths on the hotspot interface. No payload is stored or logged.
// Input = clients' upload; successful output = clients' download.
void captureTraffic(int interface, const char*, size_t length, int out, int success) {
  if (interface != SOFTAP_IF || !success) return;
  if (out) apSentBytes += length;
  else apReceivedBytes += length;
}

uint32_t configChecksum(const Config& c) {
  uint32_t crc = 0xffffffff;
  const uint8_t* b = reinterpret_cast<const uint8_t*>(&c);
  for (size_t i = 0; i < sizeof(c) - 4; ++i) {
    crc ^= b[i];
    for (int n = 0; n < 8; ++n) crc = (crc >> 1) ^ (0xedb88320 & (0u - (crc & 1)));
  }
  return ~crc;
}
bool validConfig(const Config& c) {
  if (c.magic != 0x3154414e || c.checksum != configChecksum(c)) return false;
  if (!memchr(c.ssid, 0, sizeof(c.ssid)) || !memchr(c.password, 0, sizeof(c.password)) ||
      !memchr(c.ap, 0, sizeof(c.ap)) || !memchr(c.apPassword, 0, sizeof(c.apPassword))) return false;
  return strlen(c.ssid) > 0 && strlen(c.ap) > 0 && strlen(c.password) >= 8 &&
    strlen(c.password) <= 63 && strlen(c.apPassword) >= 8 && strlen(c.apPassword) <= 63;
}
uint32_t addressNumber(IPAddress a) { return (uint32_t(a[0]) << 24) | (uint32_t(a[1]) << 16) | (uint32_t(a[2]) << 8) | a[3]; }

// ESP8266's lwIP glue bypasses netif.input. Linker wrapping is mandatory here.
extern "C" err_t __real_ethernet_input_LWIP2(struct pbuf*, struct netif*);
extern "C" err_t __wrap_ethernet_input_LWIP2(struct pbuf* p, struct netif* netif) {
  uint8_t head[114];
  size_t copied = pbuf_copy_partial(p, head, sizeof(head), 0);
  bool allow = true;
  if (netif->num == STATION_IF) {
    ++upstreamPackets;
    allow = RelayFirewall::allowUpstream(head, copied, p->tot_len, wanAddress);
  } else if (netif->num == SOFTAP_IF) {
    ++childPackets;
    if (copied < 14) allow = false;
    else if (RelayFirewall::be16(head + 12) == 0x0800) {
      if (copied < 34) allow = false;
      else {
        const uint8_t* ip = head + 14;
        const uint32_t src = RelayFirewall::be32(ip + 12), dst = RelayFirewall::be32(ip + 16);
        const uint32_t gateway = addressNumber(apIP);
        const bool dhcp = src == 0 && ip[9] == 17 && dst == 0xffffffff;
        allow = dhcp || ((src & 0xffffff00) == (gateway & 0xffffff00) && !(RelayFirewall::be16(ip + 6) & 0x3fff) &&
           (ip[9] == 6 || ip[9] == 17 || ip[9] == 1) && (natEnabled || dst == gateway || dst == 0xffffffff));
      }
    } else allow = RelayFirewall::be16(head + 12) == 0x0806;
  }
  if (!allow) { ++blockedPackets; pbuf_free(p); return ERR_OK; }
  return __real_ethernet_input_LWIP2(p, netif);
}

void reportStatus() {
  uint32_t freeHeap, maxBlock; uint8_t fragmentation;
  ESP.getHeapStats(&freeHeap, &maxBlock, &fragmentation);
  Serial.printf("NAT1 {\"connected\":%s,\"nat\":%s,\"firewall\":true,\"wan\":\"%s\",\"ap\":\"%s\",\"dns\":\"%s\",\"rssi\":%d,\"channel\":%d,\"clients\":%d,\"heap\":%u,\"upstreamPackets\":%u,\"childPackets\":%u,\"blocked\":%u,\"wifiStatus\":%d,\"uptime\":%lu,\"page\":%u,\"buttonEvents\":%u,\"flashPressed\":%s,\"cpuMHz\":%u,\"heapMaxBlock\":%u,\"heapFragmentation\":%u,\"upBps\":%u,\"downBps\":%u,\"peakUpBps\":%u,\"peakDownBps\":%u,\"apRxBytes\":%u,\"apTxBytes\":%u}\n",
    WiFi.status() == WL_CONNECTED ? "true":"false", natEnabled ? "true":"false",
    WiFi.localIP().toString().c_str(), apIP.toString().c_str(), WiFi.dnsIP(0).toString().c_str(),
    WiFi.RSSI(), WiFi.channel(), WiFi.softAPgetStationNum(), ESP.getFreeHeap(),
    upstreamPackets, childPackets, blockedPackets, WiFi.status(), millis()/1000,
    displayPage, buttonEvents, digitalRead(FLASH_BUTTON_GPIO)==LOW?"true":"false", ESP.getCpuFreqMHz(),
    maxBlock, fragmentation, traffic.upBps, traffic.downBps, traffic.peakUpBps, traffic.peakDownBps, apReceivedBytes, apSentBytes);
}
void drawRate(const char* label, uint32_t bytesPerSecond) {
  oled.println(label);
  if (bytesPerSecond >= 1000000)
    oled.printf("%u.%u MB/s\n",bytesPerSecond/1000000,(bytesPerSecond%1000000)/100000);
  else
    oled.printf("%u.%u KB/s\n",bytesPerSecond/1000,(bytesPerSecond%1000)/100);
}
void drawWrappedValue(const char* text, uint8_t columns, uint8_t size) {
  oled.setTextSize(size);
  const int16_t x = (oled.width()-columns*6*size)/2;
  while (*text) {
    oled.setCursor(x,oled.getCursorY());
    for(uint8_t n=0;n<columns && *text;++n) oled.write(*text++);
    oled.println();
  }
  oled.setTextSize(1);
  oled.setCursor(0,oled.getCursorY());
}
void drawStatus() {
  if (!screen) return;
  oled.clearDisplay(); oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE); oled.setCursor(0,0);
  oled.setTextWrap(true);
  if (displayPage == 2) {
    uint32_t freeHeap, maxBlock; uint8_t fragmentation;
    ESP.getHeapStats(&freeHeap, &maxBlock, &fragmentation);
    oled.println(F("PERF 3/3"));
    oled.printf("CPU %uMHz\n", ESP.getCpuFreqMHz());
    oled.printf("Free %uK\n",freeHeap/1024);
    oled.printf("Frag %u%%\n",fragmentation);
    drawRate("UP",traffic.upBps); drawRate("DN",traffic.downBps);
    drawRate("MAX UP",traffic.peakUpBps); drawRate("MAX DN",traffic.peakDownBps);
    oled.setCursor(0,104); oled.println(F("UPTIME"));
    oled.printf("%lus",millis()/1000);
    oled.display(); return;
  }
  if (displayPage == 1) {
    oled.println(F("WIFI 2/3"));
    if (configured) {
      oled.setCursor(0,16); oled.println(F("SSID"));
      drawWrappedValue(config.ap,10,1);
      oled.setCursor(0,oled.getCursorY()+8); oled.println(F("PASSWORD"));
      const uint8_t bigRows = (strlen(config.apPassword)+3)/4;
      if(strlen(config.apPassword)<=16 && oled.getCursorY()+8+bigRows*16<=128) {
        oled.setCursor(0,oled.getCursorY()+8);
        drawWrappedValue(config.apPassword,4,2);
      } else drawWrappedValue(config.apPassword,10,1);
    } else {oled.println(F("USB SETUP"));oled.println(F("REQUIRED"));}
    oled.display(); return;
  }
  oled.println(F("NET 1/3"));
  if (!configured) {oled.println(F("USB SETUP"));oled.println(F("REQUIRED"));oled.display();return;}
  oled.printf("%.20s\n", config.ssid);
  oled.println(F("WAN IP"));
  oled.println(WiFi.status()==WL_CONNECTED ? WiFi.localIP().toString():String(F("CONNECT")));
  oled.println(F("AP IP")); oled.println(apIP.toString());
  oled.printf("Clients %d\n", WiFi.softAPgetStationNum());
  oled.printf("RSSI %d\n", WiFi.RSSI());
  oled.println(natEnabled ? F("NAT ON") : F("NAT OFF"));
  oled.println(F("IN BLOCK"));
  oled.printf("D:%u\n%lus",blockedPackets,millis()/1000); oled.display();
}
int hexDigit(char c) { if (c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return c-'a'+10; if(c>='A'&&c<='F') return c-'A'+10; return -1; }
void selfTest() {
  uint8_t f[114]{}; f[12]=8; f[14]=0x45; f[16]=0; f[17]=40; f[23]=6;
  f[30]=192; f[31]=168; f[32]=0; f[33]=222; f[46]=0x50; f[47]=0x12;
  const uint32_t w=0xc0a800de; unsigned passed=0, total=0;
  auto check=[&](bool expected, size_t size=54) { ++total; if(RelayFirewall::allowUpstream(f,size,size,w)==expected) ++passed; };
  check(true); // SYN-ACK reply
  f[47]=2; check(false); // incoming SYN
  f[47]=16; check(true); // ACK
  f[32]=4; f[33]=2; check(false); // direct routed child, even ACK
  f[32]=0; f[33]=222; f[20]=0x20; check(false); f[20]=0;
  f[21]=1; check(false); f[21]=0;
  check(false,30); f[14]=0x44; check(false); f[14]=0x45;
  f[23]=17; f[17]=28; f[38]=0; f[39]=8; check(true,42);
  f[32]=4; f[33]=2; check(false,42);
  f[30]=f[31]=f[32]=f[33]=255; f[34]=0; f[35]=67; f[36]=0; f[37]=68; check(true,42);
  f[12]=0x86; f[13]=0xdd; check(false,42);
  f[12]=8; f[13]=6; check(true,42);
  Serial.printf("TEST1 {\"passed\":%u,\"total\":%u,\"ok\":%s}\n", passed,total,passed==total?"true":"false");
}
void handleCommand() {
  command[commandLength]=0;
  if (!strcmp(command,"STATUS")) reportStatus();
  else if (!strcmp(command,"TEST")) selfTest();
  else if (!strcmp(command,"TESTUI")) {
    const auto button = testFlashButton(); const auto meter = testTrafficMeter();
    Serial.printf("TESTUI {\"buttonPassed\":%u,\"buttonTotal\":%u,\"trafficPassed\":%u,\"trafficTotal\":%u,\"ok\":%s}\n",
      button.passed,button.total,meter.passed,meter.total,
      button.passed==button.total && meter.passed==meter.total?"true":"false");
  }
  else if (!strcmp(command,"REBOOT")) { Serial.println(F("OK REBOOT")); Serial.flush(); ESP.restart(); }
  else if (!strncmp(command,"CONFIG ",7) && commandLength == 7 + sizeof(Config)*2) {
    Config next{}; uint8_t* bytes=reinterpret_cast<uint8_t*>(&next); bool valid=true;
    for(size_t i=0;i<sizeof(next);++i) { int a=hexDigit(command[7+i*2]), b=hexDigit(command[8+i*2]); if(a<0||b<0) {valid=false;break;} bytes[i]=(a<<4)|b; }
    if(valid && validConfig(next)) {
      ip_napt_enable_no(SOFTAP_IF,0); natEnabled=false;
      EEPROM.put(0,next);
      if(EEPROM.commit()) { Serial.println(F("OK CONFIG SAVED")); Serial.flush(); delay(100); ESP.restart(); }
      else Serial.println(F("ERR CONFIG SAVE"));
    } else Serial.println(F("ERR CONFIG INVALID"));
    memset(&next,0,sizeof(next));
  } else Serial.println(F("ERR COMMAND"));
  memset(command,0,sizeof(command));
}
void setup() {
  Serial.begin(115200); Serial.println(F("NAT1 BOOT"));
  pinMode(FLASH_BUTTON_GPIO,INPUT_PULLUP);
  flashButton.begin(digitalRead(FLASH_BUTTON_GPIO)==LOW,millis());
  traffic.begin(millis(),0,0); phy_capture = captureTraffic;
  Wire.begin(14,12); Wire.setClock(400000);
  screen=oled.begin(SSD1306_SWITCHCAPVCC,0x3c,false,false);
  oled.setRotation(1); // Portrait: 64 pixels wide, 128 pixels high.
  EEPROM.begin(512); EEPROM.get(0,config); configured=validConfig(config);
  WiFi.persistent(false); WiFi.setAutoReconnect(true); WiFi.setSleepMode(WIFI_NONE_SLEEP);
  if(configured) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apIP,apIP,IPAddress(255,255,255,0));
    if(!WiFi.softAP(config.ap,config.apPassword,1,false,4)) Serial.println(F("ERR AP"));
    natReady=ip_napt_init(256,1)==ERR_OK; // One empty map slot; no mappings are installed.
    Serial.printf("NAT1 INIT %s OLED %s\n",natReady?"OK":"FAIL",screen?"OK":"FAIL");
    WiFi.begin(config.ssid,config.password);
  } else { WiFi.mode(WIFI_OFF); Serial.println(F("NAT1 CONFIG REQUIRED")); }
  drawStatus();
}
void loop() {
  if(flashButton.sample(digitalRead(FLASH_BUTTON_GPIO)==LOW,millis())) {
    displayPage = (displayPage+1)%3; ++buttonEvents;
    drawStatus(); reportStatus();
  }
  traffic.sample(millis(),apReceivedBytes,apSentBytes);
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='\n') { if(!commandOverflow) handleCommand(); else {memset(command,0,sizeof(command));Serial.println(F("ERR COMMAND LENGTH"));} commandLength=0;commandOverflow=false; }
    else if(c!='\r') { if(commandLength<sizeof(command)-1) command[commandLength++]=c; else commandOverflow=true; }
  }
  const bool connected=WiFi.status()==WL_CONNECTED;
  wanAddress=connected?addressNumber(WiFi.localIP()):0;
  if(connected && (lastWan!=WiFi.localIP() || !natEnabled)) {
    ip_napt_enable_no(SOFTAP_IF,0); natEnabled=false;
    if((addressNumber(apIP)&addressNumber(WiFi.subnetMask()))==(wanAddress&addressNumber(WiFi.subnetMask()))) {
      apIP=apIP[0]==192?IPAddress(10,42,0,1):IPAddress(172,29,4,1);
      WiFi.softAPConfig(apIP,apIP,IPAddress(255,255,255,0));
    }
    const bool overlap=(addressNumber(apIP)&addressNumber(WiFi.subnetMask()))==(wanAddress&addressNumber(WiFi.subnetMask()));
    if(natReady && !overlap && upstreamPackets > 0) {
      WiFi.softAPDhcpServer().setDns(WiFi.dnsIP(0));
      natEnabled=ip_napt_enable_no(SOFTAP_IF,1)==ERR_OK;
    }
    lastWan=WiFi.localIP();
  } else if(!connected && natEnabled) {ip_napt_enable_no(SOFTAP_IF,0);natEnabled=false;}
  if(millis()-lastScreen>=1000) { lastScreen=millis(); drawStatus(); }
  if(millis()-lastStatus>=5000) { lastStatus=millis(); reportStatus(); }
  delay(1);
}
