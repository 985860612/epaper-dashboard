#define TYPOGRAPHY_TRACE
#include "../firmware/epaper_dashboard/app.h"
#include "../firmware/epaper_dashboard/typography.h"
#include "../firmware/epaper_dashboard/frame_canvas.h"
#include "../firmware/epaper_dashboard/layout_regions.h"
#include <cassert>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>
Settings cfg;Weather weather;Stock stock;String weatherError,stockError;bool rendering=false;
static Adafruit_GFX* painter=nullptr;static bool renderBw=false;static tm currentTm{};static Phase fakePhase=Phase::Trading;static bool fakeClock=true;
bool clockReady() {return fakeClock;}Phase currentPhase() {return fakePhase;}
String dateCompact() {char b[16];strftime(b,sizeof(b),"%Y%m%d",&currentTm);return b;}
struct Trace {String text;TextRole role;TextBox box;};static std::vector<Trace> texts;static bool exactRoles=true;
void typographyTrace(const String& t,TextRole r,const TextBox& b) {texts.push_back({t,r,b});}
#include "../firmware/epaper_dashboard/page_draw.h"
struct Canvas:FrameCanvas {int clipped=0;void drawPixel(int16_t x,int16_t y,uint16_t c) override {if(x<0 || x>=400 || y<0 || y>=300)++clipped;FrameCanvas::drawPixel(x,y,c);}};
static tm date(int year,int month,int day,int hour=12,int minute=59) {tm t{};t.tm_year=year-1900;t.tm_mon=month-1;t.tm_mday=day;t.tm_hour=hour;t.tm_min=minute;mktime(&t);return t;}
struct Image {std::vector<uint8_t> black,red;};
static void save(const Canvas& c,const std::string& name) {
  std::ofstream f(name,std::ios::binary);f<<"P6\n400 300\n255\n";
  for(int y=0;y<300;++y)for(int x=0;x<400;++x) {int i=y*50+x/8;uint8_t bit=128>>(x%8);unsigned char rgb[3]={255,255,255};if(!(c.black[i]&bit))rgb[0]=rgb[1]=rgb[2]=0;else if(!(c.red[i]&bit)) {rgb[0]=176;rgb[1]=22;rgb[2]=43;}f.write(reinterpret_cast<char*>(rgb),3);}
}
static Image render(int page,const tm& now,const std::string& output="") {
  currentTm=now;Canvas c;painter=&c;texts.clear();styleDrawingFailed=false;
  if(page==5)drawStock(now,stock,weather,cfg.city,fakePhase,0,false);else if(page==6)drawFontTest(now);else drawCard(static_cast<CardId>(page),now);
  if(c.clipped) {std::cerr<<"clipped page "<<page<<" pixels="<<c.clipped<<"\n";for(auto& t:texts)std::cerr<<t.text.c_str()<<" ("<<t.box.x<<","<<t.box.y<<","<<t.box.w<<","<<t.box.h<<")\n";}
  assert(!c.clipped && !styleDrawingFailed);
  for(auto& t:texts) {if(exactRoles && textFace(t.text,t.role).size!=roleFont(t.role).size) {std::cerr<<"unexpected fallback "<<t.text.c_str()<<std::endl;assert(false);}auto b=t.box;if(b.x<0 || b.y<0 || b.x+b.w>400 || b.y+b.h>300)std::cerr<<"bad box "<<t.text.c_str()<<"\n";assert(b.x>=0 && b.y>=0 && b.x+b.w<=400 && b.y+b.h<=300);}
  for(size_t i=0;i<texts.size();++i)for(size_t j=i+1;j<texts.size();++j) {
    auto a=texts[i].box,b=texts[j].box;if(a.w && b.w && a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h) {std::cerr<<"text overlap page "<<page<<": "<<texts[i].text.c_str()<<" / "<<texts[j].text.c_str()<<"\n";assert(false);}
  }
  if(page==5 && stock.valid)assert(!texts.empty() && texts[2].role==TextRole::StockName);
  if(page==5) {int rows=0;for(auto& t:texts)if(t.text=="/") {assert((t.role==TextRole::Body || t.role==TextRole::Dense) && t.box.x>=284 && t.box.x<=342);++rows;}bool chart=stock.valid && stock.count>0 && stock.previous>0 && (fakePhase==Phase::Trading || fakePhase==Phase::Unknown);assert(rows==1);if(chart) {for(size_t i=texts.size()-2;i<texts.size();++i)assert(texts[i].role==TextRole::Dense && std::string(texts[i].text.c_str()).find_first_of("/%")==std::string::npos);}}
  if(!output.empty())save(c,output);
  return {std::vector<uint8_t>(c.black,c.black+FrameCanvas::BYTES),std::vector<uint8_t>(c.red,c.red+FrameCanvas::BYTES)};
}
static void within(const Image& a,const Image& b,const RefreshRegion* r,int n,const char* only=nullptr) {
  int changed=0;
  for(int y=0;y<300;++y)for(int x=0;x<400;++x) {int i=y*50+x/8;uint8_t bit=128>>(x%8);if(!(((a.black[i]^b.black[i])|(a.red[i]^b.red[i]))&bit))continue;
    ++changed;bool hit=false;for(int k=0;k<n;++k)if((!only || std::strcmp(only,r[k].name)==0) && x>=r[k].x && x<r[k].x+r[k].w && y>=r[k].y && y<r[k].y+r[k].h)hit=true;
    if(!hit)std::cerr<<"outside partial "<<x<<","<<y<<"\n";assert(hit);
  }assert(changed>0);
}
static void checkTimeInk(const Image& image,int page) {
  RefreshRegion regions[3];int n=layoutRegions(regions,page==6,page==5,true,page<5?static_cast<CardId>(page):CardId::MinimalClock);
  for(int k=0;k<n;++k)if(std::strcmp(regions[k].name,"time")==0 || std::strcmp(regions[k].name,"font_time")==0 || std::strcmp(regions[k].name,"header")==0) {
    auto r=regions[k];for(int y=r.y;y<r.y+r.h;++y)for(int x=r.x;x<r.x+r.w;++x)assert(image.red[y*50+x/8]&(128>>(x%8)));
  }
}
static void checkStockSegmentColors() {
  auto ink=[](const Canvas& c,int x,int y) {int i=y*50+x/8,mask=128>>(x%8);return !(c.black[i]&mask)?GxEPD_BLACK:!(c.red[i]&mask)?GxEPD_RED:GxEPD_WHITE;};
  for(bool reverse:{false,true}) {
    Canvas c;painter=&c;
    if(reverse)stockSegment(10,10,32,30,30,28,30,20);else stockSegment(10,30,28,30,10,32,30,20);
    assert(ink(c,20,20)==GxEPD_BLACK);
    assert(ink(c,15,reverse?15:25)==(reverse?GxEPD_RED:GxEPD_BLACK));
    assert(ink(c,25,reverse?25:15)==(reverse?GxEPD_BLACK:GxEPD_RED));assert(!c.clipped);
  }
  for(float value:{28.f,30.f,32.f}) {Canvas c;painter=&c;stockSegment(10,20,value,30,20,value,30,20);assert(ink(c,20,20)==(value>30?GxEPD_RED:GxEPD_BLACK));}
  {Canvas c;painter=&c;stockSegment(10,20,30,30,10,32,30,20);assert(ink(c,10,20)==GxEPD_BLACK && ink(c,30,10)==GxEPD_RED);}
  {Canvas c;painter=&c;stockSegment(20,30,28,20,10,32,30,20);assert(ink(c,20,25)==GxEPD_BLACK && ink(c,20,15)==GxEPD_RED);}
  renderBw=true;{Canvas c;painter=&c;stockSegment(10,30,28,30,10,32,30,20);assert(ink(c,25,15)==GxEPD_BLACK);}renderBw=false;
}
int main(int argc,char** argv) {
  checkStockSegmentColors();
  assert(stockRate(0)=="0.00%" && stockRate(10)=="10.00%" && stockRate(-10)=="10.00%");
  setenv("TZ","CST-8",1);tzset();std::string out=argc>1?argv[1]:".";std::filesystem::create_directories(out);
  weather.valid=weather.dailyValid=true;weather.temp=9;weather.low=-2;weather.high=12;weather.text="阴";weather.receivedAt=time(nullptr);cfg.city="北京顺义";
  stock.valid=true;stock.name="长江电力";stock.symbol="sh600900";stock.price=29.78;stock.previous=29.91;stock.open=29.65;stock.change=-.13;stock.percent=-.43;stock.stamp="20261109140300";stock.minuteDate="20261109";stock.count=184;
  for(int i=0;i<stock.count;++i)stock.points[i]={i,float(29.53+i*.0012+sin(i*.11)*.025+sin(i*.7)*.005),float(29.54+i*.0007)};
  int glyphs=0;
  for(int role=0;role<10;++role) {const auto& f=roleFont(static_cast<TextRole>(role));constexpr int sizes[]={16,18,20,24,24,48,48,80,112,164};assert(f.size==sizes[role]);int advance=-1;
    for(int i=0;i<f.count;++i) {auto code=fontCode(f,i);assert(fontIndex(f,code)==i);TextGlyph g{};assert(textGlyph(f,code,g) && g.width && g.height);if(code>='0' && code<='9') {if(advance<0)advance=g.advance;assert(advance==g.advance);}++glyphs;}
  }assert(!styleDrawingFailed);
  tm base=date(2026,11,9);assert(cardMonth(base)=="2026 年  11 月");
  for(int p=0;p<7;++p) {auto a=render(p,base,out+"/page-"+std::to_string(p)+".ppm");checkTimeInk(a,p);auto later=base;later.tm_hour=13;later.tm_min=0;auto b=render(p,later);RefreshRegion r[3];int n=layoutRegions(r,p==6,p==5,true,p<5?static_cast<CardId>(p):CardId::MinimalClock);for(int k=0;k<n;++k)assert(r[k].x%8==0 && r[k].w%8==0 && r[k].x>=0 && r[k].x+r[k].w<=400 && r[k].y>=0 && r[k].y+r[k].h<=300);within(a,b,r,n,p==5?"header":p==6?"font_time":"time");}
  exactRoles=false;
  for(int year=2026;year<=2028;++year)for(int month=1;month<=12;++month) {auto t=date(year,month,1);render(1,t);t=date(year,month,calendarMonthDays(year,month));render(1,t);}
  for(const char* city:{"黑龙江省哈尔滨市","内蒙古自治区呼和浩特市","新疆维吾尔自治区乌鲁木齐市"}) {cfg.city=city;weather.text="大暴雨转强浓雾";weather.temp=-120;weather.low=-120;weather.high=120;weatherError="weather unavailable";for(int p=0;p<7;++p)render(p,base);}
  cfg.city="北京顺义";weather.text="阴";weather.temp=9;weather.low=-2;weather.high=12;weatherError="";
  auto a=render(3,base);weather.temp=-20;weather.text="雨夹雪";auto b=render(3,base);RefreshRegion r[3];int n=layoutRegions(r,false,false,true,CardId::MinimalClock);within(a,b,r,n,"weather");
  {Canvas c;painter=&c;texts.clear();int width=textWidth(stockRate(-.27f));stockRow(112,"-0.09",-.30f,width);stockRow(156,"29.84",.03f,width);assert(texts.size()==8 && texts[0].role==TextRole::Body && texts[4].role==TextRole::Body && textFace(texts[4].text,texts[4].role).size==20);
   for(int i:{0,4})assert(texts[i].box.x+texts[i].box.w>=364-width && texts[i].box.x+texts[i].box.w<=366-width);
   for(int i:{3,7})assert(texts[i].box.x+texts[i].box.w>=394 && texts[i].box.x+texts[i].box.w<=396);
   assert(texts[1].box.x==texts[5].box.x);assert(abs((texts[1].box.x-(texts[0].box.x+texts[0].box.w))-(texts[2].box.x-(texts[1].box.x+texts[1].box.w)))<=2);}
  {Stock saved=stock;stock.previous=30;stock.count=3;stock.points[0]={0,27,0};stock.points[1]={120,33,0};stock.points[2]={240,27,0};render(5,base);auto upper=texts[texts.size()-2].box;assert(upper.y>192 && upper.y+upper.h<=274);stock=saved;}
  auto noMeanA=render(5,base);for(int i=0;i<stock.count;++i)stock.points[i].average=10000+i;auto noMeanB=render(5,base);assert(noMeanA.black==noMeanB.black && noMeanA.red==noMeanB.red);
  int redCurve=0;for(int y=132;y<=252;++y)for(int x=20;x<=396;++x)if(!(noMeanA.red[y*50+x/8]&(128>>(x%8))))++redCurve;assert(redCurve==0);
  float savedOpen=stock.open;stock.open=0;auto noOpen=render(5,base);stock.open=10000;auto differentOpen=render(5,base);stock.open=savedOpen;
  assert(noMeanA.black==noOpen.black && noMeanA.red==noOpen.red && noMeanA.black==differentOpen.black && noMeanA.red==differentOpen.red);
  float savedPrevious=stock.previous;stock.previous=29.65;auto mixed=render(5,base);int mixedRed=0,mixedBlack=0;
  for(int y=132;y<=252;++y)for(int x=20;x<=396;++x) {int i=y*50+x/8,mask=128>>(x%8);if(!(mixed.red[i]&mask))++mixedRed;if(!(mixed.black[i]&mask))++mixedBlack;}
  assert(mixedRed>50 && mixedBlack>50);stock.previous=savedPrevious;
  auto stockA=render(5,base);stock.price+=.2f;stock.points[stock.count-1].price+=.1f;auto stockB=render(5,base);RefreshRegion sr[3];int sn=layoutRegions(sr,false,true,true,CardId::MinimalClock);within(stockA,stockB,sr,sn,"stock");stock.price-=.2f;stock.points[stock.count-1].price-=.1f;
  stock.price=10000.00;stock.previous=10001.11;stock.name="贵州茅台测试股份有限公司";render(5,base);stock.valid=false;stock.count=0;stockError="网络请求失败，请稍后重试";render(5,base);render(4,base);
  stock.valid=true;stock.count=0;stock.name="长江电力";render(5,date(2026,11,10));
  for(auto role:{TextRole::Dense,TextRole::Body,TextRole::Title})for(int width=30;width<=230;width+=10) {auto s=fitText("新疆维吾尔自治区乌鲁木齐市XYZ",width,role);assert(textWidth(s,role)<=width);}
  fakePhase=Phase::Unknown;for(int p=0;p<7;++p)checkTimeInk(render(p,base),p);fakeClock=false;Canvas waiting;painter=&waiting;texts.clear();drawClock(base,false,weather,cfg.city,fakePhase);assert(!waiting.clipped && !styleDrawingFailed);fakeClock=true;fakePhase=Phase::Trading;
  const uint8_t badCodes[]={0},badData[]={1,2,3,4,5};const uint16_t first[]={63},codeOffsets[]={0};const uint32_t blocks[]={0,5};Typeface bad={badCodes,first,codeOffsets,blocks,badData,1,18,1};TextGlyph g{};styleDrawingFailed=false;assert(!textGlyph(bad,63,g) && styleDrawingFailed);styleDrawingFailed=false;assert(textGlyph(FontDense,'A',g) && !styleDrawingFailed);
  std::cout<<"PASS: "<<glyphs<<" glyphs, all numeric advances, seven rendered screens, no clipped/overlapping text, time/weather partial bounds, all 36 months, long cities/names, extreme values, missing-data and UTF-8 fitting\n";
}
