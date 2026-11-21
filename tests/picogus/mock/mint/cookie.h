/* Test mock of <mint/cookie.h> */
#pragma once
#define C__VDO 0x5F56444FL
#define C_FOUND 0
#define C_NOTFOUND -1
static inline int Getcookie(long id, long *val) { (void)id; (void)val; return C_NOTFOUND; }
