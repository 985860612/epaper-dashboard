#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#define PROGMEM
constexpr uint16_t GxEPD_BLACK=0,GxEPD_WHITE=65535,GxEPD_RED=0xf800;
template<class A,class B> inline auto min(A a,B b)->decltype(a+b) {return a<b?a:b;}
template<class A,class B> inline auto max(A a,B b)->decltype(a+b) {return a>b?a:b;}
class String {
  std::string s;
public:
  String()=default;String(const char* p):s(p?p:"") {}String(std::string p):s(std::move(p)) {}String(const char* p,unsigned int n):s(p,n) {}
  String(char c):s(1,c) {}String(int v):s(std::to_string(v)) {}String(unsigned int v):s(std::to_string(v)) {}String(long v):s(std::to_string(v)) {}String(unsigned long v):s(std::to_string(v)) {}
  String(float v,unsigned char digits=2):String(double(v),digits) {}String(double v,unsigned char digits=2) {char b[100];snprintf(b,sizeof(b),"%.*f",digits,v);s=b;}
  const char* c_str() const {return s.c_str();}unsigned int length() const {return s.size();}bool isEmpty() const {return s.empty();}
  String substring(unsigned int a,unsigned int b=~0u) const {if(a>=s.size())return "";return s.substr(a,std::min<size_t>(b,s.size())-a);}
  char operator[](size_t i) const {return s[i];}String& operator+=(const String& a) {s+=a.s;return *this;}
  bool operator==(const String& a) const {return s==a.s;}bool operator!=(const String& a) const {return s!=a.s;}
  friend String operator+(const String& a,const String& b) {return a.s+b.s;}
};
