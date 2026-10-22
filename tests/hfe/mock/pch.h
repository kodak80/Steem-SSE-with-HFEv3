// mock environment for testing disk_hfe.cpp outside Steem
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <string>
typedef unsigned char BYTE; typedef unsigned short WORD; typedef uint32_t DWORD;
#define SSE_DISK_HFE
#define SECTOR_SIZE 512
#define DISK_BYTES_PER_TRACK 6256
#define DISK_HFE_BOOT_FILENAME "HFE_boot.bin"
#define SLASH "/"
#define ASSERTB(x)
#define TRACE_LOG(...) do{ if(g_trace) printf(__VA_ARGS__);}while(0)
#define SSE_ENABLE_TRACE_LOG
#define CHECKPATH(p) (p)
#define FSEEK fseek
#define FWRITE fwrite
#define FREAD fread
#define FPUTC fputc
#define FGETC fgetc
extern int g_trace;
struct EasyStr { std::string s; const char *Text; EasyStr(const char*c=""):s(c){Text=s.c_str();}
  EasyStr(const std::string&x):s(x){Text=s.c_str();}
  EasyStr operator+(const char*c)const{return EasyStr(s+c);} 
  EasyStr& operator=(const EasyStr&o){s=o.s;Text=s.c_str();return *this;}
  EasyStr(const EasyStr&o):s(o.s){Text=s.c_str();} };
extern EasyStr RunDir;
long GetFileLength(FILE*f);
