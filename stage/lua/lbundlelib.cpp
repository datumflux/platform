/*
Copyright (c) 2018-2019 DATUMFLUX CORP.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
 */
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include "single.h"
#include "lua.hpp"
#include <lhelper.hpp>
#include <sys/stat.h>

#include <fstream>
#include <string>
#include <list>

#include "include/well512.h"

#if defined(LUAJIT_VERSION)
DECLARE_LOGGER("luajit");
#else
DECLARE_LOGGER("lua");
#endif

#define _HELPER_TRACE()	        _TRACE("[%u.%p] - %s [%s:%d]", pthread_self(), this, this->SIGN.c_str(), __FUNCTION__, __LINE__)

namespace std {
	typedef basic_string<unsigned char> buffer;
};

namespace lua__stage {

static int __bson(lua_State *L)
{
_gW:switch (lua_type(L, 1))
	{
		case LUA_TSTRING:
		{
			size_t len = 0;
			const char *buffer = lua_tolstring(L, 1, &len);
			uint32_t __x;

			bson_little_endian32(&__x, buffer);
			if (__x != len)
				return luaL_error(L, "%d: This is not BSON data.", __LINE__);
			else {
				lua_newtable(L);
			} luaL_pushBSON(L, -1, buffer);
		} break;

		case LUA_TTABLE: /* dictionary -> json */
		{
			bson_scope b_o;

			if (luaL_toBSON(L, 1, "", &b_o, NULL) <= 0)
				return 0;
			else {
				bson_finish(&b_o);
				lua_pushlstring(L, bson_data(&b_o), bson_size(&b_o));
			}
		} break;

		case LUA_TFUNCTION: luaObjectHelper::call(L, 0, lua_gettop(L) - 1); goto _gW;
		default: break;
	}
	return 1;
}


/* local jv = stage.json("json-string") */
static int __json(lua_State *L)
{
	Json::Value value;
	Json::Reader reader;

_gW:switch (lua_type(L, 1))
	{
		case LUA_TSTRING:
		{
			const char *k = lua_tostring(L, 1);

			switch (*(k + 0))
			{
				case '{': case '[':
				{
					if (reader.parse(k, value)) goto _gD;
				} return luaL_error(L, "%d: %s", __LINE__, reader.getFormattedErrorMessages().c_str());

				default: while (*(k + 0) == ' ') ++k;
				{
					std::ifstream json_file(k);

					if (json_file.is_open() == false)
						return luaL_error(L, "%d: %s [%s]", __LINE__, strerror(ENOENT), k);
					else {
						if (reader.parse(json_file, value))
						{
							json_file.close();
							goto _gD;
						}
					} json_file.close();
				} break;

_gD:            switch (lua_type(L, 2))
				{
					case LUA_TNUMBER: luaL_pushJSON(L, value[(int)lua_tointeger(L, 2)]); break;
					case LUA_TSTRING: luaL_pushJSON(L, value[lua_tostring(L, 2)]); break;

					case LUA_TTABLE:
					{
						Json::Value result;

						for (lua_pushnil(L); lua_next(L, 2); lua_pop(L, 1))
						{
							std::stringset parts;
							Json::Value &root = value;

							/* "a.#2" */
							std::tokenize(lua_tostring(L, -1), parts, ".");
							for (std::stringset::iterator p_it = parts.begin(); p_it != parts.end(); ++p_it)
							{
								Json::Value v;

								switch (*(p_it->c_str() + 0))
								{
									case '#': if (root.isArray() == false) goto _gB;
									{
										long i = strtol(p_it->c_str() + 1, NULL, 10);
										if ((i == LONG_MIN) || (i == LONG_MAX) || (i >= root.size()))
											goto _gB;

										v = root[(int)i];
									} break;

									default: if (root.isObject() == false) goto _gB;
									{
										if (root.isMember(p_it->c_str()) == false)
											goto _gB;

										v = root[p_it->c_str()];
									} break;
								}

								if ((p_it + 1) == parts.end())
								{
									if (lua_type(L, -2) == LUA_TSTRING)
										result[lua_tostring(L, -2)] = v;
									else result.append(v);
								}
								else if (v.isObject() || v.isArray()) root = v;
								else break;
							}
_gB:                        ;
						}
						luaL_pushJSON(L, result);
					} break;
					default: luaL_pushJSON(L, value);
				}
			}
		} break;

		case LUA_TTABLE: /* dictionary -> json */
		{
			if (luaL_toJSON(L, 1, "", &value) > 0)
				lua_pushstring(L, value.toStyledString().c_str());
		} break;

		case LUA_TFUNCTION: luaObjectHelper::call(L, 0, lua_gettop(L) - 1); goto _gW;
		default: break;
	}
	return 1;
}

/* stage.random({}, start, end)
 * stage.random({}, function (f))
 */
static int __random(lua_State *L)
{
	struct _well512 x__w, *w;
	int top = lua_gettop(L);

	if (!(w = (struct _well512 *)lua_touserdata(L, lua_upvalueindex(2))))
		well512_srand(w = &x__w, time(NULL), 1024);

	if (lua_type(L, 1) == LUA_TNONE)
		lua_pushnumber(L, well512_randf(w));
	else {
		int w_idx = 0;

		switch (lua_type(L, 1))
		{
			case LUA_TTABLE:
			{
				for (lua_pushnil(L); lua_next(L, 1); lua_pop(L, 1))
					switch (lua_type(L, -2))
					{
						case LUA_TNUMBER:
						{
							lua_Integer i = lua_tointeger(L, -2);

							if (i == 0) w->index = lua_tointeger(L, -1);
							else if (i <= WELL512_MAX)
								w->state[i - 1] = lua_tointeger(L, -1);
						} break;

						case LUA_TSTRING: if (*(lua_tostring(L, -2) + 0) == 0) switch (lua_type(L, -1))
						{
							case LUA_TTABLE:
							{
								lua_Integer seed = time(NULL);
								lua_Integer delta = 1024;

								for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
									switch (lua_type(L, -2))
									{
										case LUA_TNUMBER: switch (lua_tointeger(L, -2))
										{
											case 1: seed = lua_tointeger(L, -1); break;
											case 2: delta = lua_tointeger(L, -1); break;
										} break;

										default: break;
									}
								well512_srand(w, seed, delta);
							} break;
							case LUA_TNUMBER: well512_srand(w, lua_tointeger(L, -1), 1024);
						} break;
						default: break;
					}
			} w_idx = 1;

			default: switch (lua_type(L, 1 + w_idx))
			{
				case LUA_TFUNCTION:
				{
					{
						lua_pushvalue(L, lua_upvalueindex(1));
						lua_pushlightuserdata(L, w);
					} lua_pushcclosure(L, __random, 2);
					if (luaObjectHelper::call(L, 1 + w_idx, 1) != LUA_OK)
						return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
				} break;

				default:
				{
#if !defined(LUA_MAXINTEGER)
#  define LUA_MAXINTEGER            INT32_MAX
#endif
					lua_Integer low = 1, up = LUA_MAXINTEGER;

					double r = well512_randf(w);
					switch (lua_gettop(L) - w_idx)
					{
						case 0: break;
						case 1: up = luaL_checkinteger(L, 1 + w_idx); break;
						case 2:
						{
							low = luaL_checkinteger(L, 1 + w_idx);
							up = luaL_checkinteger(L, 2 + w_idx);
						} break;
					}

					luaL_argcheck(L, low <= up, 1, "interval is empty");
					luaL_argcheck(L, low >= 0 || up <= LUA_MAXINTEGER + low, 1, "interval too large");
					r *= (double)(up - low) + 1.0;

					lua_pushinteger(L, (lua_Integer)r + low);
				} break;
			} break;
		}

		if (w_idx > 0)
		{
			for (size_t i = 0; i < WELL512_MAX;)
			{
				lua_pushinteger(L, w->state[i]);
				lua_rawseti(L, w_idx, ++i);
			} lua_pushinteger(L, w->index); lua_rawseti(L, w_idx, 0);
		}
	} return lua_gettop(L) - top;
}

/* local v = stage.proxy(object, {
 *    "" = function (object, value),
 *    NAME = function (object, value),
 *    NAME = { V1, V2, ..., function (object, value), ... }
 * });
 *
 * 값: 숫자인 경우 경제 값으로 설정되며 해당 값을 초과하는 경우 함수 호출
 * 문자열: 해당 문자열이 포함되면 호출
 */
struct LUA__Proxy: public luaObjectHelper
{
	LUA__Proxy(const char *i)
			: luaObjectHelper("LUA__Proxy")
			, L(NULL), i(i ? i: "proxy")
			, o(-1), f(-1) { _HELPER_TRACE(); }
	virtual ~LUA__Proxy()
	{
		{
			for (KV_idSet::iterator i_it = kv.begin(); i_it != kv.end(); ++i_it)
				i_it->second->dispose(this->L);

			if (this->L)
			{
				luaL_unref(this->L, LUA_REGISTRYINDEX, this->o);
				if (this->f > 0)
					luaL_unref(this->L, LUA_REGISTRYINDEX, this->f);
			} kv.clear();
		} _HELPER_TRACE();
	}

	/* */
	struct KV: std::buffer {

		typedef std::list<KV *> EDGE_kvSet;

		KV(int max = 1): std::buffer(), f(-1), retain(max) { }
		virtual ~KV() { }

		/* Callback */
		virtual bool call(lua_State *L, LUA__Proxy *o, int c_idx, int k_idx, int v_idx);
		virtual void dispose(lua_State *L)
		{
			if (L && (this->f > 0)) luaL_unref(L, LUA_REGISTRYINDEX, this->f);
			{
				for (EDGE_kvSet::iterator s_it = edges.begin(); s_it != edges.end(); ++s_it)
					(*s_it)->dispose(L);
			} edges.clear();
			delete this;
		}

		/* */
		int        f;     /* function */
		EDGE_kvSet edges;

		std::atomic_int retain;
	}; typedef std::unordered_map<std::string, KV *> KV_idSet;

	virtual int prepare(lua_State *L, int k_idx)
	{
		if (this->f > 0) switch (lua_type(L, k_idx))
		{
			case LUA_TTABLE:
			case LUA_TUSERDATA: lua_rawgeti(L, LUA_REGISTRYINDEX, this->f);
			{
				lua_pushvalue(L, k_idx);                    /* k */
				lua_pushstring(L, this->i.c_str());         /* i */
				lua_rawgeti(L, LUA_REGISTRYINDEX, this->o); /* t */
				switch (luaObjectHelper::call(L, 0, 3, 1))
				{
					default: return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
					case LUA_OK: switch (lua_type(L, -1))
					{
						case LUA_TNIL: return 1; /* ignore */
						default: lua_replace(L, k_idx);
					}
				}
			} break;
			default: break;
		}

		lua_rawgeti(L, LUA_REGISTRYINDEX, this->o);
		return luaL_toindex(L, this->i.c_str(), lua_gettop(L), k_idx);
	}

	virtual int __index(lua_State *L, void *u)
	{
		if (prepare(L, 2) == 0) /* k */
			switch (lua_type(L, -1))
			{
				case LUA_TSTRING:
				{
					const char *k = lua_tostring(L, -1);
					if (*(k + 0) == 0) /* */
					{
						lua_pop(L, 1);
						return 1;
					}
				}
				default: lua_gettable(L, 3);
				{
					luaL_setproxy(L, this->i.c_str()); /* v */
				} break;
			}
		return 1;
	}

	virtual int __newindex(lua_State *L, void *u) /* proxy, K, v */
	{
		KV_idSet::iterator k_it;

		if (prepare(L, 2) == 0) /* P K v t k */
			switch (lua_type(L, -1))
			{
				default: k_it = this->kv.end(); goto _gD;

				case LUA_TNUMBER: k_it = this->kv.find("#"); goto _gD;
				case LUA_TSTRING: k_it = this->kv.find(lua_tostring(L, -1));
				if (k_it == this->kv.end()) k_it = this->kv.find("*");

_gD:		    if ((k_it != this->kv.end()) ||
                        ((k_it = this->kv.find("")) != this->kv.end()))
				{
					KV *kv = k_it->second;

					if (kv->retain == 0)
						;   /* 호출중인 상태에서 다시 호출이되면 값을 그대로 적용한다. */
					else {
						--kv->retain;
						{
							lua_pushvalue(L, -1); /* t k k */
						} lua_gettable(L, 4); /* t k cv */

						int top = lua_gettop(L);
						if (kv->call(L, this, top, 5, 3) && (lua_type(L, top + 1) == LUA_TBOOLEAN))
							switch (lua_toboolean(L, top + 1))
							{
								default: ++kv->retain;
								{
									;
								} return 1;
								case true: break;
							}
						lua_settop(L, top - 1);
						++kv->retain;
					}
				}
				lua_pushvalue(L, 3); /* t k nv */
				lua_settable(L, 4);
			}
		return 1;
	}

	/* */
	virtual int __pairs(lua_State *L, void *u)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, this->o);
		if (luaL_getmetafield(L, 2, "__pairs") == LUA_TNIL) {  /* no metamethod? */
			lua_pushcfunction(L, luaB_next);  /* will return generator, */
			lua_pushvalue(L, 2);
			lua_pushnil(L);
		}
		else {
			lua_pushvalue(L, 2);
			lua_call(L, 1, 3);  /* get 3 values from metamethod */
		} return 3;
	}

	virtual int __ipairs(lua_State *L, void *u)
	{
		lua_pushcfunction(L, ipairsaux);  /* iteration function */
		lua_rawgeti(L, LUA_REGISTRYINDEX, this->o);
		lua_pushinteger(L, 0);  /* initial value */
		return 3;
	}

	virtual int __len(lua_State *L, void *u)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, this->o); {
			lua_len(L, -1);
		} return 1;
	}

	/* */
	virtual int __call(lua_State *L, void *u)
	{
		switch (lua_type(L, 2))
		{
			default: return luaL_error(L, "%d: bad argument #2", __LINE__);

			case LUA_TFUNCTION:
			{
				luaL_pushhelper(L, this, NULL);
			} lua_call(L, 1, LUA_MULTRET);
			case LUA_TNONE: lua_rawgeti(L, LUA_REGISTRYINDEX, this->o);
		} return 1;
	}

	virtual int __tostring(lua_State *L, void *u)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, this->o); {
			size_t pos = this->i.find(':');

			lua_pushfstring(L, "%s:[%s]", (pos == std::string::npos)
			        ? this->i.c_str(): this->i.substr(0, pos).c_str(), luaL_tolstring(L, -1, NULL));
		} return 1;
	}

	lua_State  *L;
	std::string i;

	int         o, f;   /* object, index_function ref */
	KV_idSet    kv;
protected:
	virtual void Dispose() { delete this; }

	/* */
	static int luaB_next (lua_State *L)
	{
		luaL_checktype(L, 1, LUA_TTABLE);
		lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
		if (lua_next(L, 1))
			return 2;
		else {
			lua_pushnil(L);
			return 1;
		}
	}

	static int ipairsaux (lua_State *L)
	{
		lua_Integer i = luaL_checkinteger(L, 2) + 1;
		lua_pushinteger(L, i);
		return (lua_geti(L, 1, i) == LUA_TNIL) ? 1 : 2;
	}
};


bool LUA__Proxy::KV::call(lua_State *L, LUA__Proxy *o, int c_idx, int k_idx, int v_idx)
{
	auto f__call = [&o](lua_State *L, int r_callback, int c_idx, int k_idx, int v_idx)
	{
		int top = lua_gettop(L);

		lua_rawgeti(L, LUA_REGISTRYINDEX, r_callback);
		switch (lua_type(L, -1))
		{
			case LUA_TFUNCTION:
			{
				luaL_pushhelper(L, o, NULL);
				lua_pushvalue(L, c_idx);
				lua_pushvalue(L, (k_idx < 0) ? (top - (k_idx + 1)): k_idx);
				lua_pushvalue(L, (v_idx < 0) ? (top - (v_idx + 1)): v_idx);
			} return (luaObjectHelper::call(L, 0, 4) == LUA_OK);

			default: return false;
		}
	};

	if (this->f > 0)
		return f__call(L, this->f, c_idx, k_idx, v_idx);
	else {
		static std::function<bool(lua_State *L, int i, int v_idx, int8_t *e)> T__lessThen[] = {
				[](lua_State *L, int i, int v_idx, int8_t *e) { return false; },   /* LUA_TNIL */
				[](lua_State *L, int i, int v_idx, int8_t *e) {                    /* LUA_TBOOLEAN */
					if (lua_toboolean(L, v_idx) != *((int *)(e + 1))) return false;
					else if (i > 0)
					{
						lua_pushboolean(L, *((int *)(e + 1)));
						lua_rawseti(L, -2, i);
					} return true;
				},
				[](lua_State *L, int i, int v_idx, int8_t *e) { return false; },   /* LUA_TLIGHTUSERDATA */
				[](lua_State *L, int i, int v_idx, int8_t *e) {                    /* LUA_TNUMBER */
					if (*(e + 1))
					{
						if (lua_tointeger(L, v_idx) < *((lua_Integer *)(e + 2))) return false;
						else if (i > 0)
						{
							lua_pushinteger(L, *((lua_Integer *)(e + 2)));
							lua_rawseti(L, -2, i);
						}
					}
					else if (lua_tonumber(L, v_idx) < *((lua_Number *)(e + 2))) return false;
					else if (i > 0)
					{
						lua_pushnumber(L, *((lua_Number *)(e + 2)));
						lua_rawseti(L, -2, i);
					} return true;
				},
				[](lua_State *L, int i, int v_idx, int8_t *e) {                    /* LUA_TSTRING */
					const char *s = lua_tostring(L, v_idx);
					if (strcmp(s, (const char *)(e + 1))) return false;
					else if (i > 0)
					{
						lua_pushstring(L, (const char *)(e + 1));
						lua_rawseti(L, -2, i);
					} return true;
				},
				[](lua_State *L, int i, int v_idx, int8_t *e) { return false; },   /* LUA_TTABLE */
				[](lua_State *L, int i, int v_idx, int8_t *e) { return false; },   /* LUA_TFUNCTION */
				[](lua_State *L, int i, int v_idx, int8_t *e) { return false; },   /* LUA_TUSERDATA */
				[](lua_State *L, int i, int v_idx, int8_t *e) { return false; },   /* LUA_TTHREAD */
		};

		lua_newtable(L);
		{
			int t_idx = lua_gettop(L);
			int c_type = lua_type(L, c_idx);
			int i__o = (c_type == LUA_TNIL) ? 0: -1;

			EDGE_kvSet::iterator f_it = edges.end();
			for (EDGE_kvSet::iterator e_it = edges.begin(); e_it != edges.end(); ++e_it)
			{
				KV *v = (*e_it);

				if (v->f > 0)
				{
					/* 첫 항목에 함수가 있거나 || 저장된 항목이 있거나 */
					if ((e_it == edges.begin()) || (i__o > 0)) /* 항목이 결정되지 않는 상황 */
					{
						if (f__call(L, v->f, t_idx, k_idx, v_idx) && (lua_type(L, t_idx + 1) == LUA_TBOOLEAN))
							switch (lua_toboolean(L, t_idx + 1))
							{
								default:
								{
									lua_remove(L, t_idx);
								} return true;
								case true: break;
							}

						if (i__o > 0)
						{
							lua_settop(L, t_idx - 1); { /* 테이블 제거 */
								i__o = -1;
							} lua_newtable(L);
						}
					}
					f_it = edges.end(); /* 실행 함수 검색 시작위치 리셋 */
				}
				else if (v->length() == 0);
				else if ((c_type == LUA_TNIL) || (*(v->data() + 0) == c_type))
				{
					int8_t *e = (int8_t *)v->data();

					if (i__o < 0)
					{
						if (T__lessThen[*(e + 0)](L, -1, c_idx, e))
							continue;

						i__o = 0;
					}

					if (T__lessThen[*(e + 0)](L, i__o + 1, v_idx, e))
					{
						++i__o; f_it = e_it;
					}
				}
			}

			if (i__o > 0) for (;f_it != edges.end(); ++f_it)
				if ((*f_it)->f > 0)
				{
					bool r = f__call(L, (*f_it)->f, t_idx, k_idx, v_idx);
					lua_remove(L, t_idx);
					return r;
				}
		} lua_pop(L, 1);
	} return false;
}

static int __proxy(lua_State *L)
{
	switch (lua_type(L, 1))
	{
		case LUA_TTABLE:
		case LUA_TUSERDATA: break;
		default: return luaL_error(L, "%d: Unsupported data types. [%d]", __LINE__, lua_type(L, 1));
	}
	{
		static std::function<bool(lua_State *L, LUA__Proxy::KV *v)> T__packKV[] = {
				[](lua_State *L, LUA__Proxy::KV *v) { return false; },   /* LUA_TNIL */
				[](lua_State *L, LUA__Proxy::KV *v) {                    /* LUA_TBOOLEAN */
					v->resize(sizeof(int8_t) + sizeof(int)); {
					int8_t *o = (int8_t *)v->data();

					*(o + 0) = LUA_TBOOLEAN;
					*((int *)(o + 1)) = lua_toboolean(L, -1);
				} return true;
				},
				[](lua_State *L, LUA__Proxy::KV *v) { return false; },   /* LUA_TLIGHTUSERDATA */
				[](lua_State *L, LUA__Proxy::KV *v) {                    /* LUA_TNUMBER */
					v->resize(sizeof(int8_t) + sizeof(int8_t) +
					          std::max(sizeof(lua_Integer), sizeof(lua_Number))); {
					int8_t *o = (int8_t *)v->data();

					*(o + 0) = LUA_TNUMBER;
					*(o + 1) = lua_isinteger(L, -1);

					if (*(o + 1)) *((lua_Integer *)(o + 2)) = lua_tointeger(L, -1);
					else *((lua_Number *)(o + 2)) = lua_tonumber(L, -1);
				} return true;
				},
				[](lua_State *L, LUA__Proxy::KV *v) {                    /* LUA_TSTRING */
					size_t l; const char *s = lua_tolstring(L, -1, &l);
					v->resize(sizeof(int8_t) + (l + 1)); {
					int8_t *o = (int8_t *)v->data();

					*(o + 0) = LUA_TSTRING;
					memcpy((void *)(o + 1), s, l + 1);
				} return true;
				},
				[](lua_State *L, LUA__Proxy::KV *v) { return false; },   /* LUA_TTABLE */
				[](lua_State *L, LUA__Proxy::KV *v) {                    /* LUA_TFUNCTION */
					lua_pushvalue(L, -1); {
					v->f = luaL_ref(L, LUA_REGISTRYINDEX);
				} return true;
				},
				[](lua_State *L, LUA__Proxy::KV *v) { return false; },   /* LUA_TUSERDATA */
				[](lua_State *L, LUA__Proxy::KV *v) { return false; },   /* LUA_TTHREAD */
		};

		auto T__fetchKV = [](LUA__Proxy *p, const char *k, int max) {
			LUA__Proxy::KV_idSet::iterator k_it;

			if ((k_it = p->kv.find(k)) == p->kv.end())
				k_it = p->kv.insert(
						LUA__Proxy::KV_idSet::value_type(k, new LUA__Proxy::KV(max))
				).first;
			return k_it->second;
		};

		LUA__Proxy *p = new LUA__Proxy(NULL);

		{
			lua_pushvalue(L, 1);
		} p->o = luaL_ref(L, LUA_REGISTRYINDEX);
		switch (lua_type(L, 2))
		{
			case LUA_TTABLE: for (lua_pushnil(L); lua_next(L, 2); lua_pop(L, 1)) /* stage.proxy({}, { ... } */
			{
				switch (lua_type(L, -2))
				{
					case LUA_TNUMBER: switch (lua_type(L, -1))
					{
						case LUA_TFUNCTION:
						{
							lua_pushvalue(L, -1);
							p->f = luaL_ref(L, LUA_REGISTRYINDEX);
						} break;

						case LUA_TSTRING: p->i = lua_tostring(L, -1);
					} break;

					case LUA_TSTRING:
					{
						const char *k = lua_tostring(L, -2);

						if (*(k + 0) == 0) /* [""] = { ... } */
							switch (lua_type(L, -1))
							{
								case LUA_TTABLE:
								{
									for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
										switch (lua_type(L, -2))
										{
											case LUA_TNUMBER: switch (lua_type(L, -1))
											{
												case LUA_TFUNCTION: if (p->f <= 0)
												{
													lua_pushvalue(L, -1);
													p->f = luaL_ref(L, LUA_REGISTRYINDEX);
												} break;

												case LUA_TSTRING: p->i = lua_tostring(L, -1);
											} break;

											case LUA_TSTRING: k = lua_tostring(L, -2);
											{
												/* property name */
												switch (lua_type(L, -1))
												{
													case LUA_TFUNCTION: lua_pushvalue(L, -1);
													{
														T__fetchKV(p, k, 1)->f = luaL_ref(L, LUA_REGISTRYINDEX);
													} break;
												}
											} break;
											default: break;
										}
								} break;

								case LUA_TFUNCTION: lua_pushvalue(L, -1);
								{
									T__fetchKV(p, "", 1)->f = luaL_ref(L, LUA_REGISTRYINDEX);
								} break;

								case LUA_TSTRING: p->i = lua_tostring(L, -1);
								default: break;
							}
						else {
							LUA__Proxy::KV *kv = T__fetchKV(p, k, 1);

							switch (lua_type(L, -1))
							{
								case LUA_TTABLE: lua_pushnil(L);
								{
									LUA__Proxy::KV *v = NULL;

									for (;lua_next(L, -2); lua_pop(L, 1))
									{
										if (v == NULL) v = new LUA__Proxy::KV(1);
										if (T__packKV[lua_type(L, -1)](L, v))
										{
											kv->edges.push_back(v);
											v = NULL;
										}
									}
									if (v != NULL) delete v;
								} break;

								case LUA_TFUNCTION: lua_pushvalue(L, -1);
								{
									kv->f = luaL_ref(L, LUA_REGISTRYINDEX);
								} break;
							}
						}
					} break;

					default: break;
				}
			} break;

			case LUA_TFUNCTION: /* stage.proxy({}, function (t, o, k, v) */
			{
				T__fetchKV(p, "", 1)->f = luaL_ref(L, LUA_REGISTRYINDEX);
			} break;

			case LUA_TSTRING: p->i = lua_tostring(L, 2);
			default: break;
		}

		{
			lua_getglobal(L, "_LUA");
			p->L = (lua_type(L, -1) == LUA_TNIL) ? L: lua_tothread(L, -1);
		} lua_pop(L, 1);
		luaL_pushhelper(L, p, NULL);
		return 1;
	}
}

/* */
struct declare__t;  /* 자료형 */
struct declare__i { /* 자료의 접근 위치 */
	declare__t *io;
	int offset;
	int length;
};

#define DECLUARE__TYPE(P__T, P__F)   \
{   \
	sizeof(P__T),   \
	[](lua_State *L, void *v, struct declare__i *i) {  \
		P__T *x__v = (P__T *)v; \
		\
		if (i->length == 1)  \
			lua_push##P__F(L, *(x__v + 0));    \
		else {  \
			lua_newtable(L);    \
			for (int x__i = 0; x__i < i->length;)    \
			{   \
				lua_push##P__F(L, *(x__v + x__i)); \
				lua_rawseti(L, -2, ++x__i); \
			}   \
		} return 0; \
	},  \
	[](lua_State *L, int idx, void *v, struct declare__i *i) { \
		P__T *x__v = (P__T *)v; \
	\
		if (i->length > 1)   \
			for (lua_pushnil(L); lua_next(L, idx); lua_pop(L, 1))   \
				switch (lua_type(L, -2))    \
				{  \
					case LUA_TNUMBER:   \
					{   \
						int x__i = lua_to##P__F(L, -2) - 1;    \
						\
						if ((x__i >= 0) && (x__i < i->length))   \
							switch (lua_type(L, -1))    \
							{   \
								case LUA_TNUMBER:   \
								{   \
									*(x__v + x__i) = lua_to##P__F(L, -1);  \
								} break;    \
								default: break; \
							}   \
					} break;    \
					default: break; \
				}   \
		else switch (lua_type(L, idx))  \
		{   \
			case LUA_TNUMBER:   \
			{   \
				*(x__v + 0) = lua_to##P__F(L, idx);    \
			} break;    \
			default: break; \
		}   \
		return 0;   \
	}   \
}

static struct declare__t {
	int type_size;

	int (*__index)(lua_State *L, void *v, struct declare__i *i);  /* -> lua */
	int (*__newindex)(lua_State *L, int idx, void *v, struct declare__i *i);    /* <- lua */
} DECLARE__T[] = {
		{
				1,
				[](lua_State *L, void *v, struct declare__i *i) {
					char *x__v = (char *)v;

					(i->length <= 0)
						? lua_pushstring(L, x__v): lua_pushlstring(L, x__v, i->length);
					return 0;
				},
				[](lua_State *L, int idx, void *v, struct declare__i *i) {
					char *x__v = (char *)v;

					size_t l__i = 0;
					const char *l__v = lua_tolstring(L, idx, &l__i);

					if (i->length < 0)
						*(x__v + (l__i = std::min(-(i->length + 1), (int)l__i))) = 0;
					else {
						if ((int)l__i > i->length) l__i = i->length;
						else memset(x__v + l__i, 0, i->length - l__i);
					}
					memcpy(x__v, l__v, l__i);
					return 0;
				}
		},

		DECLUARE__TYPE(int8_t  , integer), /* 1 */
		DECLUARE__TYPE(uint8_t , integer),
		DECLUARE__TYPE(int16_t , integer), /* 3 */
		DECLUARE__TYPE(uint16_t, integer),
		DECLUARE__TYPE(int32_t , integer), /* 5 */
		DECLUARE__TYPE(uint32_t, integer),
		DECLUARE__TYPE(int64_t , integer), /* 7 */
		DECLUARE__TYPE(uint64_t, integer),
		DECLUARE__TYPE(float   , number ), /* 9 */
		DECLUARE__TYPE(double  , number ),
};
#undef DECLUARE__TYPE


/*
 *
 local FILE = { "data", length }
 local FILE = { "data", { "i", "i", "10c" } }

 stage.viewof(FILE, start, nelem, function (buffer)
    buffer[10] = { 10, 2, "HELLO" }
    buffer.commit()
 end)

 stage.viewof("data", 1024, function (buffer)
 end)
*/
#include <fcntl.h>
#include <sys/mman.h>

static int __viewof(lua_State *L)
{
	static struct luaViewOfHelper: public luaObjectHelper {
		struct Struct: public std::deque<struct declare__i> {
			Struct(): std::deque<struct declare__i>(), length(0) { }
			virtual ~Struct() { }

			int length;   /* declare__i 의 전체 크기 */
		};

		struct Segment: public luaObjectHelper {
			Segment(): luaObjectHelper("luaViewOfHelper::Segment")
			{
				this->fd = 0;
				this->m.length = 0;
				this->m.buffer = NULL;

				this->io_elems = 0;
				this->io_table = false;
				this->io_buffer = NULL;
				this->io_owner = NULL;
				_HELPER_TRACE();
			}

			virtual ~Segment()
			{
				if (this->io_owner != NULL)
					this->io_owner->Unref();
				else if (this->fd > 0)
				{
					if (this->m.buffer != MAP_FAILED)
					{
						if (msync(this->m.buffer, this->m.length, 0) < 0)
							_WARN("%d: %s", __LINE__, strerror(errno));

						munmap(this->m.buffer, this->m.length);
					}
					close(this->fd);
				}
				_HELPER_TRACE();
			}

			virtual int __index(lua_State *L, void *u)
			{
				switch (lua_type(L, 2))
				{
					case LUA_TNUMBER:
					{
						char *x__buffer = (char *)u;
						int x__i = lua_tointeger(L, 2) - 1;

						if ((x__i < 0) || (x__i > (int)this->io_structs.size()))
							return luaL_error(L, "%d: %d is out of range.", __LINE__, x__i + 1);
						else {
							struct declare__i &d__i = this->io_structs[x__i];
							d__i.io->__index(L, x__buffer + d__i.offset, &d__i);
						}
					} return 1;
					default: return 0;
				}
			}

			virtual int __newindex(lua_State *L, void *u)
			{
				switch (lua_type(L, 2))
				{
					case LUA_TNUMBER:
					{
						char *x__buffer = (char *)u;
						int x__i = lua_tointeger(L, 2) - 1;

						if ((x__i < 0) || (x__i > (int)this->io_structs.size()))
							return luaL_error(L, "%d: %d is out of range.", __LINE__, x__i + 1);
						else {
							struct declare__i &d__i = this->io_structs[x__i];
							d__i.io->__newindex(L, 3, x__buffer + d__i.offset, &d__i);
						}
					} return 1;
					default: return 0;
				}
			}

			virtual int __ipairs(lua_State *L, void *u)
			{
				{
					lua_pushlightuserdata(L, u);
				} lua_pushcclosure(L, ipairsaux, 1);
				lua_pushlightuserdata(L, this);
				lua_pushinteger(L, 0);
				return 3;
			}

			virtual int __pairs(lua_State *L, void *u) {
				{
					lua_pushlightuserdata(L, u);
				} lua_pushcclosure(L, pairsaux, 1);
				lua_pushlightuserdata(L, this);
				lua_pushstring(L, "");
				return 3;
			}

			virtual int __len(lua_State *L, void *u)
			{
				lua_pushinteger(L, this->io_structs.size());
				return 1;
			}

			virtual int __tostring(lua_State *L, void *u)
			{
				lua_pushfstring(L, "viewof: %p [%d size]", u, this->io_structs.length);
				return 1;
			}

			virtual void Dispose() { delete this; }

			/* */
			int __export(lua_State *L, int x__i)
			{
				if ((x__i < 0) || (x__i >= this->io_elems))
					return -1;
				else {
					char *x__buffer = this->io_buffer + (x__i * this->io_structs.length);

					if (this->io_table == false)
						for (x__i = 0; x__i < (int)this->io_structs.size(); ++x__i)
						{
							struct declare__i &d__i = this->io_structs[x__i];

							d__i.io->__index(L, x__buffer + d__i.offset, &d__i);
						}
					else luaL_pushhelper(L, this, x__buffer);
				} return x__i;
			}

			struct {
				off64_t length;
				void   *buffer;
			} m; int fd;
			Segment *io_owner;

			char  *io_buffer;
			bool   io_table;    /* 테이블로 전달되는 정보인지 확인 */
			int    io_elems;    /* 연결된 항목 수 */
			Struct io_structs;

		private:
			static int pairsaux (lua_State *L)
			{
				const char *k = lua_tostring(L, 2);
				{
					char *x__buffer = (char *)lua_touserdata(L, lua_upvalueindex(1));
					Segment *seg = (Segment *)lua_touserdata(L, 1);
					lua_Integer i = (*(k + 0) == 0) ? 0: strtol(k, NULL, 10);

					if (i >= (int)seg->io_structs.size())
						lua_pushnil(L);
					else {
						struct declare__i &d__i = seg->io_structs[i];

						lua_pushfstring(L, "%d", i + 1);
						d__i.io->__index(L, x__buffer + d__i.offset, &d__i);
						return 2;
					}
				} return 1;
			}

			static int ipairsaux (lua_State *L)
			{
				lua_Integer i = lua_tointeger(L, 2);
				{
					char *x__buffer = (char *)lua_touserdata(L, lua_upvalueindex(1));
					Segment *seg = (Segment *)lua_touserdata(L, 1);

					if (i >= (int)seg->io_structs.size())
						lua_pushnil(L);
					else {
						struct declare__i &d__i = seg->io_structs[i];

						lua_pushinteger(L, i + 1);
						d__i.io->__index(L, x__buffer + d__i.offset, &d__i);
						return 2;
					}
				} return 1;
			}
		};

		luaViewOfHelper(): luaObjectHelper("luaViewOfHelper") { }
		virtual ~luaViewOfHelper() { }

		virtual int __index(lua_State *L, void *u)
		{
			Segment *seg = (Segment *)u;

			switch (lua_type(L, 2))
			{
				case LUA_TSTRING:
				{
					std::string k = lua_tostring(L, 2);

					if (seg->io_owner == NULL)
					{
						if (k == "commit") return link(L, (luaEventClosure_t)__commit, u);
						else if (k == "rollback") return link(L, (luaEventClosure_t)__rollback, u);
					}

					if (k == "scope") return link(L, (luaEventClosure_t)__scope, u);
					/* */
					else if (k == "size") lua_pushinteger(L, seg->io_structs.length);
					else if (k == "count") lua_pushinteger(L, seg->io_elems);
					else return 0;
				} return 1;

				default: break;
			}
			return (seg->__export(L, luaL_checkinteger(L, 2) - 1) < 0) ? 0: 1;
		}

		virtual int __newindex(lua_State *L, void *u)
		{
			Segment *seg = (Segment *)u;
			lua_Integer x__i = luaL_checkinteger(L, 2) - 1;

			if ((x__i < 0) || (x__i >= seg->io_elems))
				return 0;
			else {
				char *x__buffer = seg->io_buffer + (x__i * seg->io_structs.length);

				if (seg->io_table)
					switch (lua_type(L, 3))
					{
						case LUA_TTABLE: for (lua_pushnil(L); lua_next(L, 3); lua_pop(L, 1))
						{
							switch (lua_type(L, -2))
							{
								case LUA_TNUMBER:
								{
									if ((x__i = lua_tointeger(L, -2) - 1) < 0);
									else if (x__i < (int)seg->io_structs.size())
									{
										struct declare__i &d__i = seg->io_structs[x__i];
										d__i.io->__newindex(L, lua_absindex(L, -1), x__buffer + d__i.offset, &d__i);
									}
								} break;
								default: break;
							}
						} break;
						default: goto _gI;
					}
				else
_gI:			{
					struct declare__i &d__i = seg->io_structs[0];
					d__i.io->__newindex(L, 3, x__buffer + d__i.offset, &d__i);
				}
			} return 1;
		}

		virtual int __ipairs(lua_State *L, void *u)
		{
			lua_pushcfunction(L, ipairsaux);
			lua_pushlightuserdata(L, u);
			lua_pushinteger(L, 0);
			return 3;
		}

		virtual int __pairs(lua_State *L, void *u)
		{
			lua_pushcfunction(L, pairsaux);
			lua_pushlightuserdata(L, u);
			lua_pushstring(L, "");
			return 3;
		}

		virtual int __len(lua_State *L, void *u)
		{
			lua_pushinteger(L, ((Segment *)u)->io_elems);
			return 1;
		}

		virtual int __tostring(lua_State *L, void *u)
		{
			Segment *seg = (Segment *)u;

			lua_pushfstring(L,
					"viewof: %p [%d size, %d count]", u, seg->io_structs.length, seg->io_elems);
			return 1;
		}

		virtual int __gc(lua_State *L, void *u)
		{
			((Segment *)u)->Unref();
			return luaObjectHelper::__gc(L, NULL);
		}

		virtual void Dispose() {}
	private:
		static int __commit(lua_State *L, luaViewOfHelper *_this, void *u)
		{
			static const char *const catnames[] = { "async", "sync", NULL };
			static const int msync_flags[] = { MS_ASYNC, MS_SYNC };

			Segment *seg = (Segment *)u;
			{
				int flags = luaL_checkoption(L, 1, "async", catnames);

				if (msync(seg->m.buffer, seg->m.length, msync_flags[flags]) < 0)
					return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
			}
			return 0;
		}

		static int __rollback(lua_State *L, luaViewOfHelper *_this, void *u)
		{
			Segment *seg = (Segment *)u;
			{
				if (msync(seg->m.buffer, seg->m.length, MS_INVALIDATE) < 0)
					return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
			}
			return 0;
		}

		/* buffer.scope(function () ... end) */
		static int __scope(lua_State *L, luaViewOfHelper *_this, void *u)
		{
			static const char *const catnames[] = { "rw", "ro", NULL };
			static const int lock_flags[] = { F_WRLCK, F_RDLCK };
			Segment *seg = (Segment *)u;

			for (luaL_checktype(L, -1, LUA_TFUNCTION); seg->io_owner != NULL; ) seg = seg->io_owner;
			{
				struct flock64 fl; memset(&fl, 0, sizeof(fl));

				fl.l_whence = SEEK_SET;
				fl.l_type = lock_flags[
						(lua_type(L, 1) == LUA_TSTRING) ? luaL_checkoption(L, 1, "rw", catnames): 0
					];
				if (fcntl(seg->fd, F_SETLKW64, &fl) < 0)
					;
				else {
					int top = lua_gettop(L);
					int r = luaObjectHelper::call(L, -1, 0);

					fl.l_type = F_UNLCK;
					if (fcntl(seg->fd, F_SETLK64, &fl) < 0);
					else if (r == LUA_OK) return lua_gettop(L) - top;
					else {
						const char *msg = lua_tostring(L, -1);
						return luaL_error(L, "%d: %s", __LINE__, msg ? msg: "<break>");
					}
				}
			} return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
		}

		/* */
		static int pairsaux (lua_State *L)
		{
			const char *k = lua_tostring(L, 2);
			lua_Integer i = (*(k + 0) == 0) ? 0: strtol(k, NULL, 10);

			lua_pushfstring(L, "%d", i + 1);
			if (((Segment *)lua_touserdata(L, 1))->__export(L, i) >= 0)
				return 2;
			else {
				lua_pushnil(L);
			} return 1;
		}

		static int ipairsaux (lua_State *L)
		{
			lua_Integer i = luaL_checkinteger(L, 2);

			lua_pushinteger(L, i + 1);
			if (((Segment *)lua_touserdata(L, 1))->__export(L, i) >= 0)
				return 2;
			else {
				lua_pushnil(L);
			} return 1;
		}
	} DECLARE__HELPER;

	luaViewOfHelper::Segment *seg = new luaViewOfHelper::Segment();
	std::string file;

	/* 스키마 설정 */
	switch (lua_type(L, 1))
	{
		case LUA_TTABLE: for (lua_pushnil(L); lua_next(L, 1); lua_pop(L, 1))
			switch (lua_type(L, -2))
			{
				case LUA_TNUMBER: switch (lua_type(L, -1))
				{
					case LUA_TUSERDATA:
					{
						void *x__u = NULL;

						if (luaObjectHelper::unwrap(L, -1, (void **)&x__u) == &DECLARE__HELPER)
							(seg->io_owner = (luaViewOfHelper::Segment *)x__u)->Ref();
						else {
							seg->Unref();
							return luaL_error(L, "%d: Not an available object", __LINE__);
						}
					} break;

					case LUA_TSTRING: file = lua_tostring(L, -1); break; /* file */
					case LUA_TNUMBER: /* array_size */
					{
						struct declare__i p__i = {
								DECLARE__T + 2, 0, (int)lua_tointeger(L, -1) /* uint8_t */
						}; seg->io_structs.push_back(p__i);
					} break;

					case LUA_TTABLE: seg->io_table = true;

					for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
						switch (lua_type(L, -2))
						{
							case LUA_TNUMBER: switch (lua_type(L, -1))
							{
								case LUA_TNUMBER:
								{
									struct declare__i p__i = {
											DECLARE__T + 2, 0, (int)lua_tointeger(L, -1) /* uint8_t */
									}; seg->io_structs.push_back(p__i);
								} break;

								case LUA_TSTRING:
								{
									char *v = (char *)lua_tostring(L, -1);

									do {
										struct declare__i p__i = { DECLARE__T, 0, 0 };

_gW:									switch (*(v + 0)) /* i10 */
										{
											case ' ': ++v; goto _gW;
											default: goto _gB;

											case 'd': ++p__i.io;
											case 'f': ++p__i.io;

											case 'L': ++p__i.io;
											case 'l': ++p__i.io;
											case 'I': ++p__i.io;
											case 'i': ++p__i.io;
											case 'H': ++p__i.io;
											case 'h': ++p__i.io;
											case 'B': ++p__i.io;
											case 'b': ++p__i.io;

											case 'c': case 's': break;
										} ++v;

										if (isdigit(*(v + 0)) == false)
											p__i.length = 1;
										else {
											char *x = NULL;

											p__i.length = strtol(v, &x, 10);
											if ((p__i.length < 0) || (p__i.length == LONG_MAX)) goto _gB;
											else if (*(v - 1) == 's') p__i.length = -(p__i.length);
											v = x;
										} seg->io_structs.push_back(p__i);
									} while (*(v + 0) != 0);
								} break;
_gB:							default: break;
							} break;
							default: break;
						} break;
				} break;

				default: break;
			} break;

		case LUA_TUSERDATA:
		{
			void *x__u = NULL;

			if (luaObjectHelper::unwrap(L, 1, (void **)&x__u) == &DECLARE__HELPER)
				(seg->io_owner = (luaViewOfHelper::Segment *)x__u)->Ref();
			else {
				seg->Unref();
				return luaL_error(L, "%d: Not an available object", __LINE__);
			}
		} break;

		case LUA_TSTRING: file = lua_tostring(L, 1); break;
		default: break;
	}

	if ((seg->io_owner == NULL) && file.empty())
		seg->Unref();
	else {
		off64_t i__start = 0;
		off64_t f__length = 0;
		int flags = O_RDWR;

		if (seg->io_owner)
			f__length = seg->io_owner->io_structs.length * seg->io_owner->io_elems;
		else {
			struct stat64 st;

			if (lstat64(file.c_str(), &st) < 0);
			else if (S_ISREG(st.st_mode)) f__length = st.st_size;
			else {
				seg->Unref();
				return luaL_error(L, "%d: Not an accessible file", __LINE__);
			}
		}

		for (size_t x__i = 0; x__i < seg->io_structs.size(); ++x__i)
		{
			struct declare__i &d__i = seg->io_structs.at(x__i);

			d__i.offset = seg->io_structs.length;
			seg->io_structs.length += (d__i.io->type_size * std::abs(d__i.length));
		}

		switch (lua_type(L, 2))
		{
			case LUA_TNUMBER: flags |= O_CREAT;
			{
				switch (lua_type(L, 3))
				{
					case LUA_TNUMBER: i__start = lua_tointeger(L, 2);
					{
						seg->io_elems = lua_tointeger(L, 3);
					} break;
					default: seg->io_elems = lua_tointeger(L, 2);
				}

				if (seg->io_structs.length == 0)
				{
					struct declare__i d__i = {
							DECLARE__T + 2, 0, 1 /* uint8_t */
					}; seg->io_structs.push_back(d__i);
					seg->io_structs.length = d__i.io->type_size * d__i.length;
				}

				seg->m.length = seg->io_structs.length * seg->io_elems;
			} break;
			default: if ((seg->m.length = f__length) == 0) goto _gE;
			{
				if (seg->io_structs.length > 0)
					seg->io_elems = seg->m.length / seg->io_structs.length;
				else {
					struct declare__i d__i = {
							DECLARE__T + 2, 0, 1 /* uint8_t */
					}; seg->io_structs.push_back(d__i);

					seg->io_structs.length = d__i.io->type_size * d__i.length;
					seg->io_elems = (int)(seg->m.length / seg->io_structs.length);
				}
			} break;
		}

		if (seg->io_owner)
		{
			if ((i__start + seg->m.length) <= f__length)
				seg->io_buffer = seg->io_owner->io_buffer + i__start;
			else {
				off64_t x__length = seg->m.length;

				seg->Unref();
				return luaL_error(L, "%d: No buffers available. (%d + %d > %d)",
						__LINE__, (int)i__start, (int)x__length, (int)f__length);
			}
		}
		else if ((seg->fd = open64(file.c_str(), flags, 0644)) < 0) goto _gE;
		else {
			off64_t x__start = PAGE_OFFSET(i__start);

			seg->m.length = PAGE_ALIGN(seg->m.length);
			if ((flags & O_CREAT) && (f__length < (x__start + seg->m.length)))
			{
				static char x__eof = 0;

				if (pwrite64(seg->fd, &x__eof, 1, (x__start + seg->m.length) - 1) < 0)
					goto _gC;
			}

			if ((seg->m.buffer = mmap(NULL, seg->m.length,
					PROT_READ|PROT_WRITE, MAP_SHARED, seg->fd, x__start)) != MAP_FAILED)
				seg->io_buffer = ((char *)seg->m.buffer) + (i__start - x__start);
			else
_gC:		{
				close(seg->fd);
				goto _gE;
			}
		}

		seg->Ref();
		{
			int top = lua_gettop(L);

			luaL_pushhelper(L, &DECLARE__HELPER, seg);
			if (lua_type(L, -2) != LUA_TFUNCTION);
			else if (luaObjectHelper::call(L, -2, 1) != LUA_OK)
			{
				const char *msg = lua_tostring(L, -1);
				return luaL_error(L, "%d: %s", __LINE__, msg ? msg: "<break>");
			}
			return lua_gettop(L) - top;
		}
_gE:	seg->Unref();
		return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
	}
	return luaL_error(L, "%d: No file or object specified to viewof()", __LINE__);
}

static const luaL_Reg bundlelib[] = {

		{ "json"  , __json     },
		{ "bson"  , __bson     },

		{ "random", __random   },
		{ "viewof", __viewof   },
		{ "proxy" , __proxy    },

		/* */
		{ NULL      , NULL       }
};

};


LUALIB_API "C" int luaopen_bundle(lua_State *L)
{
	luaL_newlib(L, lua__stage::bundlelib);
	return 1;
}