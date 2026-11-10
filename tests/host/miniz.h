#pragma once
#include <zlib.h>
#include <cstddef>
constexpr int TINFL_FLAG_PARSE_ZLIB_HEADER=1,TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF=2;
constexpr size_t TINFL_DECOMPRESS_MEM_TO_MEM_FAILED=~size_t(0);
inline size_t tinfl_decompress_mem_to_mem(void* out,size_t cap,const void* in,size_t len,int) {uLongf n=cap;return uncompress(static_cast<Bytef*>(out),&n,static_cast<const Bytef*>(in),len)==Z_OK?n:TINFL_DECOMPRESS_MEM_TO_MEM_FAILED;}
