#pragma once
#include <Adafruit_GFX.h>
#include <miniz.h>
#include "font_data.h"
#include <cstring>

inline bool styleDrawingFailed=false;
inline uint8_t glyphCache[FONT_CACHE_BYTES];
inline const Typeface* cachedFace=nullptr;
inline int cachedBlock=-1;
inline size_t cachedBytes=0;
struct TextGlyph { const uint8_t* bits;uint8_t width,height,advance;int8_t dx,dy; };
enum class TextAlign:uint8_t { Left,Center,Right };
struct TextBox { int x,y,w,h; };
inline uint32_t textCode(const char*& p) {
  uint8_t c=static_cast<uint8_t>(*p++);if(c<128)return c;
  int n=c<0xe0?1:c<0xf0?2:3;uint32_t value=c&((1u<<(6-n))-1);
  while(n-- && *p)value=(value<<6)|(static_cast<uint8_t>(*p++)&63);return value;
}
inline uint32_t fontCode(const Typeface& f,int index) {
  int block=index/f.blockGlyphs;uint32_t code=f.firstCodes[block];size_t pos=f.codeOffsets[block];
  for(int i=0;i<index%f.blockGlyphs;++i) {uint32_t delta=f.codes[pos++];if(delta==255) {delta=f.codes[pos]+256*f.codes[pos+1];pos+=2;}code+=delta;}return code;
}
inline int fontIndex(const Typeface& f,uint32_t code) {
  int lo=0,hi=(f.count-1)/f.blockGlyphs,block=-1;
  while(lo<=hi) {int mid=(lo+hi)/2;if(f.firstCodes[mid]<=code) {block=mid;lo=mid+1;}else hi=mid-1;}
  if(block<0)return -1;uint32_t value=f.firstCodes[block];int index=block*f.blockGlyphs;size_t pos=f.codeOffsets[block];
  int end=min(int(f.count),index+int(f.blockGlyphs));
  while(value<code && ++index<end) {uint32_t delta=f.codes[pos++];if(delta==255) {delta=f.codes[pos]+256*f.codes[pos+1];pos+=2;}value+=delta;}
  return value==code && index<end?index:-1;
}
inline bool textGlyph(const Typeface& f,uint32_t code,TextGlyph& g) {
  int index=fontIndex(f,code);if(index<0)index=fontIndex(f,'?');if(index<0)return false;
  int block=index/f.blockGlyphs;
  if(cachedFace!=&f || cachedBlock!=block) {
    cachedBytes=tinfl_decompress_mem_to_mem(glyphCache,sizeof(glyphCache),f.data+f.blocks[block],f.blocks[block+1]-f.blocks[block],TINFL_FLAG_PARSE_ZLIB_HEADER|TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if(cachedBytes==TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {cachedFace=nullptr;styleDrawingFailed=true;return false;}
    cachedFace=&f;cachedBlock=block;
  }
  size_t pos=0;
  for(int i=0;i<index%f.blockGlyphs;++i) {if(pos+2>cachedBytes) {styleDrawingFailed=true;return false;}pos+=2+glyphCache[pos]+256*glyphCache[pos+1];}
  if(pos+2>cachedBytes) {styleDrawingFailed=true;return false;}
  const size_t length=glyphCache[pos]+256*glyphCache[pos+1];
  if(pos+7>cachedBytes || length<5 || pos+2+length>cachedBytes) {styleDrawingFailed=true;return false;}
  const uint8_t* b=glyphCache+pos+2;g={b+5,b[0],b[1],b[2],static_cast<int8_t>(b[3]),static_cast<int8_t>(b[4])};
  if(5+((g.width+7)/8)*g.height>length) {styleDrawingFailed=true;return false;}return true;
}
inline const Typeface& textFace(const String& text,TextRole role) {
  const auto& f=roleFont(role);const char* p=text.c_str();
  while(*p)if(fontIndex(f,textCode(p))<0)return FontDense;
  return f;
}
inline int emphasis(TextRole,const Typeface&) {return 0;}
inline int faceWidth(const String& text,const Typeface& f,int tracking=0) {
  int width=0;const char* p=text.c_str();TextGlyph g{};while(*p)if(textGlyph(f,textCode(p),g))width+=g.advance+tracking;return width;
}
inline int textWidth(const String& text,TextRole role=TextRole::Body) {const auto& f=textFace(text,role);return faceWidth(text,f,emphasis(role,f));}
inline String fitText(const String& text,int limit,TextRole role=TextRole::Body) {
  const auto& f=textFace(text,role);int extra=emphasis(role,f);if(faceWidth(text,f,extra)<=limit)return text;
  const String ellipsis=fontIndex(f,0x2026)>=0?"…":"...";int tail=max(faceWidth(ellipsis,f,extra),faceWidth(ellipsis,roleFont(role)));String result;
  const char* p=text.c_str();while(*p) {const char* start=p;textCode(p);String next=result+String(start,static_cast<unsigned int>(p-start));if(max(faceWidth(next,f,extra),faceWidth(next,roleFont(role)))+tail>limit)break;result=next;}
  return limit>=tail?result+ellipsis:String("");
}
inline TextBox textBounds(int x,int baseline,const String& text,TextRole role=TextRole::Body,TextAlign align=TextAlign::Left) {
  const auto& f=textFace(text,role);int extra=emphasis(role,f),advance=faceWidth(text,f,extra);if(align==TextAlign::Right)x-=advance;else if(align==TextAlign::Center)x-=advance/2;
  int minx=x,miny=baseline,maxx=x,maxy=baseline;const char* p=text.c_str();TextGlyph g{};
  while(*p)if(textGlyph(f,textCode(p),g)) {minx=min(minx,x+g.dx);miny=min(miny,baseline+g.dy);maxx=max(maxx,x+g.dx+g.width+extra);maxy=max(maxy,baseline+g.dy+g.height);x+=g.advance+extra;}
  return {minx,miny,maxx-minx,maxy-miny};
}
#ifdef TYPOGRAPHY_TRACE
void typographyTrace(const String&,TextRole,const TextBox&);
#endif
inline void drawText(Adafruit_GFX& d,int x,int baseline,const String& text,TextRole role=TextRole::Body,uint16_t color=0,TextAlign align=TextAlign::Left) {
  const auto& f=textFace(text,role);int extra=emphasis(role,f);if(align==TextAlign::Right)x-=faceWidth(text,f,extra);else if(align==TextAlign::Center)x-=faceWidth(text,f,extra)/2;
#ifdef TYPOGRAPHY_TRACE
  if(!text.isEmpty())typographyTrace(text,role,textBounds(x,baseline,text,role));
#endif
  const char* p=text.c_str();TextGlyph g{};while(*p)if(textGlyph(f,textCode(p),g)) {d.drawBitmap(x+g.dx,baseline+g.dy,g.bits,g.width,g.height,color);if(extra)d.drawBitmap(x+g.dx+1,baseline+g.dy,g.bits,g.width,g.height,color);x+=g.advance+extra;}
}
