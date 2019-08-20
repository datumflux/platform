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
#include "include/odbc.h"
#include "lua.hpp"
#include <sys/stat.h>
#include <atomic>
#include <lhelper.hpp>
#include <fstream>
#include <dirent.h>
#include "libzippp.h"
#include "c++/tls.hpp"
#include "pqueue.h"
#include <signal.h>

#if defined(LUAJIT_VERSION)
DECLARE_LOGGER("luajit");
#else
DECLARE_LOGGER("lua");
#endif

#define _HELPER_TRACE()	        _TRACE("[%u.%p] - %s [%s:%d]", pthread_self(), this, this->SIGN.c_str(), __FUNCTION__, __LINE__)
#define SCRIPT_EXPIRETIME       (30 * 1000)

namespace lua__stage {

struct LUA__SYS: public luaObjectRef {

    struct Script: public std::string {
        int    depth;
        time_t mtime;

        Script(): std::string(), depth(0), mtime(0) { }
        Script &operator = (const Script &r) {
            this->depth = r.depth;
            this->mtime = r.mtime;
            this->assign(r.data(), r.length());
            return *this;
        }
    };

    struct Source : public luaObjectRef
    		      , public std::basic_string<unsigned char> {
        Source(int depth, const char *filePath, utime_t timeNow = utimeNow(NULL))
                : luaObjectRef("LUA__SYS::Source")
                , std::basic_string<unsigned char>()
                , script_()
                , timeUpdate_(0) {
            script_.depth = depth;
            script_.assign(filePath);
            if (timeNow && (this->Update(timeNow) < 0)) throw __LINE__;
        }
        virtual ~Source() { }

        virtual int Update(utime_t timeNow, int timeExpire = SCRIPT_EXPIRETIME)
        {
            int modify = 0;

            if (timeUpdate_ > timeNow);
            else if (timeExpire < 0) return timeExpire; /* 강제 오류 발생 */
            else if (timeExpire == 0) ++modify; /* 업데이트 시간 */
            else {
                std::stringset paths;
                struct stat64 st;

                ++modify; std::tokenize(script_, paths, ":");
                if (stat64(paths[0].c_str(), &st) < 0) return -1;
                else if ((st.st_mode & S_IFREG) == 0) return -1;
                else if (st.st_mtime != script_.mtime) switch (paths.size())
                {
                    case 1:
                    {
                        FILE *fp;

                        if (!(fp = fopen(script_.c_str(), "rb"))) return -1;
                        {
                            size_t f_length = st.st_size;

                            this->resize(f_length);
                            for (uint8_t *b = (uint8_t *)this->data(); ;)
                            {
                                size_t l = fread(b, 1, std::min(f_length, (size_t)PAGE_SIZE), fp);

                                if (ferror(fp)) break;
                                else if ((f_length -= l) == 0) break;
                                b += l;
                            }
                        } fclose(fp);
                    } goto _gS;

                    default:
                    {
                        libzippp::ZipArchive zipArchive(paths[0].c_str());

                        if (zipArchive.isOpen() == false)
                            return -1;
                        else {
                            libzippp::ZipEntry f = zipArchive.getEntry(paths[1].c_str());
                            if (f.isFile() == false)
                                return -1;
                            else {
                                this->resize(f.getSize());
                            }

                            memcpy((void *)this->data(), f.readAsBinary(), f.getSize());
                        }
                    }

_gS:                ++modify; script_.mtime = st.st_mtime;  /* 업데이트 됨 */
					_INFO("%d: UPDATE - '%s'", getpid(), paths[0].c_str())
                }

                timeUpdate_ = timeNow + timeExpire;
            } return modify;
        }

        virtual int Load(lua_State *L) {
            return luaL_loadbuffer(L, (const char *)this->data(), this->length(), script_.c_str());
        }

        virtual int Compile(lua_State *L,
                utime_t timeNow = utimeNow(NULL), int timeExpire = SCRIPT_EXPIRETIME)
        {
            int r = Update(timeNow, timeExpire);
            if (r < 0) return LUA_ERRFILE;
            else if (L == NULL);
            else {
                int __x = this->Load(L);
                if (__x != LUA_OK) return __x;
            } return r <= 1 ? LUA_YIELD: LUA_OK;
        }

        const char *getSourcePath() { return script_.c_str(); }
        int         getDepth() { return script_.depth; }

        struct Script operator *() { return script_; }

        virtual Source *Ref() { luaObjectRef::Ref(); return this; }
    protected:
    	virtual void Dispose() { delete this; }

    private:
        struct Script script_;
        utime_t timeUpdate_;
    }; typedef std::unordered_map<std::string, Source *> SOURCE_luaSet;

    virtual int load(lua_State *L, const char *lpszSourceFile, Script *lpScript = NULL);

    /* */
    LUA__SYS()
        : luaObjectRef("LUA__SYS")
        , config(Json::ValueType::objectValue)
        , PROCESS(this) {
        INIT_RWLOCK(&rwlock);
    }

    virtual ~LUA__SYS() {
        {
            for (SOURCE_luaSet::iterator s_it = source.begin(); s_it != source.end(); ++s_it)
                s_it->second->Unref();
        } DESTROY_RWLOCK(&rwlock);
    }

    /* */
    typedef std::unordered_map<std::string, utime_t> OnceSet;

    OnceSet       once;
    SOURCE_luaSet source;
    Json::Value   config;
	RWLOCK_T      rwlock;

    /* */
    struct PROCESS_luaObject : public luaObjectHelper
    {
        PROCESS_luaObject(LUA__SYS *s)
                : luaObjectHelper("PROCESS_luaObject")
                , sys_(s) { }
        virtual ~PROCESS_luaObject() { }

        /* */
        virtual int __index(lua_State *L, void *x__u) {
            std::string method = luaL_checkstring(L, -1);

            if (method == "id") lua_pushinteger(L, getpid());
            else if (method == "tick") lua_pushnumber(L, ((double)utimeNow(NULL)) / 1000.0);
            else if (method == "args")
            {
                const char **argv;
                int argc = stage_args(&argv);

                lua_newtable(L);
                for (int __i = 0; __i < argc; ++__i)
                {
                    lua_pushstring(L, argv[__i]);
                    lua_rawseti(L, -2, __i);
                }
            }
            else if (method == "stage") lua_pushinteger(L, stage_id());
            else switch (*(method.c_str() + 0))
            {
                case '%':
                {
                    const char *v = getenv(method.c_str() + 1);

                    if (v == NULL) lua_pushnil(L);
                    else lua_pushstring(L, v);
                } break;

                case '$':
                {
                    const char *i = method.c_str() + 1;

	                ENTER_RDLOCK(&sys_->rwlock); {
		                if (*(i + 0) == 0) luaL_pushJSON(L, sys_->config);
		                else luaL_pushJSON(L, sys_->config[i]);
	                } LEAVE_RDLOCK(&sys_->rwlock);
                } break;

                default: break;
            }
            return 1;
        }

        virtual int __newindex(lua_State *L, void *u) {
            std::string method = luaL_checkstring(L, -2);

            if (method == "title") stage_setproctitle(luaL_checkstring(L, -1));
            else switch (*(method.c_str() + 0))
            {
                case '%': switch (lua_type(L, -1))
                {
                    case LUA_TNIL: unsetenv(method.c_str() + 1); break;
                    default: setenv(method.c_str() + 1, luaL_checkstring(L, -1), 1);
                } break;

                case '$':
                {
                    Json::Value value;
                    Json::Reader reader;

                    if (reader.parse(luaL_checkstring(L, -1), value) == false)
                        return luaL_error(L, "%d: %s", __LINE__, reader.getFormattedErrorMessages().c_str());
                    else {
                        ENTER_RWLOCK(&sys_->rwlock); {
                            if (*(method.c_str() + 1) != 0) sys_->config = value;
                            else sys_->config[method.c_str() + 1] = value;
                        } LEAVE_RWLOCK(&sys_->rwlock);
                    }
                } break;

                default: break;
            }
            return 1;
        }

        virtual int __tostring(lua_State *L, void *u) {
            lua_pushfstring(L, "process: %p [%d]", this, getpid());
            return 1;
        }

        virtual void Dispose() { }
    private:
        LUA__SYS *sys_;
    } PROCESS;
protected:
    virtual void Dispose() { delete this; }
};

/*
 *
 */
int LUA__SYS::load(lua_State *L, const char *lpszSourceFile, LUA__SYS::Script *lpScript)
{
    utime_t timeNow = utimeNow(NULL);
    guard::rwlock x__lock(&this->rwlock);

    SOURCE_luaSet::iterator s_it = source.find(lpszSourceFile);
    if (s_it != source.end())
	    switch (s_it->second->Compile(L, timeNow, s_it->second->getDepth()
	                  ? -1: config.get("%reflush", Json::Value(SCRIPT_EXPIRETIME)).asInt()))
	    {
		    case LUA_YIELD: /* 이전과 동일하다 */
		    case LUA_OK:
		    {
			    if (lpScript) (*lpScript) = *(*s_it->second);
		    } return LUA_OK;
		    default: s_it->second->Unref();
		    {
			    ;
		    } source.erase(s_it);
	    }

    auto addSource = [&](lua_State *L, int depth, std::string prefix, const char *file) {
        auto compile = [&](const char *f)
        {
            Source *s = new Source(depth, f, timeNow);
            if (s->Load(L) == LUA_OK)
                return source.insert(SOURCE_luaSet::value_type(lpszSourceFile, s->Ref())).first;
            else {
                _WARN("%s: %s", f, lua_tostring(L, -1));
            } s->Unref();

            throw __LINE__;
        };

        char x__path[PATH_MAX + 1];

        if (prefix.find(":") != std::string::npos) /* package: */
            return compile((prefix + file).c_str());
        else {
            char pwd[BUFSIZ];

            if (prefix.length() > 0) prefix += "/";
            if (realpath((prefix + file).c_str(), x__path) == NULL) throw __LINE__;
            else if (getcwd(pwd, sizeof(pwd)))
            {
                const char *l__p = x__path;

                strcat(pwd, "/");
                for (int __i = 0; __i < (int)std::min(strlen(pwd), strlen(x__path)); ++__i)
                {
                    if (*(pwd + __i) != *(x__path + __i)) break;
                    else if (*(x__path + __i) == '/')
                        l__p = x__path + (__i + 1);
                }
                return compile(l__p);
            }
        }
        return compile(x__path);
    };

    /* "script:package.sample" -> "script/package/sample.lua" */
    {
        std::string filePath = lpszSourceFile;
        {
            const char *p = strrchr(lpszSourceFile, '.');
            if ((p == NULL) || (strncasecmp(p, ".lua", 4) != 0))
            {
                std::replace(
                        std::replace(filePath, ":", "/"), ".", "/"
                );
                filePath.append(".lua");
            }
        }

        try {
            Json::Value v = this->config.get("%package", Json::nullValue);

            if (v.isArray())
            {
                for (int __x = 0; __x < (int)v.size(); ++__x)
                {
                    if (v[__x].isString())
                        try {
                            Json::Value _v = v[__x];
                            std::string _f = _v.isString() ? _v.asCString(): "";

                            long  depth = __x;
                            char *prefix;

                            if (strchr("#[", *(_f.c_str() + 0)) == NULL)
                                prefix = (char *)_f.c_str();
                            else {
                                depth = strtol(_f.c_str() + 1, &prefix, 10);
                                if ((depth == LONG_MIN) || (depth == LONG_MAX))
                                    throw __LINE__;
                                while (strchr(":]", *(prefix + 0)) != NULL)
                                    if (*(++prefix) == 0)
                                    {
                                        break;
                                    }
                            } s_it = addSource(L, depth, prefix, filePath.c_str());
                            goto _gS;
                        } catch (...) { }
                }
                return LUA_ERRFILE;
            }
            else s_it = addSource(L, 0, v.isString() ? v.asCString(): "", filePath.c_str());
        } catch (...) { return LUA_ERRFILE; }

_gS:    if (lpScript) (*lpScript) = *(*s_it->second);
	    _INFO("%d: SCRIPT '%s' - '%s'", getpid(), lpszSourceFile, s_it->second->getSourcePath())
    } return LUA_OK;
}

/* */
static LUA__SYS *ESYS = NULL;

struct SANDBOX_luaHelper: public luaObjectHelper {
	static const char *INDEX_ID;

	SANDBOX_luaHelper()
            : luaObjectHelper("SANDBOX_luaHelper") { INIT_RWLOCK(&rwlock); }
    virtual ~SANDBOX_luaHelper()
    {
	    {
		    luaObjectHelper::destroy(this->sandbox);
	    } luaObjectHelper::destroy(this->objects);
        DESTROY_RWLOCK(&rwlock);
    }

    virtual int __index(lua_State *L, void *u)
    {
        if (luaL_toindex(L, INDEX_ID, 0, 2) == 0) switch (lua_type(L, -1))
        {
            default:
            {
                luaObjectHelper::Bson *o = luaObjectHelper::fetch(
		                u ? this->sandbox: this->objects, luaI_tostring(L, -1, u).c_str(), &this->rwlock);
                if (o == NULL)
                    return 0;
                else {
                    o->fetch(L, INDEX_ID, false);
                } o->Unref();
            } break;

            case LUA_TNUMBER:
            {
                if (u != NULL)
                    return luaL_error(L, "%d: The access index is incorrectly specified.", __LINE__);
            } luaL_pushhelper(L, this, (void *)((intptr_t)lua_tointeger(L, -1)));
        } return 1;
    }

    virtual int __newindex(lua_State *L, void *u)
    {
	    luaObjectHelper::BsonSet *o_entry = u ? &this->sandbox: &this->objects;

        if (luaL_toindex(L, INDEX_ID, 0, 2) == 0) switch (lua_type(L, -1))
        {
            default:
            {
            	std::string k = luaI_tostring(L, -1, u);

                return (lua_type(L, 3) == LUA_TNIL)
                    ? luaObjectHelper::erase(*o_entry, k.c_str(), &this->rwlock)
                    : luaObjectHelper::assign(L, *o_entry, k.c_str(), 3, &this->rwlock);
            }

            case LUA_TNUMBER: switch (lua_type(L, 3))
            {
                case LUA_TNIL:
                {
                    int u__i = lua_tointeger(L, -1);

                    if (u__i <= 0)
                        return luaL_error(L, "%d: The access index is incorrectly specified.", __LINE__);
                    else {
                        std::string u__k = std::format("#%lu.", u__i);

                        ENTER_RWLOCK(&this->rwlock);
                        for (luaObjectHelper::BsonSet::iterator i_it = o_entry->begin(); i_it != o_entry->end(); )
                            if (i_it->first.find(u__k))
                                ++i_it;
                            else {
                                i_it->second->Unref();
                                i_it = o_entry->erase(i_it);
                            }
                    } LEAVE_RWLOCK(&this->rwlock);
                } return 0;
                default: break;
            } break;
        } return luaL_error(L, "%d: Object with limited variable settings.", __LINE__);
    }

    virtual int __call(lua_State *L, void *u)
    {
        int r = luaL_toindex(L, INDEX_ID, 0, 2);
        if (r < 0) return 0;
        else if (r == 0)
        {
            lua_replace(L, 2);
            return luaObjectHelper::update(L, u ? this->sandbox: this->objects,
            		INDEX_ID, luaI_tostring(L, 2, u).c_str(), 3, &this->rwlock);
        }
        return r;
    }

    virtual int __pairs(lua_State *L, void *u)
    {
        {
        	;
        } lua_pushcclosure(L, luaB_next, 0); /* will return generator, */
        lua_pushvalue(L, 1);
        lua_pushnil(L);
        return 3;
    }

    virtual int __tostring(lua_State *L, void *u)
    {
        lua_pushfstring(L, "sandbox: %p [%d]", this, (uintptr_t)u);
        return 1;
    }

    virtual void Dispose() { }

    /* */
	struct Proxy: public luaSandboxHelper {
		int fetch(lua_State *L, const char *k, luaObjectHelper::Bson *o)
		{
			std::string i = INDEX_ID;
			{
				guard::rdlock __rdlock(&sandbox_->rwlock);

				luaSandboxHelper::RuleSet::iterator r_it = sandbox_->rules.find(k);
				if (r_it != sandbox_->rules.end())
					i = r_it->second.c_str();
			}
			return o->fetch(L, i.c_str(), true);
		}

		/* */
		Proxy(SANDBOX_luaHelper *sandbox)
			: luaSandboxHelper("SANDBOX_luaHelper::State")
			, sandbox_(sandbox) { }
		virtual ~Proxy() { }

		virtual int __next(lua_State *L, int step, void *u)
		{
			switch (lua_type(L, -1))
			{
				case LUA_TNIL:
				case LUA_TSTRING:
				{
					std::string k;

					if (step > 1) k.assign(lua_tostring(L, -1));
					{
						luaObjectHelper::Bson *o =
								luaObjectHelper::advance(sandbox_->objects, (step > 1), &k, &sandbox_->rwlock);
						if (o == NULL)
							break;
						else {
							lua_pushstring(L, k.c_str());
							this->fetch(L, k.c_str(), o);
						}
						o->Unref();
					}
				} return 2;
				default: break;
			}
			return luaSandboxHelper::__next(L, step, u);
		}

		virtual int __index(lua_State *L, void *u)
		{
			if (luaL_toindex(L, INDEX_ID, 0, 2) == 0) switch (lua_type(L, -1))
			{
				case LUA_TSTRING:
				{
					const char *k = lua_tostring(L, -1);

					if (strcmp(k, "__") == 0)
						luaL_pushhelper(L, sandbox_, NULL);
					else {
						luaObjectHelper::Bson *o =
								luaObjectHelper::fetch(sandbox_->objects, k, &sandbox_->rwlock);
						if (o == NULL)
							goto _gR;
						else {
							this->fetch(L, k, o);
						}
						o->Unref();
					}
				} return 1;
_gR:            default: lua_replace(L, 2);
			}
			return luaSandboxHelper::__index(L, u);
		}

		/* luaSandboxHelper::__property() */
		virtual int __property(lua_State *L, const char *k, int v)
		{
			if ((k == NULL) || (*(k + 0) != '*')) return 0; /* clean -- ignore */
			{
				std::string x__k = k + 1;
				char       *x__p = (char *)strchr(x__k.c_str(), ':');

				auto assignRule = [&x__k, &x__p, this](RuleSet::iterator r_it,  int flags) {
					if (r_it != sandbox_->rules.end())
						r_it->second.flags = flags;
					else {
						r_it = sandbox_->rules.insert(
								luaSandboxHelper::RuleSet::value_type(x__k.c_str(), RULE(INDEX_ID, flags))
						).first;
					}

					if (x__p != NULL) r_it->second.assign(x__p + 1);
				};

				if (x__p != NULL) *(x__p + 0) = 0;
				{
					guard::rwlock __rwlock(&sandbox_->rwlock);
					RuleSet::iterator r_it = sandbox_->rules.find(x__k.c_str());

					if (v != 0) switch (lua_type(L, v))
						{
							case LUA_TNIL: if (r_it != sandbox_->rules.end())
							{
								sandbox_->rules.erase(r_it);
								luaObjectHelper::erase(sandbox_->objects, x__k.c_str());
							} break;

							default: assignRule(r_it, 0); /* read-only */
							{
								luaObjectHelper::assign(L, sandbox_->objects, x__k.c_str(), v);
							} break;
						}
					else assignRule(r_it, 1);
				}
			} return 1;
		}

		virtual int __newindex(lua_State *L, void *u)
		{
			if (luaL_toindex(L, INDEX_ID, 0, 2) == 0) switch (lua_type(L, -1))
				{
					case LUA_TSTRING:
					{
						const char *k = lua_tostring(L, -1);

						if (*(k + 0) != 0) switch (lua_type(L, 3))
						{
							case LUA_TNIL:
							{
								if (luaObjectHelper::erase(sandbox_->objects, k, &sandbox_->rwlock))
									return 1;
							} break;

							default:
							{
								guard::rwlock __rwlock(&sandbox_->rwlock);

								luaObjectHelper::Bson *o = luaObjectHelper::fetch(sandbox_->objects, k);
								if (o == NULL)
								{
									luaSandboxHelper::RuleSet::iterator r_it = sandbox_->rules.find(k);
									if (r_it != sandbox_->rules.end())
										switch (r_it->second.flags)
										{
											case 0: return luaL_error(L, "%d: read only", __LINE__); /* read-only */
											default: return luaObjectHelper::assign(L, sandbox_->objects, k, 3);
										}
									break;
								}
								else if (o->assign(L, 3)) o->Unref();
								else {
									o->Unref();
									return luaL_error(L, "%d: Includes non-serialable data [%d]", __LINE__, lua_type(L, 3));
								}
							} return 1;
						}
					}
					default: lua_replace(L, 2);
				}
			return luaSandboxHelper::__newindex(L, u);
		}

		virtual void Dispose() { }
	private:
		SANDBOX_luaHelper *sandbox_;
	};
protected:
	friend class lua__Stage;

	static std::string luaI_tostring(lua_State *L,  int idx, void *u)
	{
		const char *k = luaL_tolstring(L, idx, NULL);

		lua_pop(L, 1);
		return u ? std::format("#%lu.%s", (intptr_t)u, k): k;
	};

    static int luaB_next(lua_State *L)
    {
        void *u = NULL;
        SANDBOX_luaHelper *_this = (SANDBOX_luaHelper *)luaObjectHelper::unwrap(L, 1, &u);
	    std::string k = (lua_type(L, 2) == LUA_TNIL) ? "": luaI_tostring(L, 2, u);

        luaObjectHelper::Bson *o = luaObjectHelper::advance(u
                ? _this->sandbox: _this->objects, (k.length() > 0), &k, &_this->rwlock);
        if (o == NULL)
            lua_pushnil(L);
        else {
            {
                lua_pushfstring(L, u ? strchr(k.c_str(), '.') + 1: k.c_str());
                o->fetch(L, INDEX_ID, true);
            } o->Unref();
            return 2;
        } return 1;
    }

	luaSandboxHelper::RuleSet rules;
    luaObjectHelper::BsonSet  objects, sandbox; /* _G, _P */
	RWLOCK_T rwlock;
}; static SANDBOX_luaHelper SANDBOX__G;

const char *SANDBOX_luaHelper::INDEX_ID = "sandbox";

/* */
struct LUA__STAGE;

struct lua__Stage: public SANDBOX_luaHelper::Proxy
{
    lua_State *L;

    struct GC: public luaSandboxHelper::GC {
        GC(lua__Stage *context, LUA__STAGE *stage);
        virtual ~GC();
    };

    lua__Stage(SANDBOX_luaHelper *sandbox = &SANDBOX__G/*, Json::Value *odbc = NULL */);
    virtual ~lua__Stage() { lua_close(L); }

    void dofile(const char *f, std::function<void(lua_State *, const char *, int)> o);
    int  prepare(Json::Value v);
protected:
    /* */
    struct THREAD_luaHelper : public luaObjectHelper
    {
        THREAD_luaHelper()
                : luaObjectHelper("THREAD_luaHelper") { }

        /* */
        virtual int __index(lua_State *L, void *u);

        virtual int __tostring(lua_State *L, void *u) {
            lua_pushfstring(L, "thread: %p [%d]", this, pthread_self());
            return 1;
        }

        virtual int __gc(lua_State *L, void *u);
        virtual void Dispose() { }

    protected:
        static int __new(lua_State *L, THREAD_luaHelper *_this, void *u);

        static int __signal(lua_State *L, THREAD_luaHelper *_this, void *u);
        static int __waitfor(lua_State *L, THREAD_luaHelper *_this, void *u);
    } THREAD;
};


struct LUA__TLS: public lua__Stage {
    LUA__TLS();
    virtual ~LUA__TLS() { }
}; static ThreadLocalStorage<LUA__TLS> TLS_LUA;


/*
 * "group+message"
 *    group.lua
 *       ...
 *       return {
 *          ["message"] = function (args)
 *          end
 *       };
 *
 * "message"
 *
 * "group/message.lua"
 */
void lua__Stage::dofile(const char *f, std::function<void(lua_State *, const char *, int)> result)
{
    std::string m = f;
    /* luaSandboxHelper::Scope x__scope(&this->G, this->L); */

    char *p = (char *)strchr(m.c_str(), '+');
    if (p != NULL) *(p + 0) = 0;
    switch (ESYS->load(this->L, std::replace(m, ".", "/").c_str()))
    {
        default:
        {
            const char *e = lua_tostring(this->L, -1);
            _WARN("%s - '%s'", m.c_str(), (e == NULL) ? "<unknown>": e)
        } break;

        case LUA_OK:
        {
	        {
		        const char **argv;
		        int argc = stage_args(&argv);

		        lua_newtable(this->L);
		        for (int __i = 1; __i < argc; ++__i)
		        {
			        lua_pushstring(this->L, argv[__i]);
			        lua_rawseti(this->L, -2, __i);
		        }
	        } lua_setglobal(this->L, "arg");

        	luaObjectHelper::call(this->L, 0, 0, 1);
	        switch (lua_type(this->L, -1))
	        {
                case LUA_TTABLE: if (p == NULL) goto _gR;
                {
                    {
                        lua_pushstring(this->L, p + 1);
                        lua_rawget(this->L, -2);
                    } lua_remove(this->L, -2); /* remove table */
                } /* if (lua_isfunction(L, -1) == false) goto _gR;

                case LUA_TFUNCTION: luaObjectHelper::call(L, 0, v); */
_gR:            default: result(this->L, p ? (p + 1) : m.c_str(), lua_gettop(this->L));
            }
        } break;
    }
}

/* */
int lua__Stage::THREAD_luaHelper::__index(lua_State *L, void *u)
{
	std::string method = luaL_checkstring(L, -1);

	if (method == "id") lua_pushinteger(L, pthread_self());
	else if (method == "tick") lua_pushnumber(L, ((double)utimeNow(NULL)) / 1000.0);
	else if (method == "new") return link(L, (luaEventClosure_t)__new, u);
	else if (u == NULL)
	{
		lua_getglobal(L, "_THREAD");
		if ((u = lua_touserdata(L, -1)) == NULL) return luaL_error(L, "%d: Thread is not in use", __LINE__);
		else if (method == "signal") return link(L, (luaEventClosure_t)__signal, u);
	}
	else if (method == "waitfor") return link(L, (luaEventClosure_t)__waitfor, u);
	return 1;
}

/* */
struct LUA__STAGE: public luaObjectRef {

	struct Callback: public luaObjectRef,
	                 public luaObjectHelper::Callback {
		Callback(const char *args)
				: luaObjectRef("LUA__STAGE::Callback")
				, luaObjectHelper::Callback()
				, args(args) { }
		virtual ~Callback() { if (args != NULL) bson_free((void *)args); }

		virtual int call(lua_State *L, int nret = LUA_MULTRET)
		{
			int r;

			if ((r = this->load(L)) != LUA_OK) return r;
			else if (this->args == NULL)
				return luaObjectHelper::call(L, 0, 0, nret);
			else {
				bson_iterator b_it; bson_iterator_from_buffer(&b_it, this->args);
				return luaObjectHelper::call(L, 0, &b_it, nret);
			}
		}

		const char *args;
	protected:
		virtual void Dispose() { delete this; }
	}; typedef std::unordered_map<std::string, Callback *> CallbackSet;

	struct Timer: public Callback {
		utime_t timeUpdate;

		int     timeInterval;
		Timer(int interval, const char *args)
				: Callback(args)
				, timeUpdate(utimeNow(NULL) + std::abs(interval))
				, timeInterval(interval) { }
		virtual ~Timer() { }

		virtual Timer *Ref() { luaObjectRef::Ref(); return this; }
	protected:
		virtual void Dispose() { delete this; }
	};

	/* */
	CallbackSet waitfors;

	pq_pState   timers;
	RWLOCK_T    rwlock;

	std::string sign;
	utime_t     timeYield;

	LUA__STAGE(const char *stage_id);
	virtual ~LUA__STAGE()
	{
		for (CallbackSet::iterator it = waitfors.begin(); it != waitfors.end(); ++it)
			it->second->Unref();
		{
			pq_exit(timers, [](void *__x, void *__u) {
				((Timer *)__x)->Unref();
			}, this);
		} DESTROY_RWLOCK(&rwlock);
	}

	/* */
	static void apply(lua_State *L, LUA__STAGE *stage)
	{
		{
			(stage ? lua_pushlightuserdata(L, stage): lua_pushnil(L));
		} lua_setglobal(L, "_STAGE");
	}

	static LUA__STAGE *current(lua_State *L)
	{
		LUA__STAGE *stage = NULL;
		{
			lua_getglobal(L, "_STAGE");
			if (lua_type(L, -1) == LUA_TLIGHTUSERDATA)
				stage = (LUA__STAGE *)lua_touserdata(L, -1);
		} lua_pop(L, 1);
		return stage;
	}

	static LUA__STAGE *ref(lua_State *L)
	{
		LUA__STAGE *stage = LUA__STAGE::current(L);
		{
			if (stage != NULL)
				stage->Ref();
		} return stage;
	}

	static void unref(LUA__STAGE *stage) {
		if (stage != NULL) stage->Unref();
	}
protected:
	virtual void Dispose() { delete this; }

private:
	static int __lessThen(const void *l, const void *r, void *u) {
		int delta = ((Timer *)l)->timeUpdate - ((Timer *)r)->timeUpdate;
		return (delta == 0) ? (((intptr_t)l) < ((intptr_t)r)): (delta < 0);
	}
};

/* */
lua__Stage::GC::GC(lua__Stage *context, LUA__STAGE *stage)
	: luaSandboxHelper::GC(context, context->L) {
	LUA__STAGE::apply(this->L, stage);
}

lua__Stage::GC::~GC() { LUA__STAGE::apply(this->L, NULL); }

/* */
struct SUBMIT_luaCallback: public luaObjectRef
                         , public luaObjectHelper::Callback
{
    SUBMIT_luaCallback(lua_State *L, int base)
            : luaObjectRef("SUBMIT_luaCallback")
            , luaObjectHelper::Callback()
            , allowClone(0)
            , args(luaL_toargs(L, base, "v", &allowClone))
    {
    	if (!(stage = LUA__STAGE::ref(L)))
    		throw __LINE__;
    }

    virtual ~SUBMIT_luaCallback() {
	    {
		    luaL_freeBSON(NULL, args, allowClone);
	    } LUA__STAGE::unref(this->stage);
    }

    virtual int call(lua_State *L) {
        return luaObjectHelper::call(L, 0, luaObjectHelper::args(L, this->args));
    }

    int allowClone;
    const char *args;
    LUA__STAGE *stage;
protected:
    virtual void Dispose() { delete this; }
};

/* */
int lua__Stage::THREAD_luaHelper::__new(lua_State *L, THREAD_luaHelper *_this, void *u)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    {
        auto __main = [](utime_t timeClosure, void *__o) {
            SUBMIT_luaCallback *o = (SUBMIT_luaCallback *)__o;
            {
	            lua__Stage::GC g__scope(*TLS_LUA, o->stage);
                /* lua__Stage::Scope x__scope(*TLS_LUA, TLS_LUA->L); */
                int top = lua_gettop(TLS_LUA->L);

                {
                    lua_pushlightuserdata(TLS_LUA->L, o);
                } lua_setglobal(TLS_LUA->L, "_THREAD");
                if (o->load(TLS_LUA->L) == LUA_OK)
                    switch (o->call(TLS_LUA->L))
                    {
                        case LUA_OK:
                        {
	                        bson_scope b_i;

	                        {
		                        luaL_toargs(TLS_LUA->L, top + 1,
		                                    std::format("%s:%p", o->stage->sign.c_str(), o).c_str(), &b_i, NULL);
	                        } bson_finish(&b_i);
#if defined(LUA_DIRECT_CALL)
                            {
                                bson_iterator b_it; bson_iterator_init(&b_it, &b_i);
                                if (stage_call(&b_it) < 0)
                                    _WARN("%d: Internal error", __LINE__);
                            }
#else
	                        if (stage_signal(0, NULL, &b_i, 0, NULL) < 0)
		                        _WARN("%d: %s", __LINE__, strerror(errno));
#endif
                        } break;
                        default: break;
                    }
                else _WARN("%d - LOAD[%s]", __LINE__, lua_tostring(TLS_LUA->L, -1))

#define RESET_TIME          1000
                stage_addtask(utimeNow(NULL) + RESET_TIME, [](uint64_t timeBreak, bson_iterator *__it, struct sockaddr_stage *addr, void **udata) {
	                SUBMIT_luaCallback *o = *((SUBMIT_luaCallback **)udata);

                	if (addr == NULL)
		                o->Unref();
                	else {
		                LUA__STAGE *stage = o->stage;

		                std::string k = std::format("%p", o).c_str();
		                ENTER_RWLOCK(&stage->rwlock); {
			                LUA__STAGE::CallbackSet::iterator c_it = stage->waitfors.find(k.c_str());

			                if (c_it != stage->waitfors.end())
			                {
				                c_it->second->Unref();
				                stage->waitfors.erase(c_it);
			                }
		                } LEAVE_RWLOCK(&stage->rwlock);
	                } return -1;
                }, o);
#undef RESET_TIME
            }
            return 0;
        };

        SUBMIT_luaCallback *buffer = new SUBMIT_luaCallback(L, 2);

        buffer->dump(L, 1);
        if (stage_submit(0, __main, buffer->Ref()) >= 0)
            luaL_pushhelper(L, _this, buffer->Ref());
        else {
            buffer->Unref();
            return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
        }
    } return 1;
}

int lua__Stage::THREAD_luaHelper::__signal(lua_State *L, THREAD_luaHelper *_this, void *u)
{
    SUBMIT_luaCallback *o = (SUBMIT_luaCallback *)u;

    if (o == NULL) return luaL_error(L, "%d: Thread is not in use", __LINE__);
    else if (lua_gettop(L) > 0)
    {
        bson_scope b_i;

	    {
		    luaL_toargs(L, 1,
		    		std::format("%s:%p", o->stage->sign.c_str(), o).c_str(), &b_i, NULL);
	    } bson_finish(&b_i);
#if defined(LUA_DIRECT_CALL)
	    {
		    bson_iterator b_it; bson_iterator_init(&b_it, &b_i);
		    if (stage_call(&b_it) < 0)
		    	return luaL_error(L, "%d: Internal error", __LINE__);
	    }
#else
        if (stage_signal(0, NULL, &b_i, 0, NULL) < 0)
            return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
#endif
    } return 1;
}

int lua__Stage::THREAD_luaHelper::__gc(lua_State *L, void *u)
{
	if (u != NULL)
        ((SUBMIT_luaCallback *)u)->Unref();
    return this->luaObjectHelper::__gc(L, NULL);
}

/* */
static void x__setCallbackObject(lua_State *L, int idx, LUA__STAGE::Callback *c)
{
    bson b_arg; bson_init(&b_arg);

    if (luaL_toargs(L, idx, "", &b_arg, NULL) < 0)
        bson_destroy(&b_arg);
    else {
        bson_finish(&b_arg);
        if (c->args != NULL)
            bson_free((void *)c->args);

        c->args = bson_data(&b_arg);
    }
};

int lua__Stage::THREAD_luaHelper::__waitfor(lua_State *L, THREAD_luaHelper *_this, void *u)
{
    if (lua_type(L, 1) == LUA_TUSERDATA) lua_remove(L, 1);
    {
	    SUBMIT_luaCallback *o = (SUBMIT_luaCallback *)u;
	    LUA__STAGE *stage = o->stage;

        std::string k = std::format("%p", o).c_str();
        ENTER_RWLOCK(&stage->rwlock); {
            LUA__STAGE::CallbackSet::iterator c_it = stage->waitfors.find(k.c_str());

            switch (c_it == stage->waitfors.end())
            {
                case false: switch (lua_type(L, 1))
                {
                    case LUA_TNIL: c_it->second->Unref();
                    {
                        stage->waitfors.erase(c_it);
                    } break;
                    default: x__setCallbackObject(L, 1, c_it->second); break;

                    case LUA_TFUNCTION:
_gD:                {
                        {
                            if (lua_isnone(L, 2) == false)
                                x__setCallbackObject(L, 2, c_it->second);
                        } c_it->second->dump(L, 1);
                    } lua_pushstring(L, k.c_str()); /* 처리된 id */
                } break;

                default: switch (lua_type(L, 1))
                {
                    case LUA_TFUNCTION:
                    {
                        {
                            c_it = stage->waitfors.insert(
                                    LUA__STAGE::CallbackSet::value_type(k.c_str(), new LUA__STAGE::Callback(NULL))
                            ).first;
                        } c_it->second->Ref();
                    } goto _gD;
                    default: lua_pushnil(L);
                } break;
            }
        } LEAVE_RWLOCK(&stage->rwlock);
    } return 1;
}

/* */
static int __submit(lua_State *L)
{
	bool x__start = (lua_type(L, 1) == LUA_TNUMBER);
    uint64_t executor_id = x__start ? lua_tointeger(L, 1): 0;

    SUBMIT_luaCallback *buffer = new SUBMIT_luaCallback(L, 2 + x__start);
    switch (lua_type(L, 1 + x__start))
    {
        case LUA_TSTRING: buffer->assign(lua_tostring(L, 1 + x__start));
        {
            if (stage_submit(executor_id, [](utime_t timeClosure, void *__o) {
                int r = 0;
                {
	                SUBMIT_luaCallback *o = (SUBMIT_luaCallback *)__o;
	                lua__Stage::GC g__scope(*TLS_LUA, o->stage);

                    TLS_LUA->dofile(o->c_str(), [&](lua_State *L, const char *f, int nret) {
                        switch (lua_type(L, -1))
                        {
                            case LUA_TFUNCTION: switch (o->call(L))
                            {
                                case LUA_OK: switch (lua_type(L, -1))
                                {
                                    case LUA_TNUMBER: goto _gR;
                                    default: break;
                                } break;
                                default: break;
                            } break;

_gR:                        case LUA_TNUMBER: r = lua_tointeger(L, -1);
                            default: break;
                        }
                    });
                    if (r <= 0) o->Unref();
                }
                return r;
            }, buffer) >= 0) return 0;
        } goto _gE;

        case LUA_TFUNCTION: buffer->dump(L, 1 + x__start);
        {
            if (stage_submit(executor_id, [](utime_t timeClosure, void *__o) {
                int r = 0;

                SUBMIT_luaCallback *o = (SUBMIT_luaCallback *)__o;
                {
	                lua__Stage::GC g__scope(*TLS_LUA, o->stage);
                    /* lua__Stage::Scope x__scope(*TLS_LUA, TLS_LUA->L); */

                    if (o->load(TLS_LUA->L) == LUA_OK)
                        switch (o->call(TLS_LUA->L))
                        {
                            case LUA_OK: switch (lua_type(TLS_LUA->L, -1))
                            {
                                case LUA_TNUMBER: r = lua_tointeger(TLS_LUA->L, -1);
                                default: break;
                            } break;
                            default: break;
                        }
                    else _WARN("%d - LOAD[%s]", __LINE__, lua_tostring(TLS_LUA->L, -1))
                    if (r <= 0) o->Unref();
                }
                return r;
            }, buffer) >= 0) return 0;
        }

_gE:    {
            buffer->Unref();
        } return luaL_error(L, "%d: %s", __LINE__, strerror(errno));

        default: buffer->Unref();
    } return luaL_error(L, "%d: Unsupported data types [%d]", __LINE__, lua_type(L, 1 + x__start));
}

/* local r = stage.load("source+id"[, function (r) ... end]) */
static int __load(lua_State *L)
{
    std::string f = luaL_checkstring(L, 1);
    bool is__f = lua_isfunction(L, 2);
	int top = lua_gettop(L);

    /*
     * "group+message"
     *    group.lua
     *       ...
     *       return {
     *          ["message"] = function (args)
     *          end
     *       };
     *
     * "message"
     *
     * "group/message.lua"
     */
    struct lua__stage::LUA__SYS::Script si;
    char *p = (char *)strchr(f.c_str(), '+');
    if (p != NULL) *(p + 0) = 0;

    switch (ESYS->load(L, std::replace(f, ".", "/").c_str(), &si))
    {
    	default: return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
	    case LUA_ERRFILE: if (is__f)
	    {
		    lua_pushnil(L);
		    lua_newtable(L);
		    {
			    lua_pushstring(L, std::replace(f, ".", "/").c_str()); lua_rawseti(L, -2, 1);
			    if (p != NULL)
			    {
				    lua_pushstring(L, p + 1); lua_rawseti(L, -2, 2);
			    }
		    }
		    if (luaObjectHelper::call(L, 2, 2) != LUA_OK)
			    return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
	    } break;

    	case LUA_OK: luaObjectHelper::call(L, 0, 0, 1);
	    {
		    if ((p != NULL) && (lua_type(L, -1) == LUA_TTABLE))
		    {
			    {
				    lua_pushstring(L, p + 1);
				    lua_rawget(L, -2);
			    } lua_remove(L, -2); /* remove table */
		    }

		    if (is__f)
		    {
			    lua_newtable(L); {
				    lua_pushinteger(L, si.depth); lua_rawseti(L, -2, 1);
				    lua_pushstring(L, si.c_str()); lua_rawseti(L, -2, 2);
				    lua_pushinteger(L, si.mtime); lua_rawseti(L, -2, 3);
			    }
			    if (luaObjectHelper::call(L, 2, 2) != LUA_OK)
				    return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
		    }
	    } break;
    } return lua_gettop(L) - top;
}

static int __require(lua_State *L)
{
	std::string f = luaL_checkstring(L, 1);
	bool is__f = lua_isfunction(L, 2);
	int top = lua_gettop(L);

	/*
	 * "group+message"
	 *    group.lua
	 *       ...
	 *       return {
	 *          ["message"] = function (args)
	 *          end
	 *       };
	 *
	 * "message"
	 *
	 * "group/message.lua"
	 */
	struct lua__stage::LUA__SYS::Script si;
	char *p = (char *)strchr(f.c_str(), '+');
	if (p != NULL) *(p + 0) = 0;

	switch (luaObjectHelper::require(L, f.c_str(), [&si](lua_State *L, const char *__f) {
		std::string f = __f; return ESYS->load(L, std::replace(f, ".", "/").c_str(), &si);
	}))
	{
		default: return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
		case LUA_ERRFILE: if (is__f)
		{
			lua_pushnil(L);
			lua_newtable(L);
			{
				lua_pushstring(L, std::replace(f, ".", "/").c_str()); lua_rawseti(L, -2, 1);
				if (p != NULL)
				{
					lua_pushstring(L, p + 1); lua_rawseti(L, -2, 2);
				}
			}
			if (luaObjectHelper::call(L, 2, 2) != LUA_OK)
				return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
		} break;
		case LUA_OK:
		{
			if ((p != NULL) && (lua_type(L, -1) == LUA_TTABLE))
			{
				{
					lua_pushstring(L, p + 1);
					lua_rawget(L, -2);
				} lua_remove(L, -2); /* remove table */
			}

			if (is__f)
			{
				if (si.length() == 0)
					lua_pushnil(L);
				else {
					lua_newtable(L);
					{
						lua_pushinteger(L, si.depth); lua_rawseti(L, -2, 1);
						lua_pushstring(L, si.c_str()); lua_rawseti(L, -2, 2);
						lua_pushinteger(L, si.mtime); lua_rawseti(L, -2, 3);
					}
				}
		        if (luaObjectHelper::call(L, 2, 2) != LUA_OK)
					return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
			}
		} break;
	} return lua_gettop(L) - top;
}


/* stage.addtask(timeout, function (args...)
 * end, args...)
 */
static int __addtask(lua_State *L)
{
    LUA__STAGE *stage = LUA__STAGE::current(L);

#define TASK_SIGN               "task: "
#define SIGN_LEN                (sizeof(TASK_SIGN) - 1)
    size_t timeOut = 0;
    switch (lua_type(L, 1))
    {
        case LUA_TSTRING:
        {
            const char *v = lua_tolstring(L, 1, &timeOut);

            if (strncmp(v, TASK_SIGN, SIGN_LEN) != 0) return luaL_error(L, "%d: Not a task object", __LINE__);

            uintptr_t x__task;
            {
                std::stringset x__token;

                /* "tick: 0x...[XXX,XXX]" */
                if (std::tokenize(v + SIGN_LEN, x__token, "[,]") != 3)
                    return luaL_error(L, "%d: Format error", __LINE__);
                else {
                    x__task = strtoul((x__token[0] + x__token[1]).c_str(), NULL, 16);
                    if (x__task != ((uintptr_t)stage))
                        return luaL_error(L, "%d: Unable to task", __LINE__);
                } x__task = strtoul((x__token[0] + x__token[2]).c_str(), NULL, 16);
            } lua_remove(L, 1);

            ENTER_RWLOCK(&stage->rwlock); {
                bool x__r = false;
                const char *r__task;

                for (int __x = 0; (r__task = (char *)pq_each(stage->timers, ++__x)) != NULL; )
                    if (x__task == (uintptr_t)r__task)
                    {
                        LUA__STAGE::Timer *task = (LUA__STAGE::Timer *)r__task;

                        switch (lua_type(L, 1))
                        {
                            case LUA_TNIL: task->Unref();
                            {
                                pq_remove(stage->timers, __x);
                                x__r = true;
                            } break;

                            case LUA_TNUMBER: pq_remove(stage->timers, __x);
                            {
                                x__r = true; {
                                    if ((task->timeInterval = lua_tointeger(L, 1)) > 0)
                                        task->timeUpdate = utimeNow(NULL) + task->timeInterval;
                                    lua_remove(L, 1);
                                } pq_push(stage->timers, (void *)task);
                            } if (lua_isfunction(L, 1) == false) break;

                            case LUA_TFUNCTION: task->dump(L, 1);
                            {
                                ;
                            } x__r = true;
                            default: break;
                        }
                        break;
                    }
                lua_pushboolean(L, x__r);
            } LEAVE_RWLOCK(&stage->rwlock);
        } return 1;

        case LUA_TNUMBER: timeOut = lua_tointeger(L, 1);
        {
            lua_remove(L, 1);
        } luaL_checktype(L, 1, LUA_TFUNCTION);
        case LUA_TFUNCTION:
        {
            LUA__STAGE::Timer *task = new LUA__STAGE::Timer(timeOut, luaL_toargs(L, 2, "v", NULL));

            task->dump(L, 1);
            {
                std::string f__s = std::format("%p", stage).c_str();
                std::string f__t = std::format("%p", task).c_str();

                std::string x__f = TASK_SIGN;
                {
                    const char *p__s = f__s.c_str(), *p__t = f__t.c_str();
                    int __x = 0;

                    for (; *(p__s + __x) && *(p__t + __x); ++__x)
                    {
                        if (*(p__s + __x) != *(p__t + __x))
                            break;
                    }
                    x__f.append(p__s, __x);
                    x__f.append(std::format("[%s,%s]", p__s + __x, p__t + __x).c_str());
                }
                lua_pushlstring(L, x__f.data(), x__f.length());
            }

            ENTER_RWLOCK(&stage->rwlock); {
                task = (LUA__STAGE::Timer *)pq_push(stage->timers, task->Ref());
            } LEAVE_RWLOCK(&stage->rwlock);
            if (task != NULL) task->Unref();    /* overflow */
        } return 1;
    }

    return 0;
}

/*
 * stage.signal({"callback_id", TARGET_ADDR, TIME_BREAK}, values, ...)
 * stage.signal({"$return", RETURN_ADDR }, value...) -- return
 * stage.signal(nil, value...) -- return
 */
static int __signal(lua_State *L)
{
    LUA__STAGE *stage = LUA__STAGE::current(L);

    std::string           x__uid;
    struct sockaddr_stage x__addr, *addr = NULL;
    utime_t               x__timeBreak = 0;

    bool i__force = false; bson_scope b_i;

    auto x__parseCallbackID = [&L, &x__uid, &b_i, &i__force, &stage](const char *k, int v_idx)
    {
        std::string i__k;
        const char *p = strchr(k, ':'); /* "stage_id:event_id" */

        /* stage_signal(1, ...) 로 처리되기 떄문에 다시 feedback될 stage_id를 추가한다. */
        if (stage->sign.length() > 0) bson_append_string(&b_i, "", stage->sign.c_str());
        if (p != NULL) switch (*(p + 1))
        {
            case 0: x__uid.append(k, p - k); /* stage_id만 가지고 있는 경우 */
            {
                /* while (strchr("*=+", *(k + 0))) ++k; */

                if (stage->sign.empty() == false)
                {
                    if (x__uid == stage->sign) /* stage_id가 sign가 동일하면 제거 */
                        x__uid.resize(0);
                    x__uid.append(":" + stage->sign);
                }

_gB:            for (lua_pushnil(L); lua_next(L, v_idx); lua_pop(L, 1))
                    switch (lua_type(L, -2))
                    {
                        case LUA_TSTRING: luaL_toBSON(L, -1, lua_tostring(L, -2), &b_i, NULL);
                        default: break;
                    }
            } return true;

            case '>':
            case '<': /* "stage_id:>event_id" : 우선권 처리를 다른 stage로 전달 */
            {
                std::string stage_id(k, p - k);
                const char *i__p = strchr(++p, '.');

                i__k.append(p, i__p ? (i__p - p): 1);
                i__k.append("." + stage_id);
                if ((*(k + 1) == '>') || (*(k + 0) == '<'))
                    i__force = true;
                else {
                    x__uid = i__k;
                    i__k = (stage_id + ":" + ((i__p ? i__p : k) + 1)).c_str();
                }
            } break;

            default: x__uid.append(k, p - k); /* "xxx:callback_id" */
            {
                while (strchr("*=+", *(k + 0))) ++k;

                if (x__uid.empty() || (*(k + 0) == ':')) /* ":callback_id" OR "*:callback" */
                    i__k = (stage->sign + k).c_str(); /* "STAGE:callback_id" */
                else if (stage->sign.empty() == false)
                {
                    if (x__uid == stage->sign) /* stage_id가 sign가 동일하면 제거 */
                        x__uid.resize(0);
                    x__uid.append(":" + stage->sign);
                }
            } break;
        }
        else if (stage->sign.length() > 0) switch (*(k + 0))
        {
            default: /* target_stage+return_id */
            {
                const char *i__p = strchr(k, '+');

                if (i__p == NULL)
                    /* x__uid.append(stage->sign.c_str()) */;
                else {
                    x__uid.append(k, i__p - k); {
                        /* "*target_stage+return_id" */
                        const char *x = x__uid.c_str();

                        while (strchr("*=+", *(x + 0))) ++x;
                        i__k.append(x).append(":");
                    } k = i__p + 1;
                }

                for (i__p = k; strchr("*=+", *(k + 0)); ) ++k;
                x__uid.insert(0, i__p, k - i__p); /* "*=+" 를 복사한다. */
                /* i__k
                 *    "target_stage:return_stage:return_id"
                 *
                 *    명령을 받은 stage에서
                 *       "target_stage"의 sign값을 확인하고 실행
                 *    이후 결과는 "return_stage:return_id" 로 전달
                 *
                 * x__uid
                 *    CLUSTER__MAIN에서 데이터를 전달할 stage_id
                 */
                x__uid.append(":" + stage->sign); /* return stage_id 설정 */
                i__k += (stage->sign + ":") + k;
            } break;

            case '@': /* self feedback */
            {
                x__uid.append(stage->sign + ":").append(stage->sign);
                i__k.append(stage->sign + ":").append(k + 1);
            } break;

            case '>':
            case '<': /* ">callback_id": 우선권 처리 */
            {
                const char *i__p = strchr(k, '.');

                i__k.append(k, i__p ? (i__p - k): 1);
                i__k.append("." + stage->sign);

                if ((*(k + 1) == '>') || (*(k + 0) == '<'))
                    i__force = true;
                else {
                    x__uid = i__k;
                    i__k = (stage->sign + ":" + ((i__p ? i__p: k) + 1)).c_str();
                }
            } break;

            /* "" 로 설정되는 경우 리턴을 받을 stage_id를 설정하지 않는다. */
            case 0: x__uid.append(stage->sign); goto _gB;
        }

	    luaL_toargs(L, v_idx, i__k.empty() ? k: i__k.c_str(), &b_i, NULL);
        return true;
    };

    /* */
    auto x__updateTarget = [&L, &x__timeBreak, &x__addr, &addr](int __idx) {
        switch (lua_type(L, __idx))
        {
            case LUA_TNUMBER:
            {
                int64_t x = lua_tointeger(L, __idx);
                if (x > 0) x__timeBreak = utimeNow(NULL) + x;
            } break;

            case LUA_TSTRING:
            {
                size_t length;
                const char *v = lua_tolstring(L, __idx, &length);
                switch (((struct sockaddr *)v)->sa_family)
                {
                    case AF_INET:
                    case AF_INET6:
                    case AF_UNIX:
                    {
                        memcpy(addr = &x__addr, v, x__addr.ss_length = length);
                    } break;
                }
            } break;

            default: return false;
        }
        return true;
    };

    switch (lua_type(L, 1))
    {
        default: return luaL_error(L, "%d: Invalid argument", __LINE__);

        case LUA_TNIL: lua_getglobal(L, "_WAITFOR"); switch (lua_type(L, -1))
        {
            case LUA_TTABLE: lua_replace(L, 1);
_gI:        {
                std::string k;

                for (lua_pushnil(L); lua_next(L, 1); lua_pop(L, 1))
                    if (lua_type(L, -2) != LUA_TNUMBER);
                    else switch (lua_tointeger(L, -2))
                    {
                        case 0: switch (lua_type(L, -1))
                        {
                            case LUA_TSTRING:
                            {
                                const char *p = lua_tostring(L, -1); /* CALLBACK_ID=RETURN_ID */
                                const char *x = strchr(p, '=');

                                k.append(x ? x + 1: p);
                            } break;
                            default: break;
                        } break;

                        case 1: x__updateTarget(-1); break;
                        case 2: switch (lua_type(L, -1))
                        {
                            case LUA_TSTRING: k.insert(0, std::format("%s:", lua_tostring(L, -1)).c_str());
                            default: break;
                        } break;
                        default: break;
                    }

                if (k.empty())
                    return 0;
				luaL_toargs(L, 2, k.c_str(), &b_i, NULL);
            } break;
            default: return luaL_error(L, "%d: stage.waitfor() not a callback function.", __LINE__);
        } break;
#if defined(LUAJIT_VERSION)
        case LUA_TTABLE: lua_rawgeti(L, 1, 0); switch (lua_type(L, -1))
#else
        case LUA_TTABLE: switch (lua_rawgeti(L, 1, 0))
#endif
        {
            case LUA_TSTRING: lua_pop(L, 1); goto _gI;
            case LUA_TNIL:
            {
                std::string k;

                for (; lua_next(L, 1); lua_pop(L, 1))
                    if (lua_type(L, -2) == LUA_TNUMBER) switch (lua_tointeger(L, -2)) /* { CALLBACK_ID, ADDR, TIMEOUT } */
                    {
                        case 1: switch (lua_type(L, -1))
                        {
                            case LUA_TSTRING: k = lua_tostring(L, -1);
                            default: break;
                        } break;

                        case 2:
                        case 3: x__updateTarget(-1);
                        default: break;
                    }

                if (k.empty())
                    return luaL_error(L, "%d: Format error", __LINE__);
                x__parseCallbackID(k.c_str(), 2);
            } break;
            default: break;
        } break;

        case LUA_TSTRING: x__parseCallbackID(lua_tostring(L, 1), 2);
    }

    bson_finish(&b_i);
    if (x__uid.length() == 0)
    {
        if (i__force) stage_signal(-1, NULL, &b_i, x__timeBreak, addr);
        else if (addr == NULL)
        {
            bson_iterator b_it; bson_iterator_init(&b_it, &b_i);
            if (stage_call(&b_it) < 0) return luaL_error(L, "%d: Internal error", __LINE__);
        }
        else if (stage_signal(0x80, NULL, &b_i, x__timeBreak, addr) < 0)
            return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
    }
    else if (stage_signal(1, x__uid.c_str(), &b_i, x__timeBreak, addr) < 0)
        return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
    return 0;
}

/* stage.waitfor([id,]function (arg...)
 * end)
 *
 * stage.waitfor([length,]function (arg...)
 * end)
 *
 * stage.waitfor(id, nil);
 */
static int __waitfor(lua_State *L)
{
    LUA__STAGE *stage = LUA__STAGE::current(L);

    LUA__STAGE::CallbackSet::iterator c_it;
#define MAX_RANDOM              16
    int         l__f = MAX_RANDOM;
    std::string s__f;

    ENTER_RWLOCK(&stage->rwlock); switch (lua_type(L, 1))
    {
        case LUA_TNONE: lua_newtable(L);
        {
            for (c_it = stage->waitfors.begin(); c_it != stage->waitfors.end(); ++c_it)
	        {
                LUA__STAGE::Callback *c = c_it->second;

		        {
			        lua_pushstring(L, c_it->first.c_str());
			        if (c->args == NULL) lua_pushnil(L);
			        else luaL_pushBSON(L, 0, c->args);
		        } lua_rawset(L, -3);
	        }
        } break;

#define MIN_RANDOM              4
        case LUA_TNUMBER:
        {
            l__f = std::max(((int)lua_tointeger(L, 1)), MIN_RANDOM);
        } lua_remove(L, 1);
#undef MIN_RANDOM
        case LUA_TFUNCTION:
        {
            do {
                s__f = std::random(l__f);
            } while (stage->waitfors.find(s__f.c_str()) != stage->waitfors.end());
_gI:        {
                c_it = stage->waitfors.insert(
                        LUA__STAGE::CallbackSet::value_type(s__f.c_str(), new LUA__STAGE::Callback(NULL))
                ).first;
            } c_it->second->Ref();
            _INFO("REGISTER: WAITFOR - '%s'", c_it->first.c_str());
        } goto _gD;

        case LUA_TSTRING:
        {
            c_it = stage->waitfors.find((s__f = lua_tostring(L, 1)).c_str());
        } lua_remove(L, 1);

        switch (c_it == stage->waitfors.end())
        {
            case false: switch (lua_type(L, 1))
            {
                case LUA_TNONE:
                {
                    int top = lua_gettop(L);

	                lua__stage::LUA__STAGE::Callback *c = c_it->second;
	                if (c->load(L) != LUA_OK)
                        LEAVE_RWLOCK(&stage->rwlock);
	                else {
	                    if (c->args != NULL)
                        {
                            bson_iterator b_it; bson_iterator_from_buffer(&b_it, c->args);

                            if (bson_iterator_type(&b_it) != BSON_ARRAY)
                                luaL_pushBSON(L, &b_it);
                            else {
                                bson_iterator a_it;

                                lua_newtable(L);
                                for (bson_iterator_subiterator(&b_it, &a_it); bson_iterator_next(&a_it) != BSON_EOO;)
                                {
                                    const char *k = bson_iterator_key(&a_it);

                                    luaL_pushBSON(L, &a_it);
                                    lua_rawseti(L, -2, strtol(k + (*(k + 0) == '#'), NULL, 10));
                                }
                            }
                        } LEAVE_RWLOCK(&stage->rwlock);
	                    return lua_gettop(L) - top;
	                }
                } return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));

                case LUA_TNIL: c_it->second->Unref();
                {
                    _INFO("UNREGISTER: WAITFOR - '%s'", c_it->first.c_str());
                    stage->waitfors.erase(c_it);
                } break;
                default: x__setCallbackObject(L, 1, c_it->second); break;

                case LUA_TFUNCTION: _INFO("UPDATE: WAITFOR - '%s'", c_it->first.c_str());
_gD:            {
                    {
                        if (lua_isnone(L, 2) == false)
                            x__setCallbackObject(L, 2, c_it->second);
                    } c_it->second->dump(L, 1);
                } lua_pushstring(L, s__f.c_str()); /* 처리된 id */
            } break;

            default: switch (lua_type(L, 1))
            {
                case LUA_TFUNCTION: goto _gI;
                default: goto _gE;
            } break;
        } break;

_gE:    default: lua_pushnil(L);
    } LEAVE_RWLOCK(&stage->rwlock);
    return 1;
}

/* stage.call({ function (), arg... }, arg...) */
static int __call(lua_State *L)
{
	int top = lua_gettop(L);

	luaSandboxHelper::Scope O(NULL, L, -2);
	switch (lua_type(L, 1))
	{
		case LUA_TFUNCTION: lua_pushvalue(L, 1);
		{
			lua_xmove(L, O.L, 1);
_gI:		for (int x__i = 2; x__i <= top; ++x__i) switch (lua_type(L, x__i))
            {
                case LUA_TTABLE:
                {
                    for (lua_pushnil(L); lua_next(L, x__i); ) lua_xmove(L, O.L, 1);
                } break;

                default:
                {
                    lua_pushvalue(L, x__i);
                } lua_xmove(L, O.L, 1);
            }

			if (luaObjectHelper::call(O.L, 0, lua_gettop(O.L) - 1) == LUA_OK)
				;
			else {
				std::string r = lua_tostring(O.L, -1);

				lua_settop(L, top);
				return luaL_error(L, "%d: %s", __LINE__, r.c_str());
			}

			if (lua_gettop(O.L) > 0)
				lua_xmove(O.L, L, lua_gettop(O.L));
		} return lua_gettop(L) - top;

		case LUA_TTABLE:
		{
			for (lua_pushnil(L); lua_next(L, 1); ) lua_xmove(L, O.L, 1);
		} goto _gI;

		case LUA_TSTRING:
		{
			std::string f = lua_tostring(L, 1);

			struct lua__stage::LUA__SYS::Script si;
			char *p = (char *)strchr(f.c_str(), '+');
			if (p != NULL) *(p + 0) = 0;

			if (ESYS->load(O.L, std::replace(f, ".", "/").c_str(), &si) == LUA_OK)
				luaObjectHelper::call(O.L, 0, 0, 1);
			else {
				std::string r = lua_tostring(O.L, -1);

				lua_settop(L, top);
				return luaL_error(L, "%d: %s", __LINE__, r.c_str());
			}

			if ((p != NULL) && (lua_type(O.L, -1) == LUA_TTABLE))
			{
				{
					lua_pushstring(O.L, p + 1);
					lua_rawget(O.L, -2);
				} lua_remove(O.L, -2); /* remove table */
			}
		} goto _gI;
		default: lua_settop(L, top);
	}
	return 0;
}

/* stage.exit() */
static int __exit(lua_State *L) { return kill(0, SIGQUIT); }

static int __once(lua_State *L)
{
    switch (lua_type(L, -1))
    {
        case LUA_TNIL: if (lua_isstring(L, 1))
        {
            const char *k = lua_tostring(L, 1);

            ENTER_RWLOCK(&ESYS->rwlock); {
                lua__stage::LUA__SYS::OnceSet::iterator o_it = ESYS->once.find(k);
                if (o_it != ESYS->once.end())
                    ESYS->once.erase(o_it);
            } LEAVE_RWLOCK(&ESYS->rwlock);
        } break;

        case LUA_TFUNCTION:
        {
            const char *k =
                    (lua_type(L, 1) == LUA_TSTRING) ? lua_tostring(L, 1): "";

            guard::rwlock __rwlock(&ESYS->rwlock);
            lua__stage::LUA__SYS::OnceSet::iterator o_it = ESYS->once.find(k);
            if (o_it != ESYS->once.end())
                return 0;
            else {
                ESYS->once.insert(
                        lua__stage::LUA__SYS::OnceSet::value_type(k, utimeNow(NULL))
                );
            }
        } luaObjectHelper::call(L, -1, 0, 0);

        default: break;
    }
    return 0;
}

/* */
static int __luaTraceback(lua_State *L)
{
    LOG4CXX_WARN(logger, lua_tostring(L, -1));
    return 0;
}

/* */
static pthread_once_t LUA__initOnce = PTHREAD_ONCE_INIT;

static void LUA__atExit() { ESYS->Unref(); }
static void LUA__atInit()
{
    try {
	    (ESYS = new LUA__SYS())->Ref();
        stage_setapi("lua", [](uintptr_t arg, uintptr_t *__x) {
	        {
		        LUA__STAGE::apply(TLS_LUA->L, arg ? (LUA__STAGE *)((luaObjectRef *)arg): NULL);
	        } lua_settop(TLS_LUA->L, 0);
            return (void *)TLS_LUA->L;
        }, 0);

#define NAMESPACE_ODBC              "%odbc"
        stage_setapi(NAMESPACE_ODBC, [](uintptr_t arg, uintptr_t *__x) {
            return lua__stage::ESYS->config.isMember(NAMESPACE_ODBC)
                ? (void *)&lua__stage::ESYS->config[NAMESPACE_ODBC]: NULL;
        }, 0);
#undef NAMESPACE_ODBC

    } catch (...) { }
    atexit(LUA__atExit);
}

/* */
static int messageCallback(utime_t timeBreak, bson_iterator *bson_it, struct sockaddr_stage *addr, LUA__STAGE **__stage)
{
    if (addr == NULL) (*__stage)->Unref();
    else if (utimeNow(NULL) < timeBreak)
    {
        auto stage_callback = [](LUA__STAGE *_this, const char *f, bson_iterator *v,
                struct sockaddr_stage *addr, std::function<int(lua_State *L, const char *f, int nret)> r_callback)
        {
            auto luaL_pushargs = [](lua_State *L, bson_iterator *v)
            {
                if ((v == NULL) || (v->cur == NULL));
                else if (bson_iterator_type(v) != BSON_ARRAY) luaL_pushBSON(L, v);
                else {
                    bson_iterator a_it;
                    for (bson_iterator_subiterator(v, &a_it); bson_iterator_next(&a_it) != BSON_EOO;)
                    {
                        char *k = (char *)bson_iterator_key(&a_it);

                        luaL_pushBSON(L, &a_it);
                        switch (*(k + 0))
                        {
                            case '.': lua_setglobal(L, k + 1);
                            case '#': break;
                            default: strtol(k, &k, 10);
                            {
                                if (*(k + 0) == 0) break;
                            } lua_pop(L, 1);
                        }
                    }
                }
            };

            /* lua__Stage::Scope x__scope(*TLS_LUA, TLS_LUA->L); */
            int         x__arg = 0;
            std::string x__f = f;

            ENTER_RDLOCK(&_this->rwlock); {
                int x__i, x__e = 0;

                {
                    char *x__p = (char *)strchr(x__f.c_str(), '='); /* callback_id 제거 */
                    if (x__p == NULL)
                        x__i = x__f.length();
                    else {
                        *(x__p + 0) = 0;
                        x__i = x__p - x__f.c_str();
                    }

                    /* "stage:callback_id" vs "callback_id?argument:argument" */
                    {
                        char *p = (char *)strchr(x__f.c_str(), '?');
                        if ((x__p = (char *)strchr(x__f.c_str(), ':')) == NULL);
                        else if (x__p > p) x__e = x__p - x__f.c_str();
                    }
                }

_gW:            lua__stage::LUA__STAGE::CallbackSet::iterator c_it = _this->waitfors.find(x__f.c_str());
                if (c_it == _this->waitfors.end())
                {
                    if (x__i > 0)
                    {
                        /* stage.signal("stage+callback", ..)으로 요청이 오는 경우 stage가 포함되어 있어
                         *    "stage:callback.a" -> "stage:callback." -> "stage:" 순서로 검색
                         */
                        for (char *x__p = (char *)x__f.data(); (--x__i) >= x__e; )
                            switch (*(x__p + x__i))
                            {
                                case '?':
                                case ':':
                                case '.': *(x__p + (x__i + 1)) = 0; goto _gW;
                                default: break;
                            }

                        x__i = 0; x__f = "@"; goto _gW;
                    }
                    LEAVE_RDLOCK(&_this->rwlock);
                    return 0;
                }

                lua__stage::LUA__STAGE::Callback *c = c_it->second;
                switch (c->load(TLS_LUA->L))
                {
                    default: LEAVE_RDLOCK(&_this->rwlock);
                    {
                        _WARN("%s: %d - LOAD[%s]", f, __LINE__, lua_tostring(TLS_LUA->L, -1))
                        ENTER_RWLOCK(&_this->rwlock); {
                            c->Unref();
                        } _this->waitfors.erase(x__f.c_str());
                        LEAVE_RWLOCK(&_this->rwlock);
                    } return 1;
                    case LUA_OK: if (c->args != NULL)
                    {
                        int top = lua_gettop(TLS_LUA->L); {
                            bson_iterator bson_it;

                            bson_iterator_from_buffer(&bson_it, c->args);
                            luaL_pushargs(TLS_LUA->L, &bson_it);
                        } x__arg = lua_gettop(TLS_LUA->L) - top;
                    } break;
                }
            } LEAVE_RDLOCK(&_this->rwlock);

            /* _WAITFOR[메시지ID, 요청서버, stage_id] */
            lua_newtable(TLS_LUA->L); {
                {
                    lua_pushstring(TLS_LUA->L, f);
                } lua_rawseti(TLS_LUA->L, -2, 0);
                if (addr == NULL);
                else if (addr == (struct sockaddr_stage *)-1);
                else {
                    {
                        lua_pushlstring(TLS_LUA->L, (const char *)addr, addr->ss_length);
                    } lua_rawseti(TLS_LUA->L, -2, 1);

                    if (*(addr->ss_stage + 0) != 0)
                    {
                        {
                            lua_pushstring(TLS_LUA->L, addr->ss_stage);
                        } lua_rawseti(TLS_LUA->L, -2, 2);
                    }
                }
                {
                    lua_pushstring(TLS_LUA->L, x__f.c_str());
                } lua_rawseti(TLS_LUA->L, -2, -1);
            } lua_setglobal(TLS_LUA->L, "_WAITFOR");

            if (luaObjectHelper::call(TLS_LUA->L, 0, [&](lua_State *L) {
                /* if (x__arg > 0)
                    lua_xmove(TLS_LUA->L, L, x__arg); */
                luaL_pushargs(L, v);
                return x__arg + 1;
            }) != LUA_OK)
                _WARN("%s: %d - CALL[%s]", f, __LINE__, lua_tostring(TLS_LUA->L, -1))
            else {
                if (r_callback(TLS_LUA->L, f, lua_gettop(TLS_LUA->L)) >= 0)
                    return 0;
                _WARN("%s: %d - unknown result type", f, __LINE__);
            }
            return 1;
        };

	    lua__Stage::GC g__scope(*TLS_LUA, (*__stage));
	    if (bson_it != NULL)
        {
            const char *__i = bson_iterator_key(bson_it);

            if ((*__stage)->sign.length() == 0);
            else if (strncmp(__i, (*__stage)->sign.c_str(), (*__stage)->sign.length()) != 0) return 0; /* NOT */
            else if (*(__i += (*__stage)->sign.length()) != ':') return 0;
            else ++__i;

            return stage_callback(*__stage, __i, bson_it, addr, [&](lua_State *L, const char *f, int nret) {
                if (nret == 0);
                else if (addr == NULL);
                else if (addr == (struct sockaddr_stage *)-1);
                else if (*(addr->ss_stage + 0) != 0)
                {
                    bson_scope b_i;

                    const char *p = strchr(f, '=');
                    std::string i = std::format("%s:%s", addr->ss_stage, p ? p + 1: f).c_str();
                    if (nret == 1)
                        luaL_toBSON(L, 1, i.c_str(), &b_i, NULL);
                    else {
                        bson_append_start_array(&b_i, i.c_str());
                        for (int x__i = 0; x__i < nret; ++x__i)
                        {
                            luaL_toBSON(L, 1 + x__i, std::format("%0d", x__i + 1).c_str(), &b_i, NULL);
                        } bson_append_finish_array(&b_i);
                    } bson_finish(&b_i);
                    if (stage_signal(0, NULL, &b_i, 0, addr) < 0)
                        _WARN("feedback: '%s' - %s", f, strerror(errno));
                }
                return 0;
            });
        }

        /* */
        {
            utime_t timeNow = utimeNow(NULL);
            std::list<LUA__STAGE::Timer *> ready;

            do {
                ENTER_RWLOCK(&(*__stage)->rwlock); {
                    LUA__STAGE::Timer *task = (LUA__STAGE::Timer *)pq_top((*__stage)->timers);

                    if ((task != NULL) && (task->timeUpdate <= timeNow))
                        pq_pop((*__stage)->timers);
                    else {
                        LEAVE_RWLOCK(&(*__stage)->rwlock);
                        break;
                    }

                    LEAVE_RWLOCK(&(*__stage)->rwlock);
                    if (task->args == NULL)
                        ;
                    else {
                        /* lua__Stage::Scope x__scope(*TLS_LUA, TLS_LUA->L); */
                        int r = task->call(TLS_LUA->L);

                        timeNow = utimeNow(NULL);
                        if (r != LUA_OK)
                            _WARN("TASK - REJECT: task->call")
                        else {
                            task->timeInterval = luaL_optinteger(TLS_LUA->L, 1, task->timeInterval);
                            if (task->timeInterval > 0)
                            {
                                task->timeUpdate = timeNow + task->timeInterval;
                                ready.push_back(task);
                                goto _gT;
                            }
                        }
                    } task->Unref();
                }
_gT:            ;
            } while (timeNow < (*__stage)->timeYield);

            if (ready.empty() == false)
            {
                ENTER_RWLOCK(&(*__stage)->rwlock);
                do {
                    pq_push((*__stage)->timers, ready.front());
                    ready.pop_front();
                } while (ready.empty() == false);
                LEAVE_RWLOCK(&(*__stage)->rwlock);
            }

            if (timeNow >= (*__stage)->timeYield)
                stage_callback(*__stage, "", NULL, addr, [&](lua_State *L, const char *f, int nret) {
                    if (nret > 0) switch (lua_type(L, -1))
                    {
                        case LUA_TNUMBER:
                        {
                            (*__stage)->timeYield = timeNow + lua_tointeger(L, -1);
                        } break;
                        default: break;
                    }
                    return 0;
                });
        }
    }
    return 1;
}


/* */
static const luaL_Reg stagelib[] = {
		{ "load"   , __load    },
		{ "require", __require },

		{ "submit" , __submit  },
		{ "addtask", __addtask },

		{ "signal" , __signal  },
		{ "waitfor", __waitfor },

		{ "call"   , __call    },
		{ "once"   , __once    },

		{ "exit"   , __exit    },

		{ NULL     , NULL      }
};

};

#if defined(LUAJIT_VERSION)
static void luaopen_stage(lua_State *L)
{
    {
    	;
    } luaL_openlib(L, "stage", lua__stage::stagelib, 0);
}
#else
static int luaL_stageinit(lua_State *L)
{
    luaL_newlibtable(L, lua__stage::stagelib); {
        luaL_setfuncs(L, lua__stage::stagelib, 0);
    } return 1;
}


static void luaopen_stage(lua_State *L)
{
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    {
        {
        	;
        } lua_pushcclosure(L, luaL_stageinit, 0);

        lua_pushstring(L, "stage");  /* argument to open function */
        lua_call(L, 1, 1);  /* call 'openf' to open module */
        lua_pushvalue(L, -1);  /* make copy of module (call result) */
        lua_setfield(L, -3, "stage");  /* LOADED[modname] = module */
    }
    lua_remove(L, -2);  /* remove LOADED table */
    lua_setglobal(L, "stage");  /* _G[modname] = module */
}
#endif

#if !defined(LUA_BUNDLE)
LUALIB_API "C" int luaopen_broker(lua_State *L);
LUALIB_API "C" int luaopen_odbc(lua_State *L);
LUALIB_API "C" int luaopen_logger(lua_State *L);
LUALIB_API "C" int luaopen_bundle(lua_State *L);
#endif

struct luaL_Reg bundlelibs[] = {
#if !defined(LUA_BUNDLE)
		{ "broker" , luaopen_broker  },
		{ "odbc"   , luaopen_odbc    },
		{ "logger" , luaopen_logger  },
		{ "bundle" , luaopen_bundle  },
#endif
		{ NULL     , NULL            }
};

/* */
static int luaL_loadall(lua_State *L, const char *f)
{
    struct stat st;

    if (lstat(f, &st) < 0);
    else if (S_ISDIR(st.st_mode))
    {
        DIR *dp;

        if ((dp = opendir(f)) == NULL) return LUA_ERRERR;
        {
            std::string x__f(f);
            int x__top = lua_gettop(L);

            x__f.append("/");
            for (struct dirent *e; (e = readdir(dp)) != NULL; )
                if (e->d_name[0] != '.')
                {
                    luaL_loadall(L, (x__f + e->d_name).c_str());
                }
            lua_settop(L, x__top);
        }
        return closedir(dp);
    }
    else if (S_ISREG(st.st_mode) == false);
    else if (strstr(f, ".lua") != NULL)
    {
        if ((luaL_loadfile(L, f) == LUA_OK) &&
                (lua_pcall(L, 0, 0, 0) == LUA_OK))
            return LUA_OK;
        _WARN("%d: %s - '%s'", __LINE__, f, lua_tostring(L, -1))
    }
    else if (strstr(f, ".json") != NULL)
    {
        Json::Value root;

        std::ifstream json_file(f);
        if (json_file.is_open() == false)
            return luaL_error(L, "%d: Failed to open file [%s]", __LINE__, f);
        else {
            Json::Reader reader;

            if (reader.parse(json_file, root))
                json_file.close();
            else {
                _WARN("%d: %s - '%s'", __LINE__, f, reader.getFormattedErrorMessages().c_str())
                json_file.close();
                return LUA_ERRFILE;
            }
        }

        if (root.isArray()) for (int x__i = 0; x__i < (int)root.size(); ++x__i)
        {
            Json::Value &v = root[x__i];
            if (v.isString()) luaL_loadall(L, v.asCString());
        }
        else if (root.isObject()) for (const auto &k : root.getMemberNames())
        {
            Json::Value &v = root[k];
            if (v.isString()) luaL_loadall(L, v.asCString());
        }
        else if (root.isString()) luaL_loadall(L, root.asCString());
        return LUA_OK;
    }
    return LUA_ERRERR;
}

/* */
static int __luaPanic(lua_State *L)
{
	LOG4CXX_FATAL(logger,
			std::format("PANIC: unprotected error in call to Lua API (%s)", lua_tostring(L, -1)).c_str());
	abort();
	return 0;
}

lua__stage::lua__Stage::lua__Stage(SANDBOX_luaHelper *sandbox/*, Json::Value *odbc */)
    : SANDBOX_luaHelper::Proxy(sandbox)
    , L(luaL_newstate())
{
#if defined(LUAJIT_MODE_ENGINE)
    luaJIT_setmode(L, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_ON);
#endif
	luaL_openlibs(L);
    {
        luaopen_helper(L);
		for (const luaL_Reg *lib = bundlelibs; lib->func; lib++)
		{
			luaL_requiref(L, lib->name, lib->func, 1);
			lua_pop(L, 1);  /* remove lib */
		}
        {
            {
                lua_pushcfunction(L, __luaTraceback);
            } lua_setglobal(L, "_TRACEBACK");
            {
                lua_pushthread(L);
            } lua_setglobal(L, "_LUA");
            {
                luaL_pushhelper(L, &ESYS->PROCESS, NULL);
            } lua_setglobal(L, "process");
            {
                luaL_pushhelper(L, &this->THREAD, NULL);
            } lua_setglobal(L, "thread");
        }

	    lua_atpanic(L, __luaPanic);
    } lua_settop(L, 0);
}

int lua__stage::lua__Stage::prepare(Json::Value v)
{
    try {
        if (v.isArray())
            for (int __x = 0; __x < (int)v.size(); ++__x)
            {
                if (v[__x].isString())
                    luaL_loadall(this->L, v[__x].asCString());
            }
        else if (v.isString()) luaL_loadall(this->L, v.asCString());
    } catch (...) { return -1; }
    return 0;
}

lua__stage::LUA__TLS::LUA__TLS()
    : lua__Stage(/* NULL */)
{
	luaopen_stage(L);
	{
		Json::Value v;

		ENTER_RDLOCK(&ESYS->rwlock); {
			v = ESYS->config.get("%preload", Json::nullValue);
		} LEAVE_RDLOCK(&ESYS->rwlock);
		{
			this->prepare(v);
		} lua_settop(L, 0);
	} apply(L)->Ref(); /* apply sandbox */
}

#define MAX_TASK_QUEUE          2048
lua__stage::LUA__STAGE::LUA__STAGE(const char *stage_id)
    : luaObjectRef("LUA__STAGE")
    , timers(pq_init(MAX_TASK_QUEUE, __lessThen, this))
    , sign(stage_id ? stage_id: "")
    , timeYield(0)
{
    INIT_RWLOCK(&rwlock);
}
#undef MAX_TASK_QUEUE

static void JSON_mergeObject(Json::Value &o, Json::Value &i)
{
    if ((o.isObject() == false) || (i.isObject() == false)) return;
    for (const auto &k : i.getMemberNames())
    {
        if (o[k].isObject()) JSON_mergeObject(o[k], i[k]);
        else o[k] = i[k];
    }
}

#if defined(LUAJIT_VERSION)
EXPORT_STAGE(luajit)
#else
EXPORT_STAGE(lua)
#endif
{
    Json::Value &startup = *((Json::Value *)j__config);

    pthread_once(&lua__stage::LUA__initOnce, lua__stage::LUA__atInit);
    if (startup.isString())
        ;
    else {
    	guard::rwlock x__lock(&lua__stage::ESYS->rwlock);

        if (startup.isObject()) JSON_mergeObject(lua__stage::ESYS->config, startup);
        else if (startup.isArray())
        {
            for (int __i = 0; __i < (int)startup.size(); ++__i)
            {
                Json::Value v = startup[__i];

                if (v.isObject());
                else if (v.isString())
                {
                    const char *i = v.asCString();

                    if (startup.isMember(i)) v = startup[i];
                    else if (access(i, 0) == 0)
                    {
                        try {
                            std::ifstream json_file(i);

                            if (json_file.is_open()) json_file >> v;
                            else _EXIT("File not found: '%s'", i);
                            json_file.close();
                        } catch (Json::RuntimeError e) { _WARN("%s: %s", i, e.what()); }
                    }
                }

                JSON_mergeObject(lua__stage::ESYS->config, v);
            }
        }

        startup = lua__stage::ESYS->config.get(stage_id, Json::nullValue);
        if (startup.isNull())
        {
	        char *__i = (char *)strchr(stage_id, '+');

	        if (__i != NULL)
		        startup = lua__stage::ESYS->config.get(__i + 1, Json::nullValue);
        }
    }

    /* */
    {
        char *__i = (char *)strchr(stage_id, '+');
	    lua__stage::LUA__STAGE *stage =
	    		new lua__stage::LUA__STAGE((__i != NULL) ? (__i + 1): NULL); {
				    (*udata) = stage;
	    		} stage->Ref();
    } (*r__callback) = (stage_callback)lua__stage::messageCallback;

    struct BOOT__LUA {
        Json::Value v;
        lua__stage::LUA__STAGE *stage;

	    BOOT__LUA(lua__stage::LUA__STAGE *a, Json::Value &b)
            : v(b)
            , stage(a) { stage->Ref(); }
        virtual ~BOOT__LUA() { stage->Unref(); }

        lua__stage::LUA__STAGE *operator ->() { return stage; }
    };

#define DELAY_TIME              0
    if (startup.isString())
        stage_addtask(utimeNow(NULL) + DELAY_TIME, [](uint64_t timeBreak, bson_iterator *__it, struct sockaddr_stage *addr, void **udata) {
	        BOOT__LUA *__x = *((BOOT__LUA **)udata);
	        lua__stage::lua__Stage::GC g__scope(*lua__stage::TLS_LUA, __x->stage);

            if (addr != NULL)
                lua__stage::TLS_LUA->dofile(__x->v.asCString(), [&](lua_State *L, const char *f, int nret)
                {
                    auto registerCallback = [&](lua_State *L, const char *f) {
                        _INFO("%d: WAIT for '%s'", getpid(), *(f + 0) ? f: "<idle>");

                        guard::rwlock __rwlock(&(*__x)->rwlock);

                        lua__stage::LUA__STAGE::CallbackSet::iterator c_it = (*__x)->waitfors.find(f);
                        if (c_it == (*__x)->waitfors.end())
                        {
                            c_it = (*__x)->waitfors.insert(
                                    lua__stage::LUA__STAGE::CallbackSet::value_type(f, new lua__stage::LUA__STAGE::Callback(NULL))
                            ).first;
                            c_it->second->Ref();
                        }

                        c_it->second->dump(L, -1);
                    };

                    switch (lua_type(L, -1))
                    {
                        case LUA_TTABLE:
                        {
                            int base = lua_gettop(L);

                            for (lua_pushnil(L); lua_next(L, base); lua_pop(L, 1))
                            {
                                if (lua_type(L, -1) == LUA_TFUNCTION)
                                    registerCallback(L, lua_tostring(L, -2));
                            }
                        } break;

                        case LUA_TFUNCTION: registerCallback(L, "");
                    }
                });
            else delete __x;
            return -1;
        }, new BOOT__LUA((lua__stage::LUA__STAGE *)(*udata), startup));
#undef DELAY_TIME
    return 0;
}
