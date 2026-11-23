#include "RadioNetwork.h"
#include "HfActivity.h"
#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <SI4735.h>
#include <Preferences.h>
#include <cstring>
#include <utility>
#include <type_traits>
#include "FixedText.h"
#include "Localization.h"
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "patch_init.h"

// Preserve the hardware-tested UI stack size. No TLS.
SET_LOOP_TASK_STACK_SIZE(24 * 1024);

// Radio KALIMAN: application written independently. Hardware settings obtained
// from the working user-supplied reference. Third-party dependencies retain
// their own licenses; patch_init.h retains its original provenance notice.
constexpr const char* FIRMWARE_NAME="Radio KALIMAN";
constexpr const char* FIRMWARE_VERSION="1.9.0";
constexpr const char* FIRMWARE_CREATOR="KALIMAN/KO6EAI";
TFT_eSPI screen;
TFT_eSprite canvas(&screen);
uint64_t previousRows[240]={0};
uint64_t rowFingerprint(const uint8_t* row){
 uint64_t hash=14695981039346656037ULL;
 for(int i=0;i<320;i++){hash^=row[i];hash*=1099511628211ULL;}
 return hash;
}
bool frameValid=false;
// Read-only access to the library's documented RDS response, no extra I2C.
class RdsReceiver : public SI4735 {
public:
  const si47x_rds_status& rdsResponse() const { return currentRdsStatus; }
  bool signalResponseValid() const {
    return currentRqsStatus.resp.CTS && !currentRqsStatus.resp.ERR &&
      currentRqsStatus.resp.RSSI<=127 && currentRqsStatus.resp.SNR<=127;
  }
};
RdsReceiver receiver;
Preferences prefs;
uint16_t BG=0x0022, PANEL=0x0927, ACCENT=0x07BF, INK=0xFFFF, DIM=0x9D16;
uint8_t theme=0;
uint8_t hudStyle=0;
constexpr const char* hudNames[]={"Orbitas","Consola","Observatorio","Trazo RSSI"};
constexpr uint8_t HUD_COUNT=sizeof(hudNames)/sizeof(hudNames[0]);
// Keep palette indices stable so saved selections remain valid.
const char* themeNames[]={"Oscuro Rainbow","Azul sonar","Ambar estudio","Grafito cyan","Bosque aurora","Violeta orbital","Cobre nocturno","Hielo polar","Arena nocturna","Monocromo","Fosforo radar","Petroleo marino","Rubi profundo","Titanio dorado","Orbita RX"};
constexpr uint8_t THEME_COUNT=sizeof(themeNames)/sizeof(themeNames[0]);
const uint16_t rainbow[]={0xF800,0xFD20,0xFFE0,0x07E0,0x07FF,0x001F,0xF81F};
uint16_t SNR_COLOR=0xB7FF;
struct HudPalette { uint16_t background,panel,rssi,snr,text,muted; };
// Paired signal colors, neutral readable text and quiet instrument surfaces.
const HudPalette hudPalettes[]={
  {0x0000,0x10A2,0x07FF,0xB7FF,0xFFFF,0xAD55},
  {0x0022,0x0927,0x3DFF,0x5FF5,0xEFFF,0x9D79},
  {0x0800,0x20C2,0xFDC0,0xFF2D,0xFF9C,0xCBB0},
  {0x0841,0x18E3,0x2EFF,0xA53F,0xEF7D,0xAD55},
  {0x0103,0x1247,0x5F92,0xDF6A,0xEFFE,0x9D94},
  {0x1004,0x28CA,0xC3FF,0x3EFF,0xF7BF,0xB4F8},
  {0x1000,0x28C3,0xFC0D,0xFECA,0xFF9C,0xCD16},
  {0x0023,0x1168,0x87FF,0x649F,0xEFFF,0x9D79},
  {0x1041,0x2924,0xEDED,0x6E78,0xFF9C,0xC5B3},
  {0x0000,0x2104,0xFFFF,0xBDF7,0xFFFF,0xBDF7},
  {0x0000,0x0820,0x07E0,0xBFE8,0xEFFE,0x8DD0},
  {0x0043,0x0186,0x2EF5,0x7DFF,0xEFFF,0x9DB8},
  {0x1002,0x2805,0xFA4C,0xFDD4,0xFF9D,0xD4B5},
  {0x10A2,0x2945,0xFEE0,0x6E7F,0xEF7D,0xBDF7},
  {0x0022,0x0865,0x3F19,0xC47F,0xEFFF,0x9D79}
};
void applyTheme(){
  frameValid=false;
  const HudPalette& p=hudPalettes[theme<THEME_COUNT?theme:1];
  BG=p.background;PANEL=p.panel;ACCENT=p.rssi;SNR_COLOR=p.snr;INK=p.text;DIM=p.muted;
}
// Shared theme roles: every HUD uses the same surfaces and accent treatment.
uint16_t mixColor(uint16_t a,uint16_t b,int weight){
  int r=((a>>11)*(255-weight)+(b>>11)*weight)/255;
  int g=(((a>>5)&63)*(255-weight)+((b>>5)&63)*weight)/255;
  int bl=((a&31)*(255-weight)+(b&31)*weight)/255;
  return (r<<11)|(g<<5)|bl;
}
uint16_t hudAccent(int x){
  // Orbita RX: a fixed cyan-to-violet sweep, shared by all HUD indicators.
  // No animation timer or additional redraws: reception timing is unchanged.
  if(theme==14)return mixColor(ACCENT,0xA35F,constrain(x,0,319)*200/319);
  if(theme!=0)return ACCENT;
  int t=constrain(x,0,319)*6*255/319;
  int i=t/255;
  return i>=6?rainbow[6]:mixColor(rainbow[i],rainbow[i+1],t%255);
}
// Shared recessed and raised surfaces; every shade derives from the theme.
void raisedSurface(int x,int y,int w,int h,uint16_t paper,int radius=4){
  radius=min(radius,(h-1)/2);
  canvas.fillRoundRect(x,y,w,h,radius,paper);
  canvas.drawRoundRect(x,y,w,h,radius,mixColor(paper,DIM,70));
  canvas.drawFastHLine(x+radius,y+1,w-2*radius,mixColor(paper,INK,34));
  canvas.drawFastHLine(x+radius,y+h-2,w-2*radius,mixColor(paper,BG,130));
}
void statePill(int x,int y,int w,int h,const Text& text,bool on){
  const uint16_t tint=hudAccent(x+w/2);
  uint16_t paper=on?mixColor(PANEL,tint,58):mixColor(PANEL,BG,100);
  raisedSurface(x,y,w,h,paper,h/2);
  if(on)canvas.drawRoundRect(x,y,w,h,(h-1)/2,tint);
  canvas.setTextDatum(MC_DATUM);canvas.setTextColor(INK);
  canvas.drawString(localized(text),x+w/2,y+h/2,1);
}
void hudPanel(int x,int y,int w,int h,int radius){
  raisedSurface(x,y,w,h,PANEL,radius);
}
void themeLine(int x,int y,int width){
  if(theme==14){
    for(int i=0;i<width;i++)canvas.drawPixel(x+i,y,hudAccent(x+i));
    return;
  }
  if(theme!=0){canvas.drawFastHLine(x,y,width,ACCENT);return;}
  for(int i=0;i<7;i++)canvas.drawFastHLine(x+i*width/7,y,(i+1)*width/7-i*width/7,rainbow[i]);
}
constexpr int RESET=12, LIGHT=14, MUTE=27, EA=17, EB=16, KEY=33;
enum class Page { Radio, Settings, Memories, About, Bands, Appearance, Reception, Gain, Audio, Sync, Seek, Network, WifiScan, Keyboard, Activity, Tuning };
enum class Control { Tune, Volume, Fine };
Page page=Page::Radio;
Control control=Control::Tune;
struct Band { const char* name; uint16_t lo,hi,initial; uint8_t preferredMode,preferredStep; };
const Band bands[]={
  {"FM",6400,10800,8930,0,0},
  {"OM / MW",522,1701,1395,1,2},
  {"OC / SW",1730,30000,15500,1,1},
  {"OL / LW",153,279,198,1,2},
  {"Balizas",280,470,284,1,0},
  {"630M",472,479,475,2,0},
  {"160M",1800,1910,1899,2,0},
  {"120M",2300,2495,2400,1,1},
  {"90M",3200,3400,3300,1,1},
  {"80M",3500,3800,3630,2,0},
  {"75M",3900,4000,3950,1,1},
  {"60M",5330,5410,5375,3,0},
  {"49M",5900,6200,6000,1,1},
  {"40M",7000,7200,7185,2,0},
  {"41M",7200,7450,7210,1,1},
  {"31M",9400,9900,9600,1,1},
  {"30M",10100,10150,10125,3,0},
  {"25M",11600,12100,11700,1,1},
  {"22M",13570,13870,13700,1,1},
  {"20M",14000,14350,14250,3,0},
  {"19M",15100,15830,15700,1,1},
  {"17M",17480,17900,17600,1,1},
  {"16M",18068,18168,18100,3,0},
  {"15M",18900,19020,18950,1,1},
  {"14M",21000,21450,21350,3,0},
  {"13M",21450,21850,21500,1,1},
  {"12M",24890,24990,24940,3,0},
  {"Banda corrida",25670,27999,26585,1,0},
  {"CB",26965,27405,27185,1,0},
  {"10M",28000,30000,28500,3,0}
};
constexpr uint8_t BAND_COUNT=sizeof(bands)/sizeof(bands[0]);
uint8_t bandPage=0;
uint8_t band=0, mode=0, volume=30, stepIndex=0;
uint16_t frequency=9950;
struct BandTuningState { uint16_t frequency; uint8_t mode,step; };
BandTuningState bandTuning[BAND_COUNT]{};
void rememberBandTuning(){bandTuning[band]={frequency,mode,stepIndex};}
void selectBand(uint8_t next){
  rememberBandTuning();
  band=next;
  frequency=bandTuning[band].frequency;
  mode=bandTuning[band].mode;
  stepIndex=bandTuning[band].step;
}
// Standard CB channel order (47 CFR 95.963), including skips and 23/24/25.
constexpr uint8_t CONTINUOUS_11M_BAND=27;
constexpr uint8_t CB_BAND=28;
const uint16_t cbChannels[40]={
  26965,26975,26985,27005,27015,27025,27035,27055,27065,27075,
  27085,27105,27115,27125,27135,27155,27165,27175,27185,27205,
  27215,27225,27255,27235,27245,27265,27275,27285,27295,27305,
  27315,27325,27335,27345,27355,27365,27375,27385,27395,27405
};
uint8_t cbChannelIndex(uint16_t freq){
  uint8_t nearest=0;int distance=65535;
  for(uint8_t i=0;i<40;i++){
    int d=abs((int)freq-(int)cbChannels[i]);
    if(d<distance){distance=d;nearest=i;}
  }
  return nearest;
}
Text cbLabel(){return Text("CB / Canal ")+(cbChannelIndex(frequency)+1);}
Text cbChannelDisplay(){return Text("CANAL ")+(cbChannelIndex(frequency)+1)+" / CB";}

int16_t bfo=0;
bool carrierSync=false;
bool syncPreferred=true;
bool muted=false, ready=false, dirty=true, savePending=false;
uint32_t changedAt=0, signalAt=0;
uint8_t rssi=0,snr=0;
bool signalValid=false;
uint8_t trace[48]={},traceHead=0,traceSize=0;
uint32_t signalReceivedAt=0;
constexpr uint32_t SIGNAL_STALE_MS=500;
bool fmMono=false,fmPilot=false;
uint8_t fmBlend=0;
void applyFmAudio(){
  if(!ready||mode!=0)return;
  // Documented SI4735 blend thresholds; no changes to audio wiring.
  receiver.setFmBlendStereoThreshold(fmMono?127:49);
  receiver.setFmBlendMonoThreshold(fmMono?127:30);
}

// User-selected attenuation is bounded to the documented mode limits.
bool agcAutomatic=true;
uint8_t agcAttenuation=0;
void applyAgc(){
  if(!ready)return;
  uint8_t index=min(agcAttenuation,uint8_t(mode==0?26:37));
  if(mode>=2)receiver.setSsbAgcOverrite(agcAutomatic?0:1,agcAutomatic?0:index);
  else receiver.setAutomaticGainControl(agcAutomatic?0:1,agcAutomatic?0:index);
}
void saveAgc(){
  applyAgc();prefs.putBool("agc-auto",agcAutomatic);
  prefs.putUChar("agc-index",agcAttenuation);dirty=true;
}

constexpr uint32_t SIGNAL_INTERVAL_MS=100;
uint8_t signalPeak[2]={0,0};
uint32_t peakAt[2]={0,0};
void recordRssi(uint8_t value,uint8_t noiseRatio,uint32_t now){
  const uint8_t readings[2]={value,noiseRatio};
  for(int i=0;i<2;i++){
    if(readings[i]>signalPeak[i] || now-peakAt[i]>=1500){
      if(signalPeak[i]!=readings[i]&&page==Page::Radio)dirty=true;
      signalPeak[i]=readings[i];peakAt[i]=now;
    }
  }

}
Text stationName,radioText;
constexpr uint32_t RDS_POLL_MS=40, RDS_SCROLL_MS=220, RDS_SCROLL_PAUSE_MS=1600;
constexpr uint8_t RDS_MAX_GROUPS_PER_POLL=3;
uint32_t rdsScrollAt=0;
uint8_t rdsScrollOffset=0;
bool rdsScrollAtEnd=false;
constexpr int16_t RDS_TEXT_WIDTH=244;
uint32_t rdsAt=0,rdsSeen=0,psAssemblyAt=0,rtAssemblyAt=0;
uint16_t rdsPi=0,pendingPi=0;
uint8_t piHits=0,psMask=0,rtVersion=255,rtFlag=255;
uint16_t rtMask=0;
char psWork[9]={0},rtWork[65]={0};
uint8_t psHits[4]={0},rtHits[16]={0};
Text cleanRds(const char* text,size_t limit){
  Text result;if(!text)return result;
  if(!result.reserve(limit))return result;
  for(size_t i=0;i<limit&&text[i];i++){
    uint8_t c=text[i];if(c==13||c==10)break;
    result+=(c>=32&&c<127)?char(c):' ';
  }
  result.trim();return result;
}
void clearRdsAssembly(){
  psMask=0;rtMask=0;rtVersion=255;rtFlag=255;
  psAssemblyAt=0;rtAssemblyAt=0;
  memset(psWork,0,sizeof(psWork));memset(rtWork,0,sizeof(rtWork));
  memset(psHits,0,sizeof(psHits));memset(rtHits,0,sizeof(rtHits));
}
void publishRds(Text& target,const Text& value){
  if(target!=value){
    target=value;
    if(&target==&radioText){rdsScrollOffset=0;rdsScrollAtEnd=false;rdsScrollAt=millis();}
    if(page==Page::Radio)dirty=true;
  }
}
void processRds(){
  const auto &r=receiver.rdsResponse().resp;
  // Never consume a failed command response or assemble across loss of sync.
  if(r.ERR||!r.CTS)return;
  if(!receiver.getRdsSync()){clearRdsAssembly();pendingPi=0;piHits=0;return;}
  if(!receiver.getRdsReceived())return;
  if(r.GRPLOST){clearRdsAssembly();pendingPi=0;piHits=0;}
  if(r.BLEA>1||r.BLEB>1)return;
  uint16_t pi=(uint16_t(r.BLOCKAH)<<8)|r.BLOCKAL;
  if(!pi)return;
  if(pi!=rdsPi){
    if(pi!=pendingPi){pendingPi=pi;piHits=1;return;}
    if(++piHits<3)return;
    rdsPi=pi;piHits=0;clearRdsAssembly();
    publishRds(stationName,"");publishRds(radioText,"");
  }else{pendingPi=0;piHits=0;}
  rdsSeen=millis();
  uint16_t blockB=(uint16_t(r.BLOCKBH)<<8)|r.BLOCKBL;
  uint8_t group=blockB>>12,version=(blockB>>11)&1;
  if(group==0){
    if(r.BLED>1)return;
    // Bound the age of partial names; otherwise old and new cycles can mix.
    if(!psAssemblyAt||millis()-psAssemblyAt>6000){
      psMask=0;memset(psWork,0,sizeof(psWork));memset(psHits,0,sizeof(psHits));
      psAssemblyAt=millis();
    }
    uint8_t seg=blockB&3;char pair[2]={char(r.BLOCKDH),char(r.BLOCKDL)};
    if(memcmp(psWork+seg*2,pair,2)!=0){
      memcpy(psWork+seg*2,pair,2);psHits[seg]=1;psMask&=~(1<<seg);
    }else if(psHits[seg]<2)psHits[seg]++;
    if(psHits[seg]>=2)psMask|=1<<seg;
    if(psMask==15){publishRds(stationName,cleanRds(psWork,8));psMask=0;psAssemblyAt=0;memset(psHits,0,sizeof(psHits));}
  }else if(group==2){
    if(r.BLED>1||(!version&&r.BLEC>1))return;
    uint8_t flag=(blockB>>4)&1,seg=blockB&15,width=version?2:4;
    bool newText=flag!=rtFlag||version!=rtVersion;
    if(newText||!rtAssemblyAt||millis()-rtAssemblyAt>12000){
      rtFlag=flag;rtVersion=version;rtMask=0;
      memset(rtWork,0,sizeof(rtWork));memset(rtHits,0,sizeof(rtHits));
      rtAssemblyAt=millis();
      // Keep the last complete message until its replacement is verified.
    }
    char bytes[4]={char(r.BLOCKCH),char(r.BLOCKCL),char(r.BLOCKDH),char(r.BLOCKDL)};
    const char* data=version?bytes+2:bytes;
    if(memcmp(rtWork+seg*width,data,width)!=0){
      memcpy(rtWork+seg*width,data,width);rtHits[seg]=1;rtMask&=~(uint16_t(1)<<seg);
    }else if(rtHits[seg]<2)rtHits[seg]++;
    if(rtHits[seg]>=2)rtMask|=uint16_t(1)<<seg;
    uint8_t length=version?32:64;
    for(uint8_t i=0;i<length;i++)if(rtWork[i]==13){length=i+1;break;}
    uint8_t segments=(length+width-1)/width;
    uint16_t required=segments==16?0xffff:((uint16_t(1)<<segments)-1);
    if((rtMask&required)==required){
      publishRds(radioText,cleanRds(rtWork,length));rtMask=0;rtAssemblyAt=0;memset(rtHits,0,sizeof(rtHits));
    }
  }
}
void resetRds(){
  signalValid=false;traceHead=traceSize=0;signalAt=millis()-SIGNAL_INTERVAL_MS;
  stationName="";radioText="";rdsScrollOffset=0;rdsScrollAtEnd=false;rdsScrollAt=millis();rdsSeen=0;rdsPi=0;pendingPi=0;piHits=0;clearRdsAssembly();rdsAt=millis();rssi=0;snr=0;fmPilot=false;fmBlend=0;
  signalPeak[0]=signalPeak[1]=0;peakAt[0]=peakAt[1]=millis();
  if(ready&&mode==0){receiver.RdsInit();receiver.getRdsStatus(0,1,0);}
}

struct Memory { uint16_t freq; uint8_t band,mode; };
Memory memories[4]{};
volatile int32_t turns=0;
volatile uint8_t oldAB=0;
volatile int8_t quarters=0;
portMUX_TYPE encoderLock=portMUX_INITIALIZER_UNLOCKED;
DRAM_ATTR static const int8_t encoderTransitions[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
void IRAM_ATTR encoderEdge() {
  uint8_t ab=(digitalRead(EA)<<1)|digitalRead(EB);
  portENTER_CRITICAL_ISR(&encoderLock);
  if ((oldAB^ab)==3) quarters=0;
  else quarters+=encoderTransitions[(oldAB<<2)|ab];
  oldAB=ab;
  if(quarters>=4){if(turns<32767)turns++;quarters=0;}
  if(quarters<=-4){if(turns>-32767)turns--;quarters=0;}
  portEXIT_CRITICAL_ISR(&encoderLock);
}
const char* modeName(){return mode==0?(fmMono?"FM MONO":fmPilot&&fmBlend>0?"FM ESTEREO":"FM MONO (AUTO)"):mode==1?"AM":mode==2?"LSB":"USB";}
uint16_t tuningStep(){const uint16_t fm[]={10,5,1};const uint16_t am[]={1,5,9};return band==0?fm[stepIndex]:am[stepIndex];}
void changed(){dirty=true;savePending=true;changedAt=millis();}
Text fitText(Text text,int width,int font,bool translate=true);
void label(const Text& s,int x,int y,int font=2,uint16_t color=INK){canvas.setTextDatum(TL_DATUM);canvas.setTextColor(color,BG);canvas.drawString(fitText(s,320-x-8,font),x,y,font);}
// Compact 11 x 11 glyphs, centered in a common 17 x 20 icon slot.
enum UiIcon : int { Antenna, Sliders, Bookmark, Information, Wifi, Search,
                    Dial, Signal, Display, Back };
void icon(int x,int y,int kind,uint16_t color=ACCENT){
  static constexpr uint16_t glyphs[][11]={
    {0x401,0x222,0x124,0x0A8,0x070,0x020,0x020,0x020,0x020,0x070,0x0F8},
    {0x110,0x110,0x3B8,0x110,0x110,0x154,0x154,0x17C,0x110,0x110,0x110},
    {0x1FC,0x104,0x104,0x17C,0x104,0x104,0x104,0x104,0x12C,0x144,0x084},
    {0x0F8,0x104,0x202,0x222,0x202,0x222,0x222,0x222,0x202,0x104,0x0F8},
    {0x0F8,0x306,0x401,0x0F8,0x104,0x020,0x050,0x000,0x020,0x070,0x020},
    {0x1C0,0x220,0x410,0x410,0x410,0x220,0x1D0,0x008,0x004,0x002,0x001},
    {0x0F8,0x104,0x222,0x242,0x282,0x302,0x202,0x202,0x202,0x104,0x0F8},
    {0x000,0x003,0x003,0x033,0x033,0x033,0x333,0x333,0x333,0x333,0x7FF},
    {0x3FE,0x401,0x401,0x451,0x4A9,0x501,0x401,0x3FE,0x020,0x020,0x0F8},
    {0x000,0x000,0x080,0x100,0x200,0x7FF,0x200,0x100,0x080,0x000,0x000}
  };
  if(kind<0||kind>=int(sizeof(glyphs)/sizeof(glyphs[0])))return;
  for(int row=0;row<11;++row)
    for(int col=0;col<11;++col)
      if(glyphs[kind][row]&(0x400>>col))canvas.drawPixel(x+3+col,y+4+row,color);
}
// Fit by rendered width, not character count (SSID and translated labels vary).
Text fitText(Text text,int width,int font,bool translate){
  if(translate)text=localized(text);
  if(canvas.textWidth(text,font)<=width)return text;
  if(canvas.textWidth("...",font)>width)return "";
  while(text.length()&&canvas.textWidth(text+"...",font)>width){
    size_t end=text.length()-1;
    while(end&&(uint8_t(text[end])&0xC0)==0x80)--end;
    text.remove(end);
  }
  return text+"...";
}
// Encoder focus uses the same actions as the touch screen.
struct FocusTarget { int x,y,w,h; };
FocusTarget focusTargets[48];
int focusCount=0,focusIndex=0;
Page focusPage=Page::Radio;
bool radioNavigation=false;
void addFocus(int x,int y,int w,int h){
  if(focusCount<48)focusTargets[focusCount++]={x,y,w,h};
}
void drawFocus(){
  if((page==Page::Radio&&!radioNavigation)||!focusCount)return;
  focusIndex=constrain(focusIndex,0,focusCount-1);
  const auto &f=focusTargets[focusIndex];
  canvas.drawRoundRect(f.x,f.y,f.w,f.h,4,INK);
  canvas.drawRoundRect(f.x+1,f.y+1,f.w-2,f.h-2,3,ACCENT);
}
void button(int x,int y,int w,const Text& text,bool active=false,int symbol=-1,bool translate=true){
  int h=(y>=208||page==Page::Memories)?28:34;
  if(page==Page::WifiScan&&y<213)h=28;
  if(page==Page::Activity&&y==64)h=32;
  if(page==Page::Network&&(y==61||y==137))h=32;
  h=min(h,240-y);
  addFocus(x,y,w,h);
  const uint16_t tint=hudAccent(x+w/2);
  const uint16_t paper=active?mixColor(PANEL,tint,62):PANEL;
  raisedSurface(x,y,w,h,paper,5);
  if(active)canvas.drawFastHLine(x+7,y+h-4,w-14,tint);
  // Compact toolbar uses stacked icons; wider actions retain inline labels.
  const bool stacked=page==Page::Radio&&y>=208&&symbol>=0;
  if(symbol>=0)icon(stacked?x+(w-17)/2:x+8,stacked?y-1:y+(h-20)/2,tint);
  const int space=w-(symbol>=0&&!stacked?42:12);
  const int font=!stacked&&canvas.textWidth(translate?localized(text):text,2)<=space?2:1;
  canvas.setTextDatum(MC_DATUM);canvas.setTextColor(INK);
  canvas.drawString(fitText(text,space,font,translate),x+w/2+(symbol>=0&&!stacked?10:0),stacked?y+21:y+h/2,font);
}
void menuCard(int x,int y,int w,const Text& title,const Text& value,int symbol,bool active=false){
  addFocus(x,y,w,34);
  raisedSurface(x,y,w,34,PANEL);
  if(active)canvas.drawRoundRect(x,y,w,34,4,ACCENT);
  const uint16_t tint=hudAccent(x+16);
  canvas.fillRoundRect(x+5,y+5,24,24,5,mixColor(PANEL,tint,38));
  icon(x+8,y+7,symbol,tint);
  if(page==Page::Settings){
    canvas.setTextDatum(TL_DATUM);canvas.setTextColor(INK);
    const int font=canvas.textWidth(localized(title),2)<=w-48?2:1;
    canvas.drawString(fitText(title,w-48,font),x+32,y+3,font);
    canvas.setTextColor(DIM);
    canvas.drawString(fitText(value,w-48,1),x+32,y+21,1);
  }else{
  canvas.setTextDatum(TL_DATUM);canvas.setTextColor(DIM);
  canvas.drawString(fitText(title,w-52,1),x+32,y+4,1);
  canvas.setTextColor(INK);canvas.drawString(fitText(value,w-52,2),x+32,y+15,2);
  }
  canvas.drawLine(x+w-13,y+13,x+w-9,y+17,ACCENT);
  canvas.drawLine(x+w-9,y+17,x+w-13,y+21,ACCENT);
}
constexpr uint32_t BOOT_FRAME_MS=33;
constexpr uint32_t BOOT_HOLD_MS=180;
bool bootAnimating=true,uiAvailable=false;
void welcome(){
  const uint16_t sky=0x0002, steel=0x6B90, silver=0xC638, glow=0x07FF;
  static uint32_t frameAt=0,startedAt=0;
  static int16_t frame=0;
  static bool started=false,allocated=false;
  static TFT_eSprite scene(&screen);
  if(started && uint32_t(millis()-frameAt)<(frame>=120?BOOT_HOLD_MS:BOOT_FRAME_MS))return;
  frameAt=millis();
  if(!started){started=true;startedAt=millis();

  screen.fillScreen(sky);
  screen.setTextDatum(MC_DATUM);
  screen.setTextColor(0x7BEF,sky);screen.drawString(localized("R E C E P T O R   M U L T I B A N D A"),160,16,1);
  screen.setTextColor(TFT_WHITE,sky);screen.drawString("Radio KALIMAN",160,40,4);
  screen.drawFastHLine(70,60,180,steel);
  scene.setColorDepth(16);allocated=scene.createSprite(280,128)!=nullptr;
  if(!allocated){
    scene.setColorDepth(8);allocated=scene.createSprite(280,128)!=nullptr;
  }
  if(!allocated){
    Serial.println("Inicio: sin memoria para animacion; conservando portada");
    screen.setTextColor(silver,sky);
    screen.drawString("FM  /  AM  /  SSB",160,160,2);
  }
  }
  if(allocated && frame<120){
      // Complete off-screen composition: no erase is visible on the display.
      for(int y=0;y<128;y++)scene.drawFastHLine(0,y,280,y<70?sky:y<103?0x0825:0x1046);
      for(int i=0;i<28;i++){
        int sx=(i*73+17)%280,sy=(i*31+7)%73;
        scene.drawPixel(sx,sy,(i+frame/10)%6==0?silver:steel);
        if(i%9==0){scene.drawFastHLine(sx-1,sy,3,steel);scene.drawFastVLine(sx,sy-1,3,steel);}
      }
      scene.fillCircle(257,22,12,0x8494);scene.fillCircle(253,18,3,0x6B90);
      scene.fillCircle(261,25,2,0x6B90);
      // Layered distant hills, fenced meadow and a grounded tower.
      for(int i=0;i<5;i++)scene.fillTriangle(i*70-25,110,i*70+30,83+i%2*8,i*70+80,110,0x18A7);
      scene.fillRect(0,113,280,15,0x10E6);
      scene.drawFastHLine(0,113,280,0x2A0A);
      for(int i=0;i<12;i++){int x=i*25;scene.drawLine(x,112,x+2,108,0x320B);}
      scene.fillEllipse(40,118,26,3,0x0001);
      scene.fillRect(20,114,42,4,steel);
      scene.drawLine(40,41,22,114,silver);scene.drawLine(41,41,60,114,steel);
      scene.drawLine(40,41,40,114,steel);
      for(int y=65;y<114;y+=12){
        int w=(y-41)/4;
        scene.drawLine(40-w,y,40+w,y,steel);
        scene.drawLine(40-w,y,40+(y+12-41)/4,y+12,silver);
        scene.drawLine(40+w,y,40-(y+12-41)/4,y+12,steel);
      }
      scene.drawLine(40,18,40,42,silver);scene.fillCircle(40,18,2,glow);
      // Broken wave arcs, rather than rings crossing the tower.
      for(int ring=0;ring<3;ring++){
        int radius=8+ring*7+(frame/4)%7;
        for(int deg=-55;deg<=55;deg+=4){
          float angle=deg*0.01745329252f;
          int dx=int(cosf(angle)*radius),dy=int(sinf(angle)*radius);
          scene.drawPixel(40+dx,18+dy,ring==2?steel:glow);
          scene.drawPixel(40-dx,18+dy,ring==2?steel:glow);
        }
      }
      int x=frame<42?82+frame*3:frame<86?208:208+(frame-86)*44/33;
      int y=27+int(sinf(frame*0.16f)*2);
      bool beam=frame>=48&&frame<86;
      if(beam){
        scene.fillTriangle(x,y+14,x-24,113,x+24,113,0x124A);
        scene.fillTriangle(x,y+14,x-12,113,x+12,113,0x1B30);
        scene.drawLine(x,y+14,x-24,113,0x3454);scene.drawLine(x,y+14,x+24,113,0x3454);
        scene.drawEllipse(x,113,24,3,0x3454);
        for(int i=0;i<8;i++){int py=45+(i*13+frame*2)%66;scene.drawPixel(x-9+i%4*6,py,glow);}
      }
      // Cow silhouette, spots, muzzle, ears, hooves and small tail.
      if(frame<82){
        int escape=frame>=42&&frame<66?int(sinf((frame-42)*PI/4)*10):0;
        int cy=frame<66?99:99-(frame-66)*4;
        if(frame<66)scene.fillEllipse((207+escape),115,18,2,0x0001);
        scene.fillRoundRect((192+escape),cy,25,12,4,0xEF7D);
        scene.fillEllipse((198+escape),cy+4,4,3,0x2104);scene.fillEllipse((209+escape),cy+8,4,3,0x2104);
        scene.fillRoundRect((214+escape),cy-5,10,12,3,0xEF7D);
        scene.fillRoundRect((215+escape),cy+2,10,5,2,0xCBB0);
        scene.fillTriangle((214+escape),cy-3,(210+escape),cy-7,(217+escape),cy-5,0xEF7D);
        scene.fillTriangle((222+escape),cy-3,(226+escape),cy-7,(221+escape),cy-5,0xEF7D);
        scene.drawLine((217+escape),cy-5,(216+escape),cy-8,silver);scene.drawLine((221+escape),cy-5,(222+escape),cy-8,silver);
        scene.drawPixel((220+escape),cy-2,TFT_BLACK);scene.drawPixel((223+escape),cy+4,0x4208);
        for(int i=0;i<3;i++){int lx=(195+escape)+i*8;scene.fillRect(lx,cy+10,2,5,0xEF7D);scene.fillRect(lx,cy+14,3,2,0x2104);}
        scene.drawLine((192+escape),cy+3,(187+escape),cy-1,silver);scene.drawLine((187+escape),cy-1,(185+escape),cy+4,silver);
      }
      // Saucer with canopy highlights, layered metal and running lights.
      scene.fillEllipse(x,y,13,10,0x0250);scene.drawEllipse(x,y,13,10,glow);
      scene.fillEllipse(x-4,y-4,4,2,0x9F3F);
      scene.drawLine(x,y-8,x,y+2,0x04B4);
      scene.fillEllipse(x,y+7,27,8,steel);scene.fillEllipse(x,y+5,27,5,silver);
      scene.drawEllipse(x,y+7,27,8,0x3188);scene.drawFastHLine(x-20,y+3,40,TFT_WHITE);
      for(int i=-18;i<=18;i+=9){scene.fillCircle(x+i,y+10,2,(frame/4+i/9)%3==0?TFT_WHITE:glow);}
      scene.fillEllipse(x,y+15,8,2,beam?glow:0x3188);
      scene.pushSprite(20,72);
      screen.fillRect(32,213,(frame+1)*256/120,3,glow);
      ++frame;return;
  }
  // Even if both sprite allocations fail, never skip the startup cover.
  if(!allocated && uint32_t(millis()-startedAt)<120*BOOT_FRAME_MS+BOOT_HOLD_MS)return;
  if(allocated){scene.deleteSprite();allocated=false;}
  screen.setTextColor(silver,sky);screen.drawString("FM  /  AM  /  SSB",160,229,1);
  bootAnimating=false;
  screen.fillScreen(BG);
  // The startup sprite is released before allocating the full radio UI.
  canvas.setColorDepth(8);
  uiAvailable=canvas.createSprite(320,240)!=nullptr;
  if(!uiAvailable){
    Serial.println("No hay memoria para la interfaz");
    screen.setTextColor(TFT_WHITE,TFT_BLACK);
    screen.drawString(localized("Memoria insuficiente"),10,60,2);
    return;
  }
  dirty=true;
}

// Track the actual receiver state, not just the newly selected UI mode.
// setup() establishes the reset pin before any SSB reset/patch operation.
bool receiverInitialized=false;
bool autoFilter=true, audioEnabled=true, narrowFilter=false;
constexpr uint8_t FILTER_NARROW_DB=8, FILTER_WIDE_DB=14, AM_AVC_MAX_DB=36;
constexpr uint8_t AM_WIDE=1, AM_NARROW=3, SSB_WIDE=2, SSB_NARROW=1;
void applyChannelFilter(){
  if(mode==1)receiver.setBandwidth(narrowFilter?AM_NARROW:AM_WIDE,0);
  else if(mode>=2)receiver.setSSBAudioBandwidth(narrowFilter?SSB_NARROW:SSB_WIDE);
}
void applyAudioOutput(){receiver.setAudioMute(muted||!audioEnabled);}

int8_t receiverMode=-1;
enum class RadioStage : uint8_t { Idle, PatchPower, PatchConfig };
RadioStage radioStage=RadioStage::Idle;
uint32_t radioStageAt=0;
constexpr uint32_t PATCH_POWER_MS=50, PATCH_CONFIG_MS=25;
void setCarrierSync(bool enabled){
  if(!ready||radioStage!=RadioStage::Idle||mode<2||receiverMode!=mode)return;
  carrierSync=enabled;
  bfo=0;
  receiver.setSSBBfo(bfo);
  // SI4735 SSB_MODE: divider 3 and AFC enabled for carrier sync.
  receiver.setSSBConfig(narrowFilter?SSB_NARROW:SSB_WIDE,1,enabled?3:0,1,0,enabled?0:1);
  dirty=true;
}
void configureRadio(){
  if(band==CB_BAND)frequency=cbChannels[cbChannelIndex(frequency)];
  if(!ready)return;
  Serial.printf("Banda: %s / modo %u / frecuencia %u\n",bands[band].name,mode,frequency);
  if(radioStage==RadioStage::Idle){
    if(carrierSync&&receiverMode>=2&&mode>=2)receiver.setSSBConfig(2,1,0,1,0,1);
    carrierSync=false;
    bfo=0;
  }
  if(!receiverInitialized){
    receiver.setup(RESET,mode==0?0:1);
    receiverInitialized=true;
  }
  if(mode>=2){
    // The patch remains in RAM while changing SSB bands or USB/LSB.
    if(receiverMode<2){
      if(radioStage==RadioStage::Idle){
        receiver.reset();receiver.queryLibraryId();receiver.patchPowerUp();
        radioStage=RadioStage::PatchPower;radioStageAt=millis();return;
      }
      if(radioStage==RadioStage::PatchPower){
      if(uint32_t(millis()-radioStageAt)<PATCH_POWER_MS)return;
      receiver.setI2CFastMode();
      bool patchOk=receiver.downloadPatch(ssb_patch_content,sizeof(ssb_patch_content));
      receiver.setI2CStandardMode();
      if(!patchOk){
        Serial.println("Error cargando SSB; receptor deshabilitado hasta reiniciar");
        ready=false;receiverMode=-1;radioStage=RadioStage::Idle;dirty=true;return;
      }
      receiver.setSSBConfig(2,1,0,1,0,1);
      radioStage=RadioStage::PatchConfig;radioStageAt=millis();return;
      }
      if(uint32_t(millis()-radioStageAt)<PATCH_CONFIG_MS)return;
    }
    receiver.setSSB(bands[band].lo,bands[band].hi,frequency,tuningStep(),mode==2?1:2);
    receiver.setSSBBfo(bfo);
  }else{
    // Leaving patched SSB requires a normal boot. Same-mode band changes do not.
    if(receiverMode>=2)receiver.setup(RESET,mode==0?0:1);
    if(mode==0){receiver.setFM(bands[band].lo,bands[band].hi,frequency,tuningStep());receiver.setFMDeEmphasis(1);applyFmAudio();receiver.setRdsConfig(1,1,1,1,1);receiver.setFifoCount(1);}
    else receiver.setAM(bands[band].lo,bands[band].hi,frequency,tuningStep());
  }
  radioStage=RadioStage::Idle;
  receiverMode=mode;
  narrowFilter=false;
  applyChannelFilter();
  if(mode==1)receiver.setAvcAmMaxGain(AM_AVC_MAX_DB);
  if(mode>=2&&syncPreferred){setCarrierSync(true);if(control==Control::Fine)control=Control::Tune;}
  applyAgc();receiver.setVolume(volume);applyAudioOutput();resetRds();changed();
  Serial.println("Cambio de banda terminado");
}

void meter(int x,int y,const Text& title,uint8_t value,int maximum,const char* unit,uint16_t color){
  canvas.setTextDatum(TL_DATUM);canvas.setTextColor(DIM);
  canvas.drawString(title+" "+value+" "+unit,x,y,1);
  for(int i=0;i<14;i++)canvas.fillRoundRect(x+i*10,y+13,7,7,2,value*14/maximum>i?(theme==0?hudAccent(x+i*10):color):BG);
}
// Original compositions. All drawing stays within the radio HUD rectangle.
void hudText(const Text& text,int x,int y,int font,uint16_t ink,uint16_t paper,int datum=TL_DATUM){
  canvas.setTextDatum(datum);
  if(ink==ACCENT)ink=hudAccent(x);
  canvas.setTextColor(ink);canvas.drawString(localized(text),x,y,font);
}
// Outer orbit: RSSI. Inner orbit: SNR. Bright dots mark held peaks.
void drawOrbit(int rx,int ry,uint8_t reading,uint8_t peak,int scale,uint16_t color,bool front,int tilt){
  constexpr int segments=32,cx=160,cy=101;
  const int lit=(min(int(reading),scale)*segments+scale-1)/scale;
  const int held=(min(int(peak),scale)*segments+scale-1)/scale;
  for(int i=0;i<segments;i++){
    if((i>=segments/2)!=front)continue;
    const float a=(180.0f+360.0f*(i+0.5f)/segments)*DEG_TO_RAD;
    const int x=cx+lroundf(rx*cosf(a)),y=cy+lroundf(ry*sinf(a)+tilt*cosf(a));
    const bool marker=signalValid&&i+1==held;
    const uint16_t ink=marker?INK:signalValid&&i<lit?color:mixColor(PANEL,color,65);
    canvas.fillCircle(x,y,marker?2:1,ink);
  }
}
static void drawAlternateHud(){
  if(hudStyle==3){
    hudPanel(16,68,207,61,5);
    for(int y=75;y<=120;y+=15)canvas.drawFastHLine(23,y,193,mixColor(PANEL,DIM,55));
    int px=0,py=0;
    for(int i=0;signalValid&&i<traceSize;i++){
      const int x=215-(traceSize-1-i)*4;
      const int y=121-min(80,int(trace[(traceHead+48-traceSize+i)%48]))*45/80;
      if(i)canvas.drawLine(px,py,x,y,hudAccent(x));else canvas.drawPixel(x,y,hudAccent(x));
      px=x;py=y;
    }
  }
  for(int i=0;i<2;i++){
    const bool vertical=hudStyle==2;
    const int x=vertical?16+146*i:hudStyle==3?227:16;
    const int y=vertical?68:68+31*i;
    const int scale=i?50:80;
    const int value=i?snr:rssi;
    hudPanel(x,y,vertical?142:hudStyle==3?77:288,vertical?61:28,5);
    hudText(i?"SNR":"RSSI",x+7,y+6,1,i?SNR_COLOR:ACCENT,PANEL);
    char reading[20];
    if(signalValid)snprintf(reading,sizeof(reading),"%d %s",value,i?"dB":"dBuV");
    else snprintf(reading,sizeof(reading),"-- %s",i?"dB":"dBuV");
    hudText(reading,x+(vertical?7:hudStyle==3?7:280),y+(vertical?31:hudStyle==3?16:9),vertical?2:1,INK,PANEL,hudStyle==1?TR_DATUM:TL_DATUM);
    if(hudStyle==3)continue;
    for(int j=0;j<12;j++){
      const int sx=x+(vertical?102:51+j*13),sy=y+(vertical?51-j*4:10);
      const uint16_t ink=i?SNR_COLOR:hudAccent(sx);
      const int peak=min(12,(signalPeak[i]*12+scale-1)/scale);
      canvas.fillRoundRect(sx,sy,vertical?31:10,vertical?3:8,1,
        signalValid&&j+1==peak?INK:signalValid&&j*scale<value*12?ink:mixColor(PANEL,ink,45));
    }
  }
}

void drawRadioHud(const Text& f){
  const Text unit=band==0?"MHz":"kHz";
  // Common lower information strip: real RDS or current tuning parameters.
  const Text detail=mode==0?(radioText.length()?radioText:"Esperando texto RDS"):
    (band==CB_BAND?Text("40 canales / BFO "):Text("Paso ")+tuningStep()+" kHz   BFO ")+bfo+" Hz";
  const Text station=band==CB_BAND?cbChannelDisplay():
    mode==0?(stationName.length()?stationName:"Esperando emisora RDS"):"RECEPCION / ONDA CORTA";
    hudPanel(8,36,304,98,6);
    hudText(f,148,40,4,INK,PANEL,TR_DATUM);
    hudText(unit,153,48,1,DIM,PANEL);
    statePill(199,43,103,16,modeName(),ready);
  canvas.drawFastHLine(16,64,288,mixColor(PANEL,DIM,60));
  if(hudStyle)drawAlternateHud();
  else {
  const uint16_t rssiInk=hudAccent(30),snrInk=SNR_COLOR;
  const auto rings=[&](bool front){
    drawOrbit(66,23,rssi,signalPeak[0],80,rssiInk,front,-10);
    drawOrbit(46,14,snr,signalPeak[1],50,snrInk,front,9);
  };
  rings(false);
  // Light from the upper left gives the planet depth without a new palette.
  canvas.fillCircle(160,101,19,mixColor(PANEL,rssiInk,115));
  canvas.fillCircle(157,98,15,mixColor(PANEL,rssiInk,150));
  canvas.fillCircle(154,95,11,mixColor(PANEL,rssiInk,190));
  canvas.fillCircle(152,93,6,mixColor(rssiInk,INK,55));
  rings(true);
  hudText("RSSI",18,74,1,rssiInk,PANEL);
  hudText(signalValid?Text(rssi):Text("--"),18,88,4,INK,PANEL);
  hudText("dBuV",18,116,1,DIM,PANEL);
  hudText("SNR",302,74,1,snrInk,PANEL,TR_DATUM);
  hudText(signalValid?Text(snr):Text("--"),302,88,4,INK,PANEL,TR_DATUM);
  hudText("dB",302,116,1,DIM,PANEL,TR_DATUM);
  }
    themeLine(16,133,288);
    // Broadcast content is data, never run it through the UI dictionary.
    canvas.setTextDatum(TL_DATUM);canvas.setTextColor(INK);
    canvas.drawString(fitText(station,296,2,!(mode==0&&stationName.length())),12,139,2);
    canvas.setTextColor(DIM);
    const bool broadcast=mode==0&&radioText.length();
    canvas.drawString(fitText(broadcast?Text(radioText.c_str()+rdsScrollOffset):detail,RDS_TEXT_WIDTH,1,!broadcast),12,157,1);

}
// Visual-only badge: follows the same automatic/manual state applied to the receiver.
void drawAgcBadge(){
  statePill(264,151,38,12,"AGC",ready&&agcAutomatic);
}
bool seeking=false;
uint32_t seekStarted=0,seekPoll=0;
uint16_t seekOrigin=0;
bool seekFound=false;
Text seekMessage="Elige una direccion";
static RadioNetwork::Credentials wifiInput{};
static bool wifiError=false;
static int scanPage=0;
static constexpr int ScanRows=4;
static uint8_t keySet=0;
static constexpr char wifiKeys[]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
static constexpr int keysPerPage=24, keyPages=(sizeof(wifiKeys)-2)/keysPerPage+1;

static void propagationDots(){
  const unsigned f=frequency;
  int group=-1;
  if(band!=0){
    if((f>=3500&&f<=4000)||(f>=7000&&f<=7300))group=0;
    else if((f>=10100&&f<=10150)||(f>=14000&&f<=14350))group=1;
    else if((f>=18068&&f<=18168)||(f>=21000&&f<=21450))group=2;
    else if((f>=24890&&f<=24990)||(f>=28000&&f<=29700))group=3;
  }
  const uint16_t colors[]={TFT_DARKGREY,TFT_RED,TFT_YELLOW,TFT_GREEN};
  for(int i=0;i<2;i++){
    const auto value=group<0?0:RadioNetwork::status.forecast[group*2+i];
    const int y=43+13*i;
    hudText(i==0?"D":"N",179,y-4,1,INK,PANEL);
    canvas.drawCircle(191,y,4,TFT_LIGHTGREY);
    canvas.fillCircle(191,y,3,colors[value<=3?value:0]);
  }
}

void draw(){
  if(focusPage!=page){focusPage=page;focusIndex=0;}
  focusCount=0;
  if(!canvas.getPointer())return; // Never draw through a failed allocation.
  canvas.fillSprite(BG);
  raisedSurface(0,0,320,30,PANEL,0);
  if(page!=Page::Radio){
    icon(7,4,page==Page::Settings||page==Page::Appearance||page==Page::Gain?1:page==Page::Memories?2:page==Page::Network||page==Page::WifiScan||page==Page::Keyboard?4:page==Page::Seek?5:page==Page::About?3:0);
    label(page==Page::Activity?"ACTIVIDAD HF":page==Page::Settings?"AJUSTES":page==Page::Tuning?"SINTONIA":page==Page::Memories?"FAVORITOS":page==Page::Bands?"BANDAS":page==Page::Appearance?"APARIENCIA":page==Page::Reception?"RECEPCION":page==Page::Gain?"RECEPCION":page==Page::Audio?"FILTRO Y AUDIO":page==Page::Sync?"SYNC":page==Page::Seek?"BUSCAR":page==Page::Network||page==Page::WifiScan||page==Page::Keyboard?"WI-FI":"INFORMACION",31,7,2,ACCENT);
  }
  themeLine(0,29,320);
  if(page==Page::Radio){
    hudText(FIRMWARE_NAME,10,5,1,INK,PANEL);
    hudText(bands[band].name,310,5,1,ACCENT,PANEL,TR_DATUM);
    hudText(ready?"FM / AM / SSB":"Receptor no detectado",160,17,1,DIM,PANEL,TC_DATUM);
  }
  if(page==Page::Radio){
    Text f=band==0?Text(frequency/100.0f,2):Text(frequency);
    drawRadioHud(f);
    propagationDots();
    drawAgcBadge();
    button(8,170,97,"Sintonia",control==Control::Tune);
    button(111,170,97,Text("Vol ")+volume,control==Control::Volume);
    button(214,170,98,mode>=2?"Fino":"Silencio",mode>=2?control==Control::Fine:muted);
    button(8,208,73,"Ajustes",false,1);button(85,208,73,"Buscar",false,5);button(162,208,73,"Memoria",false,2);button(239,208,73,"Info",false,3);
  }else if(page==Page::Seek){
    label("BUSCAR EMISORAS",16,40,2,ACCENT);
    label((band==0?Text(frequency/100.0f,2)+" MHz":Text(frequency)+" kHz"),16,72,4);
    bool available=ready&&mode<2&&band!=CB_BAND;
    label(!ready?"Receptor no disponible":!available?"No disponible en SSB / CB":seekMessage,16,112,2,DIM);
    if(available){
      if(seeking)button(12,150,296,"Detener",true);
      else {button(12,150,144,"Buscar -");button(164,150,144,"Buscar +");}
    }
    button(12,208,296,"Volver");
  }else if(page==Page::Bands){
    icon(12,40,0);label("ELIGE UNA BANDA",38,40,2);
    for(int j=0;j<9;j++){
      int i=bandPage*9+j;if(i>=BAND_COUNT)break;
      int bx=10+(j%3)*103,by=69+(j/3)*42;
      addFocus(bx,by,94,34);
      raisedSurface(bx,by,94,34,i==band?ACCENT:PANEL);
      hudText(bands[i].name,bx+47,by+3,i==CONTINUOUS_11M_BAND?1:2,i==band?BG:INK,i==band?ACCENT:PANEL,TC_DATUM);
      Text preset=(i==0?Text(bands[i].initial/100.0f,2):Text(bands[i].initial))+" "+(bands[i].preferredMode==0?"FM":bands[i].preferredMode==1?"AM":bands[i].preferredMode==2?"LSB":"USB");
      hudText(preset,bx+47,by+23,1,i==band?BG:DIM,i==band?ACCENT:PANEL,TC_DATUM);
    }
    label(Text("Pagina ")+(bandPage+1)+" / "+((BAND_COUNT+8)/9),104,194,2,DIM);
    button(10,212,94,"Anterior");button(113,212,94,"Volver");button(216,212,94,"Siguiente");
  }else if(page==Page::Settings){
    menuCard(12,48,144,"BANDAS",bands[band].name,0);
    menuCard(164,48,144,"SINTONIA",modeName(),Dial);
    menuCard(12,100,144,"RECEPCION","AGC / SYNC",1);
    menuCard(164,100,144,"ESTADO","RSSI / SNR",Signal);
    menuCard(12,152,144,"PANTALLA","Color e idioma",Display);
    menuCard(164,152,144,"Wi-Fi","Red y actividad HF",4);
    button(12,211,296,"Volver a la radio",false,Back);
  }else if(page==Page::Tuning){
    button(12,46,296,band?Text("Modo: ")+modeName():Text(fmMono?"Audio FM: Mono":"Audio FM: Estereo"));
    if(band!=CB_BAND)button(12,96,296,Text("Paso: ")+(band==0?tuningStep()*10:tuningStep())+" kHz");
    else label("CB: 40 canales / paso fijo",12,104,2,DIM);
    button(12,146,296,muted?"Activar sonido":"Silenciar",muted);
    button(12,211,296,"Volver");
  }else if(page==Page::Activity){
    const auto& activity=HfActivity::view;
    const char* states[]={"Sin Wi-Fi","Conectando a PSK Reporter...","Escuchando reportes","Servicio no disponible"};
    label(states[activity.state],12,40,2,ACCENT);
    button(12,64,92,"Anterior");button(216,64,92,"Siguiente");
    hudText(HfActivity::Bands[activity.index],160,71,2,INK,BG,TC_DATUM);
    label("TX > RX",12,99,1,DIM);
    hudText("MODO REPORTADO",300,99,1,DIM,BG,TR_DATUM);
    if(!activity.count)label("Sin reportes recibidos",12,116,2,DIM);
    for(uint8_t i=0;i<activity.count;++i){
      const int y=110+i*18;
      const auto& spot=activity.spots[i];
      hudPanel(12,y,296,16,4);
      hudText(spot.tx,19,y+4,1,INK,PANEL);
      hudText(">",120,y+4,1,DIM,PANEL);
      hudText(spot.rx,139,y+4,1,INK,PANEL);
      hudText(spot.mode,300,y+4,1,ACCENT,PANEL,TR_DATUM);
    }
    label("PSK Reporter / ultimos 5 min en este equipo",12,185,1,DIM);
    label("Reportes externos; no miden tu antena",12,198,1,DIM);
    button(12,213,296,"Volver");
  }else if(page==Page::Network){
    const char* states[]={"Desconectado","Conectando...","Conectado","Sin conexion: revisa red y clave"};
    label(wifiError?"No se pudo iniciar Wi-Fi":RadioNetwork::status.storageError?"Error al guardar o borrar red":states[RadioNetwork::status.connection],12,38,2,ACCENT);
    button(12,61,296,"Buscar redes Wi-Fi (2.4 GHz)");
    label(wifiInput.ssid,12,105,2,INK);
    button(12,137,296,"Olvidar red");
    char hfStatus[56];
    const auto& net=RadioNetwork::status;
    if(net.connection!=2)snprintf(hfStatus,sizeof(hfStatus),"HF: esperando Wi-Fi");
    else if(net.forecast[0])snprintf(hfStatus,sizeof(hfStatus),"HF: datos recibidos / D: dia, N: noche");
    else if(net.forecastCode==0||net.forecastCode==200)snprintf(hfStatus,sizeof(hfStatus),"HF: esperando datos vigentes");
    else if(net.forecastCode==-101)snprintf(hfStatus,sizeof(hfStatus),"HF: formato HamQSL no reconocido");
    else snprintf(hfStatus,sizeof(hfStatus),"HF: fallo de descarga (%d)",int(net.forecastCode));
    label(hfStatus,12,176,1,INK);
    label("Verde: buena / amarillo: regular / rojo: mala",12,188,1,INK);
    label("Gris: sin datos / Fuente: HamQSL - N0NBH",12,200,1,DIM);
    button(12,213,144,"Volver");button(164,213,144,"Actividad HF");
  }else if(page==Page::WifiScan){
    const auto& list=RadioNetwork::list;
    label(wifiError?"No se pudo iniciar":RadioNetwork::scanning?"Buscando redes...":list.count<0?"Fallo de busqueda":list.count==0?"No se encontraron redes":"Toca una red para conectar",12,36,2,ACCENT);
    for(int row=0;row<ScanRows&&scanPage*ScanRows+row<list.count;row++){
      const auto& ap=list.ap[scanPage*ScanRows+row];
      button(12,55+32*row,296,Text(ap.open?"[Abierta] ":"[Clave] ")+ap.name,false,-1,false);
    }
    button(12,184,96,"Anterior");button(112,184,96,"Siguiente");button(212,184,96,"Repetir");
    button(12,213,296,"Volver");
  }else if(page==Page::Keyboard){
    char masked[65];
    size_t n=strlen(wifiInput.key);memset(masked,'*',n);masked[n]=0;
    hudText(Text("Clave: ")+(masked+(n>28?n-28:0)),12,36,2,INK,BG);
    for(int k=0;k<keysPerPage;k++){
      int index=keySet*keysPerPage+k;
      if(index>=int(sizeof(wifiKeys)-1))break;
      char text[5]={wifiKeys[index],0};
      if(text[0]==' ')strcpy(text,"Esp.");
      button(4+(k%8)*39,60+(k/8)*37,37,text);
    }
    button(4,172,102,Text("Teclas ")+(keySet+1)+"/"+keyPages);
    button(109,172,102,"Borrar");button(214,172,102,"Limpiar");
    button(12,211,144,"Cancelar");button(164,211,144,"Conectar");
  }else if(page==Page::Gain){
    label("CONTROL DE RECEPCION",12,40,2,ACCENT);
    button(12,66,296,agcAutomatic?"AGC: automatico":"AGC: manual",agcAutomatic);
    button(164,101,144,"Filtro y audio");
    label(Text("Atenuacion: ")+min(agcAttenuation,uint8_t(mode==0?26:37))+" / "+(mode==0?"26":"37"),12,112,1,INK);
    button(12,145,144,"Menos");button(164,145,144,"Mas");
    button(12,179,296,carrierSync?"SYNC: aplicado":syncPreferred?"SYNC: automatico":"SYNC: desactivado",carrierSync);
    button(12,218,296,"Volver a ajustes");
  }else if(page==Page::Audio){
    button(12,48,296,autoFilter?"Filtro AM/SSB: Auto":"Filtro AM/SSB: Fijo",autoFilter);
    label(mode==0?"No aplicable en FM":mode==1?(narrowFilter?"AM: 2 kHz":"AM: 4 kHz"):
          (narrowFilter?"SSB: 2.2 kHz":"SSB: 3 kHz"),12,86,1,DIM);
    button(12,108,296,audioEnabled?"Audio: habilitado":"Audio: deshabilitado",audioEnabled);
    label("Salida de audio, no apagado del AMP.",12,148,1,DIM);
    label(mode==1?"AVC AM: automatico (max. 36 dB)":"AVC AM: solo en modo AM",12,174,1,INK);
    button(12,218,296,"Volver");
  }else if(page==Page::Sync){
    label("SINCRONIA DE PORTADORA",12,42,2,ACCENT);
    label("Para emisiones AM con portadora.",12,76,1,INK);
    label("No corrige voces en USB o LSB.",12,92,1,DIM);
    label(mode<2?"Se aplica solo al entrar en USB o LSB.":
          carrierSync?"Activo: bloqueo no confirmado":
          !ready||radioStage!=RadioStage::Idle?"Receptor no disponible para SYNC.":"Fino manual: SYNC no aplicado",12,117,1,INK);
    button(12,144,296,syncPreferred?"Desactivar SYNC":"Activar SYNC",syncPreferred);
    label("Fino pausa SYNC hasta reconfigurar la banda.",12,188,1,DIM);
    button(12,218,296,"Volver",false);
  }else if(page==Page::Reception){
    label("ESTADO DEL RECEPTOR",12,40,2,ACCENT);
    label(ready?"Receptor detectado":"Receptor no detectado",12,66,2,INK);
    char reading[64];
    if(signalValid)snprintf(reading,sizeof(reading),"RSSI: %u dBuV    SNR: %u dB",unsigned(rssi),unsigned(snr));
    else snprintf(reading,sizeof(reading),"RSSI: --    SNR: --");
    label(reading,12,96,2,INK);
    label(Text("Banda / ")+bands[band].name+" / "+modeName(),12,123,2,DIM);
    label(agcAutomatic?"AGC: automatico":"AGC: manual",12,149,2,INK);

    button(12,218,296,"Volver a ajustes");
  }else if(page==Page::Appearance){
    label("ESTILO / superficies y acentos coordinados",12,42,1,DIM);
    menuCard(12,67,296,"HUD",hudNames[hudStyle],1);
    menuCard(12,110,296,"PALETA DE COLOR",themeNames[theme],1);
    raisedSurface(12,153,296,25,PANEL);
    hudText("VISTA PREVIA",24,160,1,DIM,PANEL);
    
    for(int i=0;i<7;i++)canvas.fillRoundRect(138+i*22,158,16,14,3,(theme==0||theme==14)?hudAccent(138+i*22):i%2?ACCENT:DIM);
    button(12,179,296,Text("Idioma / Language: ")+languageName());
    button(12,218,296,"Volver a ajustes",false,1);
  }else if(page==Page::Memories){
    label("TU COLECCION / 4 emisoras",12,41,2,INK);
    label("Toca: escuchar   Mantener: guardar aqui",12,65,1,DIM);
    for(int i=0;i<4;i++){
      Text text=Text("0")+(i+1)+"   ";
      if(memories[i].freq)text+=(memories[i].band==0?Text(memories[i].freq/100.0f,2)+" MHz":Text(memories[i].freq)+" kHz");else text+="Guardar emisora";
      button(12,84+i*31,296,text,false,0);
    }
    button(12,214,296,"Volver a la radio",false,Back);
  }else{
    hudPanel(12,39,296,79,6);
    hudText("FIRMWARE DEL RECEPTOR",24,47,1,DIM,PANEL);
    hudText(FIRMWARE_NAME,24,64,4,INK,PANEL);
    hudText(Text("VERSION ")+FIRMWARE_VERSION,24,99,1,ACCENT,PANEL);
    hudText(FIRMWARE_CREATOR,296,99,1,INK,PANEL,TR_DATUM);
    label("ATS25 max-Decoder original",16,127,2,INK);
    label("30 bandas  /  FM - AM - USB - LSB",16,148,1,DIM);
    label("PubSubClient: Nick O'Leary / MIT",16,161,1,DIM);
    themeLine(16,178,288);
    label("Bibliotecas: PU2CLR / TFT_eSPI / XPT2046",16,185,1,DIM);
    label("Parche SSB: Vadim Afonkin / avisos en fuente",16,199,1,DIM);
    button(12,214,296,"Volver a la radio",false,Back);
  }
  drawFocus();
  // Keep only row fingerprints: avoids a second full framebuffer without visible erases.
  const uint8_t* pixels=static_cast<const uint8_t*>(canvas.getPointer());
  int first=-1;
  for(int row=0;row<240;row++){
    uint64_t hash=rowFingerprint(pixels+row*320);
    bool changedRow=!frameValid||hash!=previousRows[row];
    previousRows[row]=hash;
    if(changedRow&&first<0)first=row;
    if(!changedRow&&first>=0){canvas.pushSprite(0,first,0,first,320,row-first);first=-1;}
  }
  if(first>=0)canvas.pushSprite(0,first,0,first,320,240-first);
  frameValid=true;dirty=false;
}
void tune(int delta){
  if(!ready||seeking)return;
  if(control==Control::Volume){volume=constrain((int)volume+delta,0,63);receiver.setVolume(volume);}
  else if(control==Control::Fine && mode>=2){bfo=constrain((int)bfo+int64_t(delta)*10,int64_t(-16000),int64_t(16000));receiver.setSSBBfo(bfo);}
  else if(band==CB_BAND){
    int channel=constrain((int)cbChannelIndex(frequency)+delta,0,39);
    if(frequency==cbChannels[channel])return;
    frequency=cbChannels[channel];receiver.setFrequency(frequency);applyAgc();resetRds();
  }
  else {
    const uint16_t next=constrain(int64_t(frequency)+int64_t(delta)*tuningStep(),int64_t(bands[band].lo),int64_t(bands[band].hi));
    if(next==frequency)return;
    frequency=next;receiver.setFrequency(frequency);applyAgc();resetRds();
  }
  changed();
}
void finishSeek(bool cancel){
  receiver.getStatus(1,cancel?1:0);
  uint16_t found=receiver.getFrequency();
  if(!cancel&&seekFound&&found>=bands[band].lo&&found<=bands[band].hi)frequency=found;
  else {frequency=seekOrigin;receiver.setFrequency(frequency);}
  seeking=false;applyAgc();applyAudioOutput();resetRds();changed();
}
void startSeek(bool up){
  if(!ready||seeking||mode>=2||band==CB_BAND)return;
  seekOrigin=frequency;seekFound=false;
  if(mode==0){
    receiver.setSeekFmLimits(bands[band].lo,bands[band].hi);
    receiver.setSeekFmSpacing(10); // 100 kHz, independent of manual tuning step
    receiver.setSeekFmRssiThreshold(12); // dBuV: include weaker stations
    receiver.setSeekFmSNRThreshold(3); // dB: reject noise-only channels
  }
  else {receiver.setSeekAmLimits(bands[band].lo,bands[band].hi);receiver.setSeekAmSpacing(min(uint16_t(10),tuningStep()));}
  receiver.getStatus(1,0);
  signalValid=false;traceHead=traceSize=0;rssi=snr=0;signalPeak[0]=signalPeak[1]=0;
  seeking=true;seekStarted=seekPoll=millis();seekMessage=up?"Buscando hacia arriba...":"Buscando hacia abajo...";
  receiver.seekStation(up?1:0,0);dirty=true;
}
void serviceSeek(){
  if(!seeking||millis()-seekPoll<100)return;
  seekPoll=millis();receiver.getStatus(0,0);
  if(receiver.getTuneCompleteTriggered()){
    bool valid=receiver.getStatusValid()&&!receiver.getBandLimit();
    seekFound=valid;
    seekMessage=valid?"Emisora encontrada":"Sin emisora: frecuencia inicial";
    finishSeek(false);
  }else if(millis()-seekStarted>=15000){seekMessage="Tiempo agotado";finishSeek(true);}
}
void tap(uint16_t x,uint16_t y,bool held){
  // Touch and encoder share the exact visible hit boxes, including menu gaps.
  if(dirty||focusPage!=page)draw();
  int target=-1;
  for(int i=0;i<focusCount;i++){
    const auto &f=focusTargets[i];
    if(x>=f.x&&x<f.x+f.w&&y>=f.y&&y<f.y+f.h){target=i;break;}
  }
  if(target<0)return;
  focusIndex=target;
  if(page==Page::Radio){
    if(y>=208){page=x<83?Page::Settings:x<160?Page::Seek:x<237?Page::Memories:Page::About;}
    else if(y>=170){if(x<108){control=Control::Tune;radioNavigation=false;}else if(x<211){control=Control::Volume;radioNavigation=false;}else if(mode>=2){if(carrierSync)setCarrierSync(false);control=Control::Fine;radioNavigation=false;}else {muted=!muted;if(ready)applyAudioOutput();changed();}}
  }else if(page==Page::Seek){
    if(y>=208){if(seeking){seekMessage="Busqueda detenida";finishSeek(true);}page=Page::Radio;}
    else if(y>=150){if(seeking){seekMessage="Busqueda detenida";finishSeek(true);}else startSeek(x>=160);}
  }else if(page==Page::Settings){
    switch(target){
      case 0: bandPage=band/9;page=Page::Bands;break;
      case 1: page=Page::Tuning;break;
      case 2: page=Page::Gain;break;
      case 3: page=Page::Reception;break;
      case 4: page=Page::Appearance;break;
      case 5: page=Page::Network;break;
      case 6: page=Page::Radio;break;
    }
  }else if(page==Page::Tuning){
    if(y>=211)page=Page::Settings;
    else if(y>=146){muted=!muted;if(ready)applyAudioOutput();changed();}
    else if(y>=96){if(band!=CB_BAND){stepIndex=(stepIndex+1)%3;changed();}}
    else if(band){mode=mode==3?1:mode+1;control=Control::Tune;configureRadio();changed();}
    else {fmMono=!fmMono;applyFmAudio();prefs.putBool("fm-mono",fmMono);}
  }else if(page==Page::Activity){
    if(y>=213){HfActivity::select(-1);page=Page::Network;}
    else if(y>=64&&y<96){
      const int count=HfActivity::BandCount;
      HfActivity::select((HfActivity::view.index+count+(x<160?-1:1))%count);
    }
  }else if(page==Page::Network){
    if(y>=213){if(x<160)page=Page::Settings;else{HfActivity::select(HfActivity::DefaultBand);page=Page::Activity;}}
    else if(y>=137&&y<169&&!RadioNetwork::scanning){
      memset(&wifiInput,0,sizeof(wifiInput));wifiError=!RadioNetwork::send(wifiInput);
    }else if(y>=61&&y<93){scanPage=0;wifiError=!RadioNetwork::search();page=Page::WifiScan;}
  }else if(page==Page::WifiScan){
    const auto& list=RadioNetwork::list;
    if(y>=213)page=Page::Network;
    else if(!RadioNetwork::scanning){
      if(y>=184){
        if(x>=212){scanPage=0;wifiError=!RadioNetwork::search();}
        else if(x>=112){if((scanPage+1)*ScanRows<list.count)scanPage++;}
        else if(scanPage)scanPage--;
      }else if(y>=55&&y<55+32*ScanRows&&x>=12&&x<308){
        const int index=scanPage*ScanRows+(y-55)/32;
        if(index<list.count){
          const auto& ap=list.ap[index];
          memset(&wifiInput,0,sizeof(wifiInput));
          memcpy(wifiInput.ssid,ap.name,sizeof(wifiInput.ssid));
          if(ap.open){wifiError=!RadioNetwork::send(wifiInput);page=Page::Network;}
          else{keySet=0;page=Page::Keyboard;}
        }
      }
    }
  }else if(page==Page::Keyboard){
    char* field=wifiInput.key;
    size_t n=strlen(field);
    if(y>=211){
      if(x>=160){if(!n)return;wifiError=!RadioNetwork::send(wifiInput);page=Page::Network;}
      else{wifiInput.ssid[0]=0;page=Page::WifiScan;}
      memset(field,0,sizeof(wifiInput.key));
    }else if(y>=172){
      if(x<107)keySet=(keySet+1)%keyPages;
      else if(x<213){if(n)field[n-1]=0;}
      else memset(field,0,sizeof(wifiInput.key));
    }else if(n<sizeof(wifiInput.key)-1&&y>=60&&y<171&&x>=4&&x<316){
      const int k=keySet*keysPerPage+((y-60)/37)*8+(x-4)/39;
      if(k<int(sizeof(wifiKeys)-1)){field[n]=wifiKeys[k];field[n+1]=0;}
    }
  }else if(page==Page::Gain){
    if(y>=218)page=Page::Settings;
    else if(y>=179&&y<213&&x>=12&&x<308)page=Page::Sync;
    else if(y>=101&&y<135&&x>=164&&x<308)page=Page::Audio;
    else if(y>=66&&y<100){agcAutomatic=!agcAutomatic;saveAgc();}
    else if(y>=145&&y<179){
      agcAutomatic=false;
      agcAttenuation=constrain(int(min(agcAttenuation,uint8_t(mode==0?26:37)))+(x<160?-1:1),0,mode==0?26:37);
      saveAgc();
    }
  }else if(page==Page::Audio){
    if(x>=12&&x<308){
      if(y>=218)page=Page::Gain;
      else if(y>=48&&y<82){
        autoFilter=!autoFilter;prefs.putBool("filter-auto",autoFilter);
        if(!autoFilter&&narrowFilter){narrowFilter=false;if(ready)applyChannelFilter();}
      }else if(y>=108&&y<142){
        audioEnabled=!audioEnabled;prefs.putBool("audio-enable",audioEnabled);
        if(ready)applyAudioOutput();
      }
    }
  }else if(page==Page::Sync){
    if(x>=12&&x<308){
      if(y>=218)page=Page::Gain;
      else if(y>=144&&y<178){syncPreferred=!syncPreferred;prefs.putBool("sync-auto",syncPreferred);setCarrierSync(syncPreferred);if(carrierSync)control=Control::Tune;}
    }
  }else if(page==Page::Reception){
    if(y>=218)page=Page::Settings;
  }else if(page==Page::Appearance){
    if(y>=218)page=Page::Settings;
    else if(y>=179&&y<213){language=Language((uint8_t(language)+1)%LANGUAGE_COUNT);prefs.putUChar("language",uint8_t(language));frameValid=false;}
    else if(y>=67&&y<101){hudStyle=(hudStyle+1)%HUD_COUNT;prefs.putUChar("hud-style",hudStyle);}
    else if(y>=110&&y<144){theme=(theme+1)%THEME_COUNT;applyTheme();prefs.putUChar("theme",theme);}
  }else if(page==Page::Bands){
    if(y>=212){
      if(x<108)bandPage=(bandPage+(BAND_COUNT+8)/9-1)%((BAND_COUNT+8)/9);
      else if(x<211)page=Page::Settings;
      else bandPage=(bandPage+1)%((BAND_COUNT+8)/9);
    }else if(y>=69&&y<187&&x>=10&&x<310){
      int col=(x-10)/103,row=(y-69)/42;
      if(col>=3||(x-10)%103>=94||(y-69)%42>=34)return;
      int j=row*3+col;int i=bandPage*9+j;
      if(i<BAND_COUNT){selectBand(uint8_t(i));
        control=Control::Tune;configureRadio();changed();page=Page::Radio;}
    }
  }else if(page==Page::Memories){
    if(y>=214)page=Page::Radio;
    else if(y>=84&&y<208){int i=(y-84)/31;
      if(held){memories[i]={frequency,band,mode};prefs.putBytes("mem",memories,sizeof(memories));}
      else if(memories[i].freq){rememberBandTuning();frequency=memories[i].freq;band=memories[i].band;mode=memories[i].mode;stepIndex=bandTuning[band].step;control=Control::Tune;configureRadio();changed();page=Page::Radio;}
    }
  }else if(y>=214)page=Page::Radio;
  dirty=true;
}
void setup(){
  Serial.begin(115200);Serial.printf("Motivo de arranque: %d\n",int(esp_reset_reason()));Serial.println("Radio - aplicacion propia");
  // Arduino-ESP32 rejects digitalWrite before pinMode (confirmed in boot log).
  // Active-low backlight: configure first, then hide the panel immediately.
  pinMode(LIGHT,OUTPUT);digitalWrite(LIGHT,HIGH);
  pinMode(32,OUTPUT);digitalWrite(32,LOW);
  pinMode(EA,INPUT_PULLUP);pinMode(EB,INPUT_PULLUP);pinMode(KEY,INPUT);
  oldAB=(digitalRead(EA)<<1)|digitalRead(EB);
  attachInterrupt(digitalPinToInterrupt(EA),encoderEdge,CHANGE);attachInterrupt(digitalPinToInterrupt(EB),encoderEdge,CHANGE);
  screen.init();screen.setRotation(1);
  // Clear undefined panel RAM while the backlight is still off.
  screen.fillScreen(0x0002);
  uint16_t cal[5]={387,3530,246,3555,7};screen.setTouch(cal);
  prefs.begin("horizonte-new",false);
  const uint8_t savedLanguage=prefs.getUChar("language",0);
  language=savedLanguage<LANGUAGE_COUNT?Language(savedLanguage):Language::Spanish;
  autoFilter=prefs.getBool("filter-auto",true);
  audioEnabled=prefs.getBool("audio-enable",true);
  syncPreferred=prefs.getBool("sync-auto",true);
  agcAutomatic=prefs.getBool("agc-auto",true);
  agcAttenuation=min(prefs.getUChar("agc-index",0),uint8_t(37));
  fmMono=prefs.getBool("fm-mono",false);
  hudStyle=prefs.getUChar("hud-style",0);if(hudStyle>=HUD_COUNT)hudStyle=0;
  theme=prefs.getUChar("theme",1);if(theme>=THEME_COUNT)theme=1;
  if(!prefs.getBool("studio-ui",false)){theme=1;prefs.putUChar("theme",theme);prefs.putBool("studio-ui",true);}
  applyTheme();
  band=prefs.getUChar("band",0);if(band>=BAND_COUNT)band=0;
  mode=prefs.getUChar("mode",band?1:0);if(band==0)mode=0;else if(mode<1||mode>3)mode=1;
  frequency=prefs.getUShort("freq",bands[band].initial);frequency=constrain(frequency,bands[band].lo,bands[band].hi);
  if(band==CB_BAND)frequency=cbChannels[cbChannelIndex(frequency)];
  stepIndex=prefs.getUChar("step",bands[band].preferredStep);if(stepIndex>2)stepIndex=bands[band].preferredStep;
  // New storage is separate from favorites and the legacy active-band keys.
  for(uint8_t i=0;i<BAND_COUNT;i++)
    bandTuning[i]={bands[i].initial,bands[i].preferredMode,bands[i].preferredStep};
  // Once migrated, the band table is authoritative, not the legacy scalar keys.
  const bool haveBandTable=prefs.getBytesLength("band-tune-v1")==sizeof(bandTuning)&&
    prefs.getBytes("band-tune-v1",bandTuning,sizeof(bandTuning))==sizeof(bandTuning);
  for(uint8_t i=0;i<BAND_COUNT;i++){
    auto &state=bandTuning[i];
    if(state.frequency<bands[i].lo||state.frequency>bands[i].hi||state.step>2||
       (i==0?state.mode!=0:(state.mode<1||state.mode>3)))
      state={bands[i].initial,bands[i].preferredMode,bands[i].preferredStep};
  }
  if(haveBandTable){
    frequency=bandTuning[band].frequency;
    mode=bandTuning[band].mode;
    stepIndex=bandTuning[band].step;
    if(band==CB_BAND)frequency=cbChannels[cbChannelIndex(frequency)];
  }
  rememberBandTuning();
  volume=constrain(prefs.getUChar("vol",30),0,63);
  if(prefs.isKey("mem")&&prefs.getBytesLength("mem")==sizeof(memories))prefs.getBytes("mem",memories,sizeof(memories));
  for(auto &m:memories)if(m.band>=BAND_COUNT||m.mode>3||(m.freq&&((m.band==0&&m.mode!=0)||(m.band!=0&&m.mode==0)))||(m.freq&&(m.freq<bands[m.band].lo||m.freq>bands[m.band].hi)))m={0,0,0};
  Wire.begin(21,22);
  int address=receiver.getDeviceI2CAddress(RESET);ready=address!=0;
  if(ready){receiver.setDeviceI2CAddress(address==0x11?0:1);receiver.setAudioMuteMcuPin(MUTE);configureRadio();}
  Serial.println(ready?"Receptor detectado":"Receptor no detectado; no se enviaran ordenes de sintonia");
  welcome();
  // The cover and its first animation frame are now fully drawn.
  digitalWrite(LIGHT,LOW);
  wifiError=!RadioNetwork::restore(wifiInput);
}
void loop(){
  if(bootAnimating){welcome();yield();return;}
  if(!uiAvailable){
    // Retry safely without drawing through a null canvas.
    static uint32_t uiRetryAt=0;
    constexpr uint32_t UI_RETRY_INTERVAL_MS=1000;
    if(uint32_t(millis()-uiRetryAt)>=UI_RETRY_INTERVAL_MS){
      uiRetryAt=millis();
      canvas.setColorDepth(8);
      if(canvas.createSprite(320,240)){
        uiAvailable=true;frameValid=false;dirty=true;
            }
    }
    yield();return;
  }
  if(radioStage!=RadioStage::Idle){configureRadio();yield();return;}
  if(RadioNetwork::service()&&(page==Page::Radio||page==Page::Network||page==Page::WifiScan))dirty=true;
  if(HfActivity::service()&&page==Page::Activity)dirty=true;
  serviceSeek();
  int32_t delta;
  portENTER_CRITICAL(&encoderLock);delta=turns;turns=0;portEXIT_CRITICAL(&encoderLock);
  if(delta){
    if(page==Page::Radio&&!radioNavigation)tune(delta);
    else {
      if(dirty||focusPage!=page)draw();
      if(focusCount){focusIndex=((focusIndex+delta)%focusCount+focusCount)%focusCount;dirty=true;}
    }
  }
  // Debounce both edges; act on release so a long press is not a short press too.
  static bool keyRawLast=false,keyStable=false;
  static uint32_t keyEdgeAt=0,keyDownAt=0;
  bool keyRaw=analogRead(KEY)<500;
  uint32_t now=millis();
  if(keyRaw!=keyRawLast){keyRawLast=keyRaw;keyEdgeAt=now;}
  if(keyRaw!=keyStable&&now-keyEdgeAt>=35){
    keyStable=keyRaw;
    if(keyStable)keyDownAt=now;
    else {
      bool held=now-keyDownAt>=700;
      if(page==Page::Radio){
        if(held){
          radioNavigation=!radioNavigation;
          if(radioNavigation)focusIndex=control==Control::Volume?1:control==Control::Fine?2:0;
        }else if(radioNavigation){
          if(dirty||focusPage!=page)draw();
          if(focusCount){const auto f=focusTargets[focusIndex];tap(f.x+f.w/2,f.y+f.h/2,false);}
        }else {
          // A short press enters navigation; subsequent presses confirm.
          radioNavigation=true;
          if(dirty||focusPage!=page)draw();
          focusIndex=0;
          // Start on Ajustes, leaving tuning and volume explicitly selectable.
          for(int i=0;i<focusCount;i++){
            if(focusTargets[i].y>=208){focusIndex=i;break;}
          }
        }
        dirty=true;
      }else {
        if(dirty||focusPage!=page)draw();
        if(focusCount){const auto f=focusTargets[focusIndex];tap(f.x+f.w/2,f.y+f.h/2,held);}
      }
    }
  }
  if(radioStage!=RadioStage::Idle){yield();return;}
  static bool touching=false,touchCancelled=false;static uint32_t touchAt=0;static uint16_t tx=0,ty=0;
  uint16_t x,y;bool down=screen.getTouch(&x,&y);
  if(down&&!touching){touchAt=millis();tx=x;ty=y;touching=true;touchCancelled=false;}
  if(down&&touching&&(abs(int(x)-int(tx))>14||abs(int(y)-int(ty))>14))touchCancelled=true;
  if(!down&&touching){touching=false;if(!touchCancelled)tap(tx,ty,millis()-touchAt>=700);}
  if(radioStage!=RadioStage::Idle){yield();return;}
  bool signalPolled=false;
  if(ready&&!seeking&&uint32_t(millis()-signalAt)>=SIGNAL_INTERVAL_MS){
    signalPolled=true;signalAt=millis();
    receiver.getCurrentReceivedSignalQuality(0);
    // RSSI and SNR must come from the same successful RSQ command.
    // VALID describes a station threshold, not whether a weak reading is real.
    if(receiver.signalResponseValid()){
      const uint8_t r=receiver.getCurrentRSSI(),s=receiver.getCurrentSNR();
      if(!signalValid||r!=rssi||s!=snr){
        if(page==Page::Radio||page==Page::Reception)dirty=true;
      }
      if(hudStyle==3&&page==Page::Radio){
        if(traceSize<48)dirty=true;
        else for(uint8_t v:trace)if(v!=r){dirty=true;break;}
      }
      trace[traceHead]=r;
      traceHead=(traceHead+1)%48;
      if(traceSize<48)++traceSize;
      signalValid=true;signalReceivedAt=millis();rssi=r;snr=s;
      if(mode==0){
        const bool pilot=receiver.getCurrentPilot();
        const uint8_t blend=receiver.getCurrentStereoBlend();
        if((pilot!=fmPilot||((blend>0)!=(fmBlend>0)))&&page==Page::Reception)dirty=true;
        fmPilot=pilot;fmBlend=blend;
      }
      if(autoFilter&&mode!=0){
        const bool next=narrowFilter?s<FILTER_WIDE_DB:s<=FILTER_NARROW_DB;
        if(next!=narrowFilter){narrowFilter=next;applyChannelFilter();if(page==Page::Audio)dirty=true;}
      }
      recordRssi(r,s,signalReceivedAt);
    }
  }
  if(signalValid&&uint32_t(millis()-signalReceivedAt)>=SIGNAL_STALE_MS){
    signalValid=false;traceHead=traceSize=0;rssi=snr=0;fmPilot=false;fmBlend=0;
    signalPeak[0]=signalPeak[1]=0;
    if(page==Page::Radio||page==Page::Reception)dirty=true;
  }
  if(ready&&!seeking&&!signalPolled&&mode==0&&millis()-rdsAt>=RDS_POLL_MS){
    rdsAt=millis();
    // Drain a bounded batch after rendering; never monopolize I2C or inputs.
    for(uint8_t i=0;i<RDS_MAX_GROUPS_PER_POLL;i++){
      receiver.getRdsStatus(1,0,0);processRds();
      const auto& response=receiver.rdsResponse().resp;
      if(response.ERR||!response.CTS||!response.RDSFIFOUSED)break;
    }
    if(rdsSeen&&millis()-rdsSeen>15000){
      publishRds(stationName,"");publishRds(radioText,"");
      rdsSeen=0;rdsPi=0;pendingPi=0;piHits=0;clearRdsAssembly();
    }
  }
  if(page==Page::Radio&&mode==0&&radioText.length()&&uiAvailable){
    const uint32_t now=millis();
    const uint32_t interval=(rdsScrollOffset&&!rdsScrollAtEnd)?RDS_SCROLL_MS:RDS_SCROLL_PAUSE_MS;
    if(now-rdsScrollAt>=interval){
      rdsScrollAt=now;
      if(rdsScrollAtEnd){
        rdsScrollOffset=0;rdsScrollAtEnd=false;dirty=true;
      }else if(canvas.textWidth(radioText.c_str()+rdsScrollOffset,1)>RDS_TEXT_WIDTH){
        ++rdsScrollOffset;dirty=true;
        rdsScrollAtEnd=canvas.textWidth(radioText.c_str()+rdsScrollOffset,1)<=RDS_TEXT_WIDTH;
      }
    }
  }
  constexpr uint32_t TUNING_SAVE_MS=2500;
  if(savePending&&!seeking&&uint32_t(millis()-changedAt)>=TUNING_SAVE_MS){
    rememberBandTuning();
    const bool saved=prefs.putBytes("band-tune-v1",bandTuning,sizeof(bandTuning))==sizeof(bandTuning)
      &&prefs.putUShort("freq",frequency)==sizeof(frequency)
      &&prefs.putUChar("mode",mode)==sizeof(mode)
      &&prefs.putUChar("step",stepIndex)==sizeof(stepIndex)
      &&prefs.putUChar("vol",volume)==sizeof(volume)
      &&prefs.putUChar("band",band)==sizeof(band);
    savePending=!saved;
    if(!saved){Serial.println("Error guardando sintonia; se reintentara");changedAt=millis();}
  }
  static uint32_t frameAt=0;
  if(dirty && uint32_t(millis()-frameAt)>=33){frameAt=millis();draw();}
  yield();
}
