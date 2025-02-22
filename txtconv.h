#ifndef TXTCONV_H
#define TXTCONV_H

#include <wchar.h>

const wchar_t* a2w(const char* str, int cp);
const char* w2a(const wchar_t* str, int cp);
const char* a2u8(const char*);
const char* u82a(const char*);

#ifdef USE_LUA

int luaopen_txtconv(lua_State*);

#endif

#ifdef TXTCONV_IMPL

#include <Windows.h>

typedef struct tls_buf {
	char* buf;
	int m;
} tls_buf;

static _declspec(thread) tls_buf TBUF = { 0 };

static inline void checkbuf(int len)
{
	if (TBUF.m < len) {
		TBUF.buf = (char*)realloc(TBUF.buf, len);
		TBUF.m = len;
	}
}

const wchar_t* a2w(const char* str, int cp)
{
	int len = MultiByteToWideChar(cp, 0, str, -1, NULL, 0) + 1;
	checkbuf(len * sizeof(wchar_t));
	MultiByteToWideChar(cp, 0, str, -1, (wchar_t*)TBUF.buf, TBUF.m / sizeof(wchar_t));
	return (wchar_t*)TBUF.buf;
}

const char* w2a(const wchar_t* str, int cp)
{
	int len = WideCharToMultiByte(cp, 0, str, -1, NULL, 0, NULL, NULL) + 1;
	checkbuf(len);
	WideCharToMultiByte(cp, 0, str, -1, TBUF.buf, TBUF.m, NULL, NULL);
	return TBUF.buf;
}

const char* u82a(const char* u8)
{
	a2w(u8, CP_UTF8);
	char* buf = malloc(TBUF.m);
	memcpy(buf, TBUF.buf, TBUF.m);
	w2a((const wchar_t*)buf, 0);
	free(buf);
	return TBUF.buf;
}

const char* a2u8(const char* str)
{
	a2w(str, 0);
	char* buf = malloc(TBUF.m);
	memcpy(buf, TBUF.buf, TBUF.m);
	w2a((const wchar_t*)buf, CP_UTF8);
	free(buf);
	return TBUF.buf;
}

#ifdef USE_LUA

static int la2w(lua_State* L)
{
	int len = 0;
	const char* str = lua_tolstring(L, 1, &len);
	int cp = lua_tointeger(L, 2);
	luaL_Buffer buf;
	int count = MultiByteToWideChar(cp, 0, str,len,NULL, 0);
	luaL_buffinit(L, &buf);
	luaL_prepbuffsize(&buf, count * sizeof(wchar_t));
	MultiByteToWideChar(cp, 0, str, len, luaL_buffaddr(&buf), count);
	luaL_pushresultsize(&buf,count * sizeof(wchar_t));
	return 1;
}

static int lw2a(lua_State* L)
{
	int len = 0;
	const char* str = lua_tolstring(L, 1, &len);
	int cp = lua_tointeger(L, 2);
	luaL_Buffer buf;
	int count = WideCharToMultiByte(cp, 0, (LPCWCH)str,len / sizeof(wchar_t), NULL, 0, NULL, NULL);
	luaL_buffinit(L, &buf);
	luaL_prepbuffsize(&buf, count);
	WideCharToMultiByte(cp, 0, (LPCWCH)str,len / sizeof(wchar_t),luaL_buffaddr(&buf),count, NULL, NULL);
	luaL_pushresultsize(&buf,count);
	return 1;
}

static int lu82a(lua_State* L)
{
	lua_pushcfunction(L, la2w);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, CP_UTF8);
	lua_call(L, 2, 1);
	lua_pushcfunction(L, lw2a);
	lua_pushvalue(L, -2);
	lua_pushinteger(L, CP_ACP);
	lua_call(L, 2, 1);
	return 1;
}

static int lu82w(lua_State* L)
{
	lua_pushcfunction(L, la2w);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, CP_UTF8);
	lua_call(L, 2, 1);
	return 1;
}

static int la2u8(lua_State* L)
{
	lua_pushcfunction(L, la2w);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, CP_ACP);
	lua_call(L, 2, 1);
	lua_pushcfunction(L, lw2a);
	lua_pushvalue(L, -2);
	lua_pushinteger(L, CP_UTF8);
	lua_call(L, 2, 1);
	return 1;
}

static int lw2u8(lua_State* L)
{
	lua_pushcfunction(L, lw2a);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, CP_UTF8);
	lua_call(L, 2, 1);
	return 1;
}

int luaopen_txtconv(lua_State* L)
{
	static luaL_Reg f[] = {
		{"A",lu82a},
		{"W",lu82w},
		{"U8",la2u8},
		{"U8W",lw2u8},
		{"a2w",la2w},
		{"w2a",lw2a},
		{NULL,NULL}
	};
	lua_newtable(L);
	luaL_setfuncs(L, f, 0);
	return 1;
}

#endif

#endif




#endif