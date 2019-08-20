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

#if defined(LUAJIT_VERSION)
DECLARE_LOGGER("luajit");
#else
DECLARE_LOGGER("lua");
#endif

namespace lua__stage {

/* */
static int __log4cxx(log4cxx::LoggerPtr logger, log4cxx::LevelPtr level, lua_State *L, int start, int end)
{
	lua_Debug info;

	if (lua_getstack(L, 1, &info) == 0) _WARN("%d: lua_getstack", __LINE__)
	else {
		lua_getinfo(L, "nSl", &info);

		std::stringstream msg;
		log4cxx::spi::LocationInfo location(info.short_src, info.what, info.currentline);

		lua_getglobal(L, "tostring");
		for (long __l = 0; start <= end; ++start)
		{
			lua_pushvalue(L, -1);
			lua_pushvalue(L, start);
			lua_call(L, 1, 1);
			{
				const char *v = lua_tostring(L, -1);

				if (v == NULL)
					return luaL_error(L, "'tostring' must return a string to 'stage.log'");
				else {
					if ((msg.seekg(0, std::ios_base::end).tellg() - __l) > 0)
						msg << "\t";
					__l = msg.tellg();
				}

_gW:            {
					const char *x = strchr(v, '%');
					if (x != NULL)
					{
						msg << std::string(v, x - v);
						const char *p = ++x;

						for (int length = -2; *(p + 0); ++p)
							switch (length)
							{
								case -2:
								{
									if (isdigit(*(p + 0)));
									else switch (*(p + 0))
									{
										case 'l': msg << location.getFileName();
										{
											msg << std::format("[%s:%d]",
											                   location.getMethodName().c_str(), location.getLineNumber());
										} break;

										case 'F': msg << location.getFileName(); break;
										case 'M': msg << location.getMethodName(); break;
										case 'L': msg << location.getLineNumber(); break;

										case '{': length = -1;
										{
											std::string s(x, p - x);

											if (s.length() > 0)
												length = strtol(s.c_str(), NULL, 10);
											x = p + 1;
										} break;
										default: v = p; goto _gW;
									}
							} break;

							default: if (*(p + 0) != '}') break;
							{
								std::string k(x, p - x);

								switch (*(x + 0))
								{
									case '=': /* log4cxx */
									{
										char *n = (char *)(k.c_str() + 1); /* =logname:level */
										char *l = strchr(n, ':');

										if (l != NULL) *l++ = 0;
										{
											if (*(n + 0) != 0) logger = log4cxx::Logger::getLogger(n);

											if ((l == NULL) || (*(l + 0) == 0));
											else if (strcasecmp(l, "info") == 0) level = log4cxx::Level::getInfo();
											else if (strcasecmp(l, "warn") == 0) level = log4cxx::Level::getWarn();
											else if (strcasecmp(l, "error") == 0) level = log4cxx::Level::getError();
											else if (strcasecmp(l, "trace") == 0) level = log4cxx::Level::getTrace();
										}
									} break;

									default: if (length == 0) break;
									{
										const char *e = getenv(k.c_str());
										if (e == NULL);
										else if (length < 0) msg << e;
										else {
											std::string s = e;

											if ((int)s.length() > length) s.resize(length);
											else s.insert(0, length - s.length(), ' ');
											msg << s;
										}
									} break;
								}
							} v = p + 1; goto _gW;
						}
					}
					msg << v;
				}
			}
			lua_pop(L, 1);
		}

		if (logger->isEnabledFor(level)) logger->forcedLog(level, msg.str(), location);
	}
	return 0;
}

static int __trace(lua_State *L) {
	return __log4cxx(logger, log4cxx::Level::getTrace(), L, 1, lua_gettop(L));
}

static int __info(lua_State *L) {
	return __log4cxx(logger, log4cxx::Level::getInfo(), L, 1, lua_gettop(L));
}

static int __warn(lua_State *L) {
	return __log4cxx(logger, log4cxx::Level::getWarn(), L, 1, lua_gettop(L));
}

static int __error(lua_State *L) {
	return __log4cxx(logger, log4cxx::Level::getError(), L, 1, lua_gettop(L));
}

/* */
static const luaL_Reg loggerlib[] = {
		{ "out"     , __info     },

		{ "trace"   , __trace    },
		{ "info"    , __info     },
		{ "warn"    , __warn     },
		{ "error"   , __error    },

		{ NULL      , NULL       }
};

};


LUALIB_API "C" int luaopen_logger(lua_State *L)
{
	luaL_newlib(L, lua__stage::loggerlib);
	return 1;
}
/* */

