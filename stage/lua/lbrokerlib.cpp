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
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/un.h>
#include "lhelper.hpp"
#include "eventio.h"
#include "single.h"

#include "sockapi.h"
#include "pqueue.h"
#if 0
#include "zlib.h"
#endif
#include <cryptopp/rsa.h>
#include <cryptopp/des.h>
#include <cryptopp/aes.h>
#include <cryptopp/seed.h>
#include <cryptopp/modes.h>

#include <math.h>
#include <list>

DECLARE_LOGGER("lua");

#define _HELPER_TRACE()	        _TRACE("[%u.%p] - %s [%s:%d]", pthread_self(), this, this->SIGN.c_str(), __FUNCTION__, __LINE__)
namespace lua__stage {

	static luaObjectRef *ref(lua_State *L)
	{
		luaObjectRef *stage = NULL;

		{
			lua_getglobal(L, "_STAGE");
			if (lua_type(L, -1) == LUA_TLIGHTUSERDATA)
				(stage = (luaObjectRef *)lua_touserdata(L, -1))->Ref();
		} lua_pop(L, 1);
		return stage;
	}

	static void unref(luaObjectRef *stage) {
		if (stage != NULL) stage->Unref();
	}

};

namespace lua__io {

/* */
struct BROKER__SYS {
    pq_pState adapters;
    LOCK_T lock;

#define MAX_ADAPTER             16
    BROKER__SYS()
        : adapters(pq_init(MAX_ADAPTER, __lessThen, NULL)) {
        INIT_LOCK(&lock);
    }

    int setadapter(efile efd)
    {
        guard::lock __lock(&this->lock);

        pq_adjust(adapters);
        return efile_setadapter(efd, (eadapter)pq_top(adapters));
    }

private:
    static int __lessThen(const void *l, const void *r, void *u) {
        return eadapter_nactive((eadapter)r) - eadapter_nactive((eadapter)l);
    }
} *ESYS = NULL;


/* */
class CryptoAdapter: public luaObjectRef { /* 암호화 처리를 하기위한 원형(최상위) 클래스 */
public:
    CryptoAdapter(): luaObjectRef("broker::CryptoAdapter") { }
    virtual ~CryptoAdapter() { }

#define CRYPTO_KEYSIZE              32
    struct KEY {
        uint8_t _key[CRYPTO_KEYSIZE];

        KEY(uint8_t *k = NULL, int k_length = 0) {
            setKey(k, k_length);
        }
        virtual ~KEY() { }

        virtual void setKey(uint8_t *k, int k_length) {
            int l = std::min(k_length, (int)sizeof(_key));

            if (k != NULL) memcpy(_key, k, l);
            if ((sizeof(_key) - l) > 0)
                memset(_key + l, 0, sizeof(_key) - l);
        }
    };
#undef CRYPTO_KEYSIZE

    virtual void setDefaultKey(uint8_t *k, int k_length) { }
    /* */
    virtual int encrypt(struct KEY *k, const uint8_t *i, int i_length, uint8_t **o, int offset = 0) = 0;
    virtual int decrypt(struct KEY *k, const uint8_t *i, int i_length, uint8_t **o, int offset = 0) = 0;

    virtual const char *toString() = 0;

    /* */
    static CryptoAdapter *createAdapter(const char *cipher);
protected:
    virtual void Dispose() { delete this; }
};

#if 0
struct CryptoHelper: public luaObjectRef, public CryptoAdapter::KEY { /* 암호화 처리 클래스 */
    CryptoHelper(CryptoAdapter *adapter, uint8_t *k = NULL, int length = 0)
            : luaObjectRef("broker::CryptoHelper")
            , CryptoAdapter::KEY(k, length)
            , adapter_(adapter) {
        adapter->ref();
    }
    virtual ~CryptoHelper() { adapter_->unref(); }

    virtual CryptoAdapter *getAdapter() { return adapter_; }
    virtual int encrypt(bson *b, uint8_t **o) { /* 암호화 */
        int o_length = adapter_->encrypt(this, (uint8_t *)bson_data(b), bson_size(b), o, sizeof(uint32_t));
        if (o_length > 0)
        {
            uint32_t l = o_length | 0x80000000;
            bson_little_endian32( (*o), &l);
        }
        return o_length;
    }

    virtual int decrypt(const uint8_t *i, int i_length, uint8_t **o) { /* 복호화 */
        int length = adapter_->decrypt(this, i, i_length, o, sizeof(uint32_t));
        if (length > 0)
            bson_little_endian32(((uint8_t *)i), &length);
        return length;
    }

protected:
    virtual void Dispose() { delete this; }

private:
    CryptoAdapter *adapter_;
};
#endif

/* */
#if 0
#define GZIP_ENCODING				16

/* ZIP형태로 패킷을 처리하는 클래스 */
class ZLIB_CryptoAdapter: public CryptoAdapter {
public:
    ZLIB_CryptoAdapter()
            : CryptoAdapter() {}
    virtual ~ZLIB_CryptoAdapter() { }

    virtual int encrypt(struct CryptoAdapter::KEY *k, const uint8_t *i, int i_length, uint8_t **o, int offset = 0)
    {
        z_stream st;

        st.zalloc = Z_NULL;
        st.zfree = Z_NULL;
        st.opaque = Z_NULL;
        if (deflateInit2(&st, Z_DEFAULT_COMPRESSION,
                         Z_DEFLATED, MAX_WBITS | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            return -1;
        else {
            int a_length = i_length - offset;

            st.avail_out = std::max(a_length + 1, BUFSIZ);
            if (!((*o) = (uint8_t *) malloc(st.avail_out)))
                return -1;
            else {
                st.next_in = (z_const Bytef *) (i + offset);
                st.avail_in = a_length;

                st.next_out = (Bytef *) ((*o) + offset);
            }
            i_length = (deflate(&st, Z_FINISH) == Z_STREAM_END) ? (st.total_out + offset): -1;
            if (i_length > 0)
            {
                bson_little_endian32(((*o) + i_length), &a_length);
                i_length += sizeof(int);
            }
        }
        deflateEnd(&st);
        return i_length;
    }

    virtual int decrypt(struct CryptoAdapter::KEY *k, const uint8_t *i, int i_length, uint8_t **o, int offset = 0)
    {
        z_stream st;

        st.zalloc = Z_NULL;
        st.zfree = Z_NULL;
        st.opaque = Z_NULL;
        if (inflateInit2(&st, MAX_WBITS | GZIP_ENCODING) != Z_OK)
            return -1;
        else {
            int a_length;

            st.next_in = (z_const Bytef *)(i + offset);
            st.avail_in = (i_length - offset) - sizeof(int);
            bson_little_endian32(&a_length, st.next_in + st.avail_in);
            if (!((*o) = (uint8_t *)malloc(a_length + offset)))
                return -1;
            else {
                st.next_out = (*o) + offset;
                st.avail_out = a_length;
            }

            i_length = (inflate(&st, Z_FINISH) == Z_STREAM_END) ? (st.total_out + offset): -1;
        }
        inflateEnd(&st);
        return i_length;
    }

    virtual const char *toString() { return "ZLIB"; }
protected:
    virtual void dispose() { delete this; }
};
#endif

/* 암호화 처리 클래스 */
template <typename CIPHER, typename CIPHER_MODE,
        int MAX_KEYLENGTH = CIPHER::MAX_KEYLENGTH, int BLOCKSIZE = CIPHER::BLOCKSIZE>
class CIPHER_CryptoAdapter: public CryptoAdapter {
public:
    virtual void setDefaultKey(uint8_t *k, int k_length)
    {
        if (_key != NULL) delete _key;
        _key = (k == NULL) ? NULL: new KEY(k, k_length);
    }

    CIPHER_CryptoAdapter(CryptoPP::BlockPaddingSchemeDef::BlockPaddingScheme padding = CryptoPP::BlockPaddingSchemeDef::PKCS_PADDING)
            : CryptoAdapter() {
        _key = NULL;
        _blockPaddingScheme = padding;
        memset(_iv, ' ', sizeof(_iv));
    }

    virtual ~CIPHER_CryptoAdapter() {
        if (_key != NULL)
            delete _key;
    }

    virtual int encrypt(struct CryptoAdapter::KEY *k, const uint8_t *i, int i_length, uint8_t **o, int offset = 0)
    {
        uint32_t o_length = ((i_length - offset) + BLOCKSIZE) & ~(BLOCKSIZE - 1);
        if (((k == NULL) && !(k = _key)) ||
                !((*o) = (uint8_t *)calloc(1, o_length + offset)))
            ;
        else {
            try {
                CryptoPP::MeterFilter meter(new CryptoPP::ArraySink((*o) + offset, o_length));
                {
                    typename CIPHER_MODE::Encryption enc(k->_key, std::min((int)sizeof(k->_key), MAX_KEYLENGTH), _iv);
                    CryptoPP::ArraySource(i + offset, i_length - offset, true,
                                          new CryptoPP::StreamTransformationFilter(
                                                  enc, new CryptoPP::Redirector(meter), _blockPaddingScheme
                                          ));
                }
                return (meter.GetTotalBytes() + offset);
            } catch (std::exception &e) { _WARN("encrypt: %s", e.what()); }
        }
        return -1;
    }

    virtual int decrypt(struct CryptoAdapter::KEY *k, const uint8_t *i, int i_length, uint8_t **o, int offset = 0)
    {
        if ((k == NULL) && !(k = _key))
            ;
        else {
            try {
                CryptoPP::MeterFilter meter(new CryptoPP::ArraySink((*o) + offset, i_length - offset));
                {
                    typename CIPHER_MODE::Decryption dec(k->_key, std::min((int)sizeof(k->_key), MAX_KEYLENGTH), _iv);

                    CryptoPP::ArraySource(i + offset, i_length - offset, true,
                                          new CryptoPP::StreamTransformationFilter(
                                                  dec, new CryptoPP::Redirector(meter), _blockPaddingScheme
                                          ));
                }
                return (meter.GetTotalBytes()) + offset;
            } catch (std::exception &e) { _WARN("decrypt: %s", e.what()); }
        }
        return -1;
    }

    virtual const char *toString() { return typeid(CIPHER_MODE).name(); }
protected:
    KEY *_key;
    uint8_t _iv[BLOCKSIZE];
    CryptoPP::BlockPaddingSchemeDef::BlockPaddingScheme _blockPaddingScheme;

    /* */
    virtual void dispose() { delete this; }
};

/* https://ko.wikipedia.org/wiki/%EB%B8%94%EB%A1%9D_%EC%95%94%ED%98%B8_%EC%9A%B4%EC%9A%A9_%EB%B0%A9%EC%8B%9D */
template <typename CIPHER, int MAX_KEYLENGTH = CIPHER::MAX_KEYLENGTH, int BLOCKSIZE = CIPHER::BLOCKSIZE>
class CBC_CryptoAdapter: public CIPHER_CryptoAdapter<CIPHER, CryptoPP::CBC_Mode<CIPHER>, MAX_KEYLENGTH, BLOCKSIZE> {
public:
    CBC_CryptoAdapter(CryptoPP::BlockPaddingSchemeDef::BlockPaddingScheme padding = CryptoPP::BlockPaddingSchemeDef::PKCS_PADDING)
            : CIPHER_CryptoAdapter<CIPHER, CryptoPP::CBC_Mode<CIPHER>, MAX_KEYLENGTH, BLOCKSIZE>(padding) { }
    virtual ~CBC_CryptoAdapter() { }

protected:
    virtual void dispose() { delete this; }
};


template <typename CIPHER, int MAX_KEYLENGTH = CIPHER::MAX_KEYLENGTH, int BLOCKSIZE = CIPHER::BLOCKSIZE>
class CFB_CryptoAdapter: public CIPHER_CryptoAdapter<CIPHER, CryptoPP::CFB_Mode<CIPHER>, MAX_KEYLENGTH, BLOCKSIZE> {
public:
    CFB_CryptoAdapter(CryptoPP::BlockPaddingSchemeDef::BlockPaddingScheme padding = CryptoPP::BlockPaddingSchemeDef::PKCS_PADDING)
            : CIPHER_CryptoAdapter<CIPHER, CryptoPP::CFB_Mode<CIPHER>, MAX_KEYLENGTH, BLOCKSIZE>(padding) { }
    virtual ~CFB_CryptoAdapter() { }

protected:
    virtual void dispose() { delete this; }
};

/* */
CryptoAdapter *CryptoAdapter::createAdapter(const char *cipher)
{
    std::stringset sep;

    if ((cipher == NULL) || (std::tokenize(cipher, sep, "/") == 0)) return NULL;
#if 0
    else if (strcasecmp(sep[0].c_str(), "ZLIB") == 0) return new ZLIB_CryptoAdapter();
#endif
    else {
        /* NO_PADDING, ZEROS_PADDING, PKCS_PADDING, ONE_AND_ZEROS_PADDING, DEFAULT_PADDING */
        /* http://blog.daum.net/_blog/BlogTypeView.do?blogid=0Z2do&articleno=2 */
        // "DES/ECB/PKCS5Padding"
        CryptoPP::BlockPaddingSchemeDef::BlockPaddingScheme padding = CryptoPP::BlockPaddingSchemeDef::DEFAULT_PADDING;

        if (sep.size() > 3)
        {
            if (strcasecmp(sep[2].c_str(), "PKCS5Padding") == 0) ;
            else if (strcasecmp(sep[2].c_str(), "NoPadding") == 0)
                padding = CryptoPP::BlockPaddingSchemeDef::NO_PADDING;
            else if (strcasecmp(sep[2].c_str(), "PKCS1Padding") == 0)
                padding = CryptoPP::BlockPaddingSchemeDef::ONE_AND_ZEROS_PADDING;
            else throw __LINE__;
        }

#define ASSIGN_CIPHER(_K, __CIPHER) do {                                                        \
    if (strcasecmp(_K, "AES") == 0)           return new __CIPHER<CryptoPP::AES>(padding);      \
    else if (strcasecmp(_K, "DES") == 0)      return new __CIPHER<CryptoPP::DES>(padding);      \
    else if (strcasecmp(_K, "SEED") == 0)     return new __CIPHER<CryptoPP::SEED>(padding);     \
    else if (strcasecmp(_K, "RIJNDAEL") == 0) return new __CIPHER<CryptoPP::Rijndael>(padding); \
    else throw __LINE__;    \
} while (0)
        if ((sep.size() == 1) || (strcasecmp(sep[1].c_str(), "CBC") == 0))
            ASSIGN_CIPHER(sep[0].c_str(), CBC_CryptoAdapter);
        else if (strcasecmp(sep[1].c_str(), "CFB") == 0)
            ASSIGN_CIPHER(sep[0].c_str(), CFB_CryptoAdapter);
        else throw __LINE__;
#undef ASSIGN_CIPHER
    }
}

/* */
static int BSON_cachein(efile efd, struct cache *ca, uint64_t *value);

struct Adapter: public luaObjectHelper
{
    typedef std::map<std::string, luaObjectHelper::Callback> luaCallbackSet;

    struct {
        long inK,
             outK;
        ecache_combine_t combine;
    } cache;

    struct Stage: public luaObjectRef,
                  public eadapter_event
    {
        Stage(Adapter *s, uint32_t events = 0)
                : luaObjectRef("broker::Adapter::Stage")
                , eadapter_event()
                , adapter(s)
                , crypto(NULL)
                , timeExpire(0)
                , timeFlush(0) {

            this->adapter->Ref();
            this->f = NULL;
            this->events = events;

            _HELPER_TRACE();
            INIT_RWLOCK(&rwlock);
        }

        virtual ~Stage() {
            _HELPER_TRACE();
            adapter->Unref();
            {
                luaObjectHelper::destroy(objects);
                if (this->crypto != NULL) this->crypto->Unref();
                if (this->f != NULL) ::close(efile_close(this->f));
            }
            DESTROY_RWLOCK(&rwlock);
        }

        /* */
        int shutdown() {
            guard::rwlock __rwlock(&rwlock);
            return (this->f ? ::shutdown(efile_fileno(this->f), SHUT_RD): -1);
        }

        int close()
        {
            int r = -1; {
                guard::rwlock __rwlock(&rwlock);
                if (this->f == NULL)
                    return -1;
                else {
                    r = ::close(efile_close(this->f));
                    this->f = NULL;
                }
            } this->Unref();
            return r;
        }

        int flush(int timeOut = 0) {
            guard::rwlock __rwlock(&rwlock);
            return (this->f == NULL) ? -1: efile_flush(this->f, timeOut);
        }

        int dump(Stage *stage, const char *name) /* no rwlock */
        {
            guard::rdlock __rdlock(&stage->rwlock);

            luaCallbackSet::iterator c_it = stage->callbacks.find(name);
            if (c_it == stage->callbacks.end())
                return -1;
            else {
                luaCallbackSet::iterator d_it = this->callbacks.find(name);
                if (d_it == this->callbacks.end())
                    d_it = this->callbacks.insert(
                            luaCallbackSet::value_type(name, luaObjectHelper::Callback())
                    ).first;

                d_it->second.assign(c_it->second.data(), c_it->second.length());
            }
            return 0;
        }

        /* */
        int dump(lua_State *L, int f_idx, const char *name) /* no rwlock */
        {
            luaObjectHelper::Scope scope(L);
_gW:        switch (lua_type(L, f_idx))
            {
                case LUA_TNIL: this->callbacks.erase(name);
                default      : break;

                case LUA_TTABLE: lua_pushvalue(L, f_idx);
                {
                    int top = lua_gettop(L);

                    {
                        lua_pushstring(L, name);
                    } lua_rawget(L, top);

                    lua_remove(L, top); f_idx = top;
                } goto _gW;

                case LUA_TFUNCTION:
                {
                    luaCallbackSet::iterator c_it = this->callbacks.find(name);
                    if (c_it == this->callbacks.end())
                        c_it = this->callbacks.insert(
                                luaCallbackSet::value_type(name, luaObjectHelper::Callback())
                        ).first;

                    return c_it->second.dump(L, f_idx);
                }
            }
            return 0;
        }

        int call(lua_State *L, const char *name, int args, int nret = -1)
        {
            {
                guard::rdlock __rdlock(&this->rwlock);

                luaCallbackSet::iterator c_it = this->callbacks.find(name);
                if (c_it == this->callbacks.end()) return LUA_ERRFILE;
                else {
                    if (L == NULL) return LUA_OK;
                    else if (c_it->second.load(L) != LUA_OK)
                        return LUA_ERRRUN;
                }
            } lua_insert(L, lua_gettop(L) - args);
            return luaObjectHelper::call(L, 0, args, nret);
        }

        void push(lua_State *L)
        {
            this->Ref(); {
                guard::rdlock __rdlock(&this->rwlock);
                luaL_pushhelper(L, this->adapter, this);
            }
        }

        /* */
        Adapter       *adapter;
        CryptoAdapter *crypto;

        luaObjectHelper::BsonSet objects;
        luaCallbackSet callbacks;
        RWLOCK_T       rwlock;

        utime_t        timeExpire;
        utime_t        timeFlush;

        int commit(const char *o_buffer, int o_length, struct sockaddr *o_addr = NULL, socklen_t addrlen = 0)
        {
            void *x__buffer = NULL;

            if (adapter->cache.combine == &BSON_cachein)
            {
                guard::rdlock __rdlock(&this->rwlock);

                if (this->crypto != NULL)
                {
                    o_length = crypto->encrypt(NULL, (uint8_t *)o_buffer,
                            o_length, (uint8_t **)&o_buffer, sizeof(uint32_t));
                    if (o_length > 0)
                    {
                        uint32_t l = o_length | 0x80000000;
                        bson_little_endian32( (uint8_t **)o_buffer, &l);
                    }
                    x__buffer = (void *)o_buffer;
                }
            }

            int r;
            {
                guard::rwlock __rwlock(&this->rwlock);
                r = (o_addr && (addrlen > 0))
                    ? sendto(efile_fileno(this->f), o_buffer, o_length, 0, o_addr, addrlen)
                    : efile_write(this->f, o_buffer, o_length);
            } if (x__buffer != NULL) free(x__buffer);
            return r;
        }

        int commit(lua_State *L, int idx, struct sockaddr *o_addr = NULL, socklen_t addrlen = 0)
        {
            int r = -1;

            switch (lua_type(L, idx))
            {
                case LUA_TSTRING:
                {
                    size_t length = 0;
                    const char *b = lua_tolstring(L, idx, &length);

                    guard::rwlock __rwlock(&this->rwlock);
                    r = (o_addr && (addrlen > 0))
                            ? sendto(efile_fileno(this->f), b, length, 0, o_addr, addrlen)
                            : efile_write(this->f, b, length);
                } break;

                case LUA_TTABLE:
                {
                    bson_scope b_in;

                    if ((r = luaL_toBSON(L, idx, "v", &b_in, NULL)) < 0)
                        ;
                    else {
                        bson_finish(&b_in);
                        r = this->commit(bson_data(&b_in), bson_size(&b_in), o_addr, addrlen);
                    }
                } break;

                case LUA_TNIL: r = this->flush();
                default: break;
            }
            return r;
        }
    protected:
        virtual void Dispose() { delete this; }
    };

    /* int operator ()(std::function<int (lua_State *)> f) const { return TLS_luaState_(f); } */
    int wrap(std::function<int (lua_State *)> f)
    {
	    luaSandboxHelper::GC g__scope(
	    		NULL, (lua_State *)stage_api("lua", (uintptr_t)this->stage)
	    	);
	    return f(g__scope.L);
    }

    static int unwrap(lua_State *L, int idx, Stage **stage)
    {
        switch (lua_type(L, idx))
        {
            default: return luaL_error(L, "%d: Unsupported data types [%d]", __LINE__, lua_type(L, idx));

            case LUA_TNUMBER:
            {
                efile f = efile_get(lua_tointeger(L, idx));

                if ((f == NULL) || (efile_adapter(f) == NULL))
                    lua_pushnil(L);
                else {
                    (*stage) = *((Adapter::Stage **)efile_udata(f));
                    break;
                }
            } return 1 /* luaL_error(L, "%d - Not available.", __LINE__) */;

            case LUA_TUSERDATA:
            {
                Adapter *adp = (Adapter *)luaObjectHelper::unwrap(L, idx, (void **)stage);
                if (adp && (*stage) && ((*stage)->adapter == adp))
                    break;
                else {
                    lua_pushnil(L);
                }
            } return 1 /* luaL_error(L, "%d - Not available.", __LINE__) */;
        } return 0;
    }

    virtual int transfer(lua_State *L, int op, void *x__u)
    {
        Stage *stage = (Stage *)x__u;

        switch (op)
        {
            case  1: stage->push(L); break;
            case  0: stage->Ref(); break;
            default: stage->Unref();
        } return 0;
    }

    /* */
    Adapter(lua_State *L/* int (*f)(std::function<int (lua_State *)>) */)
            : luaObjectHelper("broker::Adapter", true)
            /* , TLS_luaState_(f) */ {
        this->cache.combine = NULL;
        this->cache.inK = 0;
        this->cache.outK = 0;
	    {
	    	this->stage = lua__stage::ref(L);
	    } _HELPER_TRACE();
    }

    virtual ~Adapter() {
	    {
	    	lua__stage::unref(this->stage);
	    } _HELPER_TRACE();
    }

    virtual int __index(lua_State *L, void *u)
    {
        typedef int (*getaddrname_t)(int, struct sockaddr *, socklen_t *);
        auto luaL_pushaddr = [](lua_State *L, efile efd, getaddrname_t f) {
            struct sockaddr_stage addr;

            addr.ss_length = sizeof(struct sockaddr_storage);
            if (f(efile_fileno(efd), (struct sockaddr *)&addr, &addr.ss_length) < 0)
                return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
            else {
                lua_pushlstring(L, (const char *)&addr, addr.ss_length);
            } return 1;
        };

        Stage *stage = (Stage *)u;
        switch (lua_type(L, -1))
        {
            case LUA_TNUMBER: switch (lua_tointeger(L, -1))
            {
                default: return 0;

                case -1: return luaL_pushaddr(L, stage->f, getsockname);
                case 1: return luaL_pushaddr(L, stage->f, getpeername);
                case 0: lua_pushinteger(L, efile_regno(stage->f));
            } break;

            default:
            {
                const std::string k = lua_tostring(L, -1);

                if (k == "commit") return link(L, (luaEventClosure_t)Adapter::__commit, u);
                else if (k == "close") return link(L, (luaEventClosure_t)Adapter::__close, u);
                /* else if (method == "scope") return link(L, (luaEventClosure_t) Adapter::__scope, u); */
                else {
                    guard::rdlock __rdlock(&stage->rwlock);

                    /* if (method == "fd") lua_pushinteger(L, efile_fileno(stage->f));
                    else */if (k == "id") lua_pushinteger(L, efile_regno(stage->f));
                    else if (k == "crypto") lua_pushboolean(L, (stage->crypto != NULL));
                    else if (k == "errno") lua_pushinteger(L, get_sockerr(efile_fileno(stage->f)));
                    else {
                        luaObjectHelper::Bson *o =
                                luaObjectHelper::fetch(stage->objects, k.c_str(), NULL);
                        if (o != NULL)
                        {
                            o->fetch(L, "broker", true);
                            o->Unref();
                        }
                    }
                }
            } break;
        }
        return 1;
    }

    virtual int __newindex(lua_State *L, void *u)
    {
        const std::string k = luaL_checkstring(L, -2);

        Stage *stage = (Stage *)u;
        guard::rwlock __rwlock(&stage->rwlock);

        if (k == "expire")
            stage->timeExpire = (lua_isnil(L, -1) ? -1: lua_tointeger(L, -1));
        else if (k == "commit") stage->timeFlush = lua_tointeger(L, -1);
        else if (k == "crypto")
        {
            /*
             * crypto = "KEY"
             * crypto = {
             *    "ZIP": "KEY"
             * }
             */
            if (stage->crypto != NULL)
                stage->crypto->Unref();

            switch (lua_type(L, -1))
            {
                case LUA_TNIL: stage->crypto = NULL; break;
                case LUA_TSTRING: stage->crypto = new CBC_CryptoAdapter<CryptoPP::Rijndael>();
_gK:            {
                    size_t length; const char *v = lua_tolstring(L, -1, &length);
                    stage->crypto->setDefaultKey((uint8_t *)v, length);
                } break;
                default:return luaL_error(L, "%d: Unsupported encryption policy.", __LINE__);

                case LUA_TTABLE: lua_pushnil(L); for (; lua_next(L, -2); lua_pop(L, 1))
                try {
                    switch (lua_type(L, -2))
                    {
                        case LUA_TSTRING:
                        {
                            stage->crypto = CryptoAdapter::createAdapter(lua_tostring(L, -2));
                        } goto _gK;
                        default: stage->crypto = CryptoAdapter::createAdapter(lua_tostring(L, -1));
                    } break;
                } catch (...) { }
            }
        }
        else if ((k == "close") || (k == "receive"))
        {
            if (stage->dump(L, -1, k.c_str()) != LUA_OK)
                return luaL_error(L, "%d: Unable to dump given function [%s]", __LINE__, k.c_str());
        }
        else switch (lua_type(L, -1))
        {
            default:
            {
                if (luaObjectHelper::assign(L, stage->objects, k.c_str(), -1) < 0)
                    return luaL_error(L, "%d: Includes non-serialable data", __LINE__);
            } return 1;
            case LUA_TNIL: return luaObjectHelper::erase(stage->objects, k.c_str());
        }
        return 1;
    }

    virtual int __call(lua_State *L, void *u) {
        Stage *stage = (Stage *)u;
        return luaObjectHelper::update(L,
                stage->objects, "broker", luaL_checkstring(L, 2), 3, &stage->rwlock);
    }


    virtual int __gc(lua_State *L, void *u) {
        ((Stage *)u)->Unref();
        return luaObjectHelper::__gc(L, u);
    }

    virtual int __tostring(lua_State *L, void *u) {
        auto tostring = [](struct sockaddr *addr) {
            switch (addr->sa_family)
            {
                case AF_INET:
                {
                    struct sockaddr_in *in4 = (struct sockaddr_in *)addr;
                    char buf[INET_ADDRSTRLEN + 1];

                    if (inet_ntop(addr->sa_family, &in4->sin_addr, buf, sizeof(buf)) != NULL)
                        return std::format("%s:%d", buf, ntohs(in4->sin_port));
                } break;

                case AF_INET6:
                {
                    struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)addr;
                    char buf[INET6_ADDRSTRLEN + 1];

                    if (inet_ntop(addr->sa_family, &in6->sin6_addr, buf, sizeof(buf)) != NULL)
                        return std::format("[%s]:%d", buf, ntohs(in6->sin6_port));
                } break;

                case AF_UNIX: return std::format(((struct sockaddr_un *)addr)->sun_path);
            }

            return std::string();
        };

        Stage *stage = (Stage *)u;
        guard::rdlock __rdlock(&stage->rwlock);

        static struct X__TOSTR {
            const char *k;
            int (*f)(int, struct sockaddr *, socklen_t *);
        } x__tostr[] = {
                { "=", getpeername },
                { "-", getsockname },
                { NULL, }
        };

        struct sockaddr_stage x__addr;
        struct sockaddr *addr = (struct sockaddr *)&x__addr;
        for (struct X__TOSTR *x = x__tostr; x->k != NULL; ++x)
        {
            x__addr.ss_length = sizeof(struct sockaddr_storage);
            if (x->f(efile_fileno(stage->f), addr, &x__addr.ss_length) == 0)
            {
                lua_pushfstring(L, "broker: %d%s%s", efile_regno(stage->f), x->k, tostring(addr).c_str());
                break;
            }
        }
        return 1;
    }

    virtual void Dispose() { delete this; }
private:
	luaObjectRef *stage;

    /* std::function<int (std::function<int (lua_State *)>)> TLS_luaState_; */

    /*
     *   fd.commit(data[,addr])
     */
    static int __commit(lua_State *L, Adapter *_this, void *u)
    {
        size_t addrlen = 0;
        struct sockaddr *addr = (struct sockaddr *)luaL_optlstring(L, 2, NULL, &addrlen);

        lua_pushinteger(L, ((Stage *)u)->commit(L, 1, addr, addrlen));
        return 1;
    }

    /* static int __scope(lua_State *L, Adapter *_this, void *u)
    {
        Stage *stage = (Stage *)u;

        {
            guard::rwlock __rwlock(&stage->rwlock);

            if (luaObjectHelper::call(L, 0, lua_gettop(L) - 1) != LUA_OK)
                return luaL_error(L, "scope");
        }
        return lua_gettop(L);
    } */

    /*
     *   fd.close();
     */
    static int __close(lua_State *L, Adapter *_this, void *u)
    {
        int top = lua_gettop(L);

        lua_pushinteger(L, ((Stage *)u)->shutdown());
        return lua_gettop(L) - top;
    }
};


static int BSON_cachein(efile efd, struct cache *ca, uint64_t *value)
{
    if ((int)sizeof(uint32_t) > cache_size(ca, 0))
        ;
    else {
        uint32_t buffer,
                *begin = (uint32_t *)cache_plook(ca, (char *)&buffer, sizeof(buffer), 0);
        uint32_t __x; bson_little_endian32(&__x, (char *)begin);
        {
            int len = (__x & 0x7FFFFFFF);
            if (len <= cache_size(ca, 0))
                return len;
        }
    } return 0;
}

static int LF_cachein( efile efd, struct cache *ca, uint64_t *value)
{
    if ((int)sizeof(char) > cache_size(ca, 0))
        ;
    else {
        char *buffer;

        for (int len = 0; (buffer = cache_pbuffer(ca, &len, (*value))) != NULL; (*value) += len)
        {
        	char *r = (char *)memchr(buffer, '\n', len);
        	if (r != NULL)
	        {
		        int l = (*value) + (int)((r - buffer) + 1);

		        (*value) = 0;
		        return l;
	        }
        }
    } return 0;
}

/* broker.ready("tcp://0.0.0.0:8080?driver=in,out", function (fd, addr)
 * end)
 *
 * broker.ready("udp://224.0.0.1:8080?packet", function (fd, addr)
 * end)
 */
static int setAdapterCache(Adapter *adapter, char *filter_type)
{
    long *__cacheK[] = {
            &adapter->cache.inK,
            &adapter->cache.outK,
            NULL
    };
    int __i = 0;

    char *p = strchr(filter_type, '=');
    if (p != NULL)
    {
        char *e = NULL;

        *p++ = 0;
        for (; __cacheK[__i] != NULL; ++__i)
        {
            double v;

            if (((v = strtod(p, &e)) == HUGE_VALF) || (v == HUGE_VALF))
                break;
            else {
_gW:            switch (*(e + 0))
                {
                    case 'm': case 'M': v = v * 1024.0;
                    case 'k': case 'K': v = v * 1024.0;
                    case 'b': case 'B':
                    case ',': if (*(++e)) goto _gW;
                }

                *__cacheK[__i] = v;
            } p = e;
        }
    }

    for (int __x = __i; __cacheK[__x] != NULL; ++__x) *__cacheK[__x] = BUFSIZ;

    if ((strcmp(filter_type, "bson") == 0) ||
        (strcmp(filter_type, "packet") == 0)) adapter->cache.combine = BSON_cachein;
    else if (strcmp(filter_type, "text") == 0) adapter->cache.combine = LF_cachein;
    else return -1;
    return __i;
}

static int __ready(lua_State *L)
{
    std::string __x = luaL_checkstring(L, 1);
    char *host_or_ip = (char *)strstr(__x.c_str(), "://");
    int sock_type = SOCK_STREAM;

    if (host_or_ip == NULL)
        host_or_ip = (char *)__x.c_str();
    else {
        *(host_or_ip + 0) = 0;
        sock_type = (strcasecmp(__x.c_str(), "udp") == 0) ? SOCK_DGRAM: SOCK_STREAM;
        host_or_ip = host_or_ip + 3; /* skip "://" */
    }

    char *filter_type = (char *)strchr(host_or_ip, '?');
    if (filter_type != NULL) *filter_type++ = 0;
    {
        char *port = strrchr(host_or_ip, ':');
        if (port != NULL) *port++ = 0;

        struct addrinfo *ai = af_addrinfo(sock_type, host_or_ip, port, 0);
        if (ai == NULL) lua_pushinteger(L, errno);
        else {
            auto createAdapter = [](lua_State *L, uint32_t events) {
                Adapter *adapter = new Adapter(L
                        /*, (int (*)(std::function<int(lua_State *)>))lua_touserdata(L, lua_upvalueindex(1)) */
                );

                try {
                    return new Adapter::Stage(adapter, events);
                } catch (...) { adapter->Unref(); }
                throw __LINE__;
            };

            int fd = af_socket(ai, NULL);
#define DEF_BACKLOG             10
            if (fd < 0)
_gE:            lua_pushinteger(L, errno);
            else {
                Adapter::Stage *stage = NULL;

                _INFO("BROKER - [%s]", lua_tostring(L,  1));
                switch (sock_type)
                {
                    default:
                    {
                        try {
                            stage = createAdapter(L, EPOLLIN|EPOLLONESHOT);
                        } catch (...) { goto _gE; }

                        if (stage->dump(L, 2, "receive") != LUA_OK)
                        {
                            {
                                stage->Unref();
                                close(fd);
                            } freeaddrinfo(ai);
                            return luaL_error(L, "%d: Unable to dump given function [receive]", __LINE__);
                        }
                        else {
                            if (ai->ai_family == AF_INET)
                                if (is_maddr(((struct sockaddr_in *)ai->ai_addr)->sin_addr))
                                {
                                    struct ip_mreq mreq;

                                    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
                                    mreq.imr_multiaddr = ((struct sockaddr_in *)ai->ai_addr)->sin_addr;
                                    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
                                        goto _gC;
                                }

                            goto _gD;
                        }
                    } break;

                    case SOCK_STREAM:
                    {
                        try {
                            stage = createAdapter(L, EPOLLBAND|EPOLLIN);
                        } catch (...) { goto _gE; }
                        if (listen(fd, DEF_BACKLOG) < 0)
                            goto _gC;
                        else {
                            if (stage->dump(L, 2, "accept") != LUA_OK)
                            {
                                {
                                    stage->Unref();
                                    close(fd);
                                } freeaddrinfo(ai);
                                return luaL_error(L, "%d: Unable to dump given function [accept]", __LINE__);
                            }

                            goto _gD;
                        }
                    } break;

_gD:                if (filter_type && (setAdapterCache(stage->adapter, filter_type) < 0))
                        lua_pushinteger(L, EINVAL);
                    else if (!(stage->f = efile_open(fd, stage->events, stage)) ||
                                (ESYS->setadapter(stage->f) < 0))
_gC:                    lua_pushinteger(L, errno);
                    else {
                        {
                            stage->Ref(); /* efile_open */
                        } stage->push(L);
                        return 1;
                    } break;
                }

                stage->Unref();
            }
#undef DEF_BACKLOG
            close(fd);
            freeaddrinfo(ai);
        }
    }
    return 1;
}

static int __close(lua_State *L)
{
    auto f__close = [](efile f)
    {
        if (f != NULL)
        {
            Adapter::Stage *stage = *((Adapter::Stage **)efile_udata(f));
            if (stage != NULL) stage->shutdown();
        }
    };

    switch (lua_type(L, 1))
    {
        case LUA_TUSERDATA:
        {
            Adapter::Stage *stage = NULL;

            luaObjectHelper::unwrap(L, 1, (void **)&stage);
            if (stage != NULL) stage->shutdown();
        } break;

        case LUA_TTABLE: for (lua_pushnil(L); lua_next(L, 1); lua_pop(L, 1))
        {
            f__close(efile_get(lua_tointeger(L, -1)));
        } break;

        case LUA_TNUMBER: f__close(efile_get(lua_tointeger(L, 1)));
    }
    return 0;
}

#include <net/if.h>

static int af_connect(struct addrinfo *entry, struct addrinfo **a__i)
{
    int fd;

    if (a__i != NULL) (*a__i) = NULL;
    for (struct addrinfo *ai = entry; ai != NULL; ai = ai->ai_next)
        if ((fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) > 0)
        {
            if (set_sockattr(fd, SOCK_NDELAY) < 0) ;
            else if (ai->ai_family != AF_INET6)
                switch (connect(fd, ai->ai_addr, ai->ai_addrlen))
                {
                    case 0: goto _gD;
                    default: if (errno == EINPROGRESS)
                    {
_gE:                    fd = -fd;
_gD:                    {
                            if (a__i != NULL) (*a__i) = ai;
                        }
                        return fd;
                    } break;
                }
            else {
                struct if_nameindex *ifidx = if_nameindex();
                struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) ai->ai_addr;

                while (ifidx->if_name != NULL)
                {
                    addr6->sin6_scope_id = ifidx->if_index;
                    switch (connect(fd, (struct sockaddr *)addr6, ai->ai_addrlen))
                    {
                        case 0: goto _gD;
                        default: if (errno == EINPROGRESS) goto _gE;
                    }
                }
            }

            close(fd);
        }
    return 0;
}

/*
 * broker.join("www.naver.com:80?bson", function (fd)
 *    return function (fd, data)
 *    end;
 * end)
 */
static int __join(lua_State *L)
{
    std::string __x = luaL_checkstring(L, 1);
    char *host_or_ip = (char *)strstr(__x.c_str(), "://");

    if (host_or_ip == NULL)
        host_or_ip = (char *)__x.c_str();
    else {
        *(host_or_ip + 0) = 0;
        if (strcasecmp(__x.c_str(), "udp") == 0)
            return luaL_error(L, "%d: Unsupported type", __LINE__);
        host_or_ip = host_or_ip + 3; /* skip "://" */
    }

    char *filter_type = (char *)strchr(host_or_ip, '?');
    if (filter_type != NULL) *filter_type++ = 0;
    {
        char *port = strrchr(host_or_ip, ':');
        if (port != NULL) *port++ = 0;

        int fd = 0;
        {
            struct addrinfo *ai = af_addrinfo(SOCK_STREAM, host_or_ip, port, 0);
            if (ai != NULL)
            {
                fd = af_connect(ai, NULL);
                freeaddrinfo(ai);
                if (fd != 0)
                    goto _gS;
            } lua_pushinteger(L, errno);
            return 1;
        }

_gS:    {
            auto createAdapter = [](lua_State *L, uint32_t events) {
                Adapter *adapter = new Adapter(L
                        /*, (int (*)(std::function<int(lua_State *)>)) lua_touserdata(L, lua_upvalueindex(1)) */
                );

                try {
                    return new Adapter::Stage(adapter, events);
                } catch (...) { adapter->Unref(); }
                throw __LINE__;
            };

            Adapter::Stage *stage = createAdapter(L,
                    ((fd < 0) ? EPOLLBAND: 0)|EPOLLALIVE|EPOLLET|EPOLLOUT|EPOLLIN|EPOLLONESHOT);

#define EXPIRE_TIME             3000
            stage->timeExpire = utimeNow(NULL);
            if (lua_type(L, -1) != LUA_TNUMBER)
                stage->timeExpire += EXPIRE_TIME;
            else {
                stage->timeExpire += lua_tointeger(L, -1);
                lua_remove(L, -1);
            }
#undef EXPIRE_TIME

            if (stage->dump(L, 2, "connect") != LUA_OK)
            {
                {
                    stage->Unref();
                    close(abs(fd));
                } return luaL_error(L, "%d: Unable to dump given function [connect]", __LINE__);
            }

            /* */
            {
                Adapter *adapter = stage->adapter;

                if (filter_type && (setAdapterCache(adapter, filter_type) < 0))
                    lua_pushinteger(L, EINVAL);
                else if (!(stage->f = efile_open(abs(fd), stage->events, stage)) ||
                                (efile_setcache(stage->f, adapter->cache.inK,
                                        adapter->cache.combine, 0, adapter->cache.outK) < 0) ||
                                (ESYS->setadapter(stage->f) < 0))
                    lua_pushinteger(L, errno);
                else {
                    stage->Ref(); /* efile_open */
                    if ((stage->events & EPOLLBAND) == 0)
                    {
                        stage->timeExpire = (utime_t)-1; /* 연결 유지 */
                        stage->adapter->wrap([&](lua_State *L) {
                            stage->push(L);
                            return stage->call(L, "connect", 1);
                        });
                    }

                    stage->push(L);
                    return 1;
                }
            }

            stage->Unref();
            close(abs(fd));
        }
    } return 1;
}


static int __itof(lua_State *L)
{
    Adapter::Stage *stage = NULL;

    if (Adapter::unwrap(L, 1, &stage) == 0)
        stage->push(L);
    return 1;
}

/* broker.signal(fd, { ... }) */
static int __signal(lua_State *L)
{
    size_t      length = 0;
    const char *buffer = NULL;

    switch (lua_type(L, 2))
    {
        case LUA_TTABLE:
        {
            bson b_in; bson_init(&b_in);
            if (luaL_toBSON(L, 2, "", &b_in, NULL) >= 0)
                bson_finish(&b_in);
            else {
                bson_destroy(&b_in);
                return luaL_error(L, "%d: Includes non-serialable data", __LINE__);
            }

            buffer = bson_data(&b_in);
        } goto _gD;

        default: return luaL_error(L, "%d: Unsupported data types [%d]", __LINE__, lua_type(L, 2));

        case LUA_TSTRING: buffer = lua_tolstring(L, 2, &length);
_gD:    switch (lua_type(L, 1))
        {
            case LUA_TUSERDATA:
            case LUA_TNUMBER:
            {
                struct RECEIVE__S: public luaObjectRef {
                    Adapter::Stage *stage;

                    const char *buffer;
                    size_t      length;

                    struct sockaddr_stage addr;
                    /* */
                    Adapter::Stage *operator -> () { return stage; }
	                RECEIVE__S(const char *b, size_t l)
                            : luaObjectRef("broker::signal::RECEIVE__S")
                            , stage(NULL)
                            , buffer(b)
                            , length(l) {
                        memset(&this->addr, 0, sizeof(struct sockaddr_stage));
                    }
                    virtual ~RECEIVE__S() {
                        if ((length == 0) && buffer) bson_free((void *)buffer);
                        if (stage != NULL) stage->Unref();
                    }

                    virtual void Dispose() { delete this; }
                } *in = new RECEIVE__S(buffer, length);

                if (Adapter::unwrap(L, 1, &in->stage) > 0)
                    in->Unref();
                else {
                    size_t addrlen = 0;
                    const char *addr = luaL_optlstring(L, 3, NULL, &addrlen);

                    if (addr != NULL)
                        memcpy(&in->addr, addr, in->addr.ss_length = addrlen);
                    goto _gS;
                }

                break;
_gS:            return stage_submit((uintptr_t)((*in)->f), [](utime_t timeClosure, void *__o) {
                    struct RECEIVE__S *o = (struct RECEIVE__S *) __o;

                    (*o)->adapter->wrap([&](lua_State *L) {
                        int top = lua_gettop(L);

                        (*o)->push(L);
                        if (o->length == 0) luaL_pushBSON(L, 0, o->buffer);
                        else lua_pushlstring(L, o->buffer, o->length);

                        (*o)->call(L, "receive", lua_gettop(L) - top);
                        for (int __i = 0; (++__i) <= lua_gettop(L);)
                        {
                            if ((*o)->commit(L, __i, (struct sockaddr *)&o->addr, o->addr.ss_length) < 0)
                                return -1;
                        }
                        return 0;
                    });
                    o->Unref();
                    return 0;
                }, in->Ref());
            } break;

            case LUA_TSTRING: if ((length == 0) && buffer) /* table 변환만 처리 */
            {
                size_t addrlen;
                struct sockaddr *v = (struct sockaddr *)luaL_tolstring(L, 1, &addrlen);

                switch (v->sa_family)
                {
                    case AF_INET:
                    case AF_INET6:
                    {
                        struct sockaddr_stage x__addr;
                        bson b_o; bson_init_finished_data(&b_o, (char *)buffer);

                        memcpy(&x__addr, v, x__addr.ss_length = addrlen);
                        if (stage_signal(-1, NULL, &b_o, 0, &x__addr) < 0)
                        {
                            bson_free((void *)buffer);
                            return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
                        }
                    } break;
                    default:
                    {
                        bson_free((void *)buffer);
                    } return luaL_error(L, "%d: Not supported (only udp)", __LINE__);
                }
            } else return luaL_error(L, "%d: Not supported (only object)", __LINE__);
            break;

            case LUA_TTABLE:
            { /* 그룹 전송 */
                struct GROUP_fdStage {
                    int f;
                    struct sockaddr_stage addr;

	                GROUP_fdStage(int f): f(f) { this->addr.ss_family = AF_UNSPEC; }
	                GROUP_fdStage(struct sockaddr *addr, socklen_t len): f(-1) {
                        memcpy(&this->addr, addr, this->addr.ss_length = len);
                    }
                }; typedef std::list<GROUP_fdStage> GROUP_fdSet;

                struct GROUP__S: public luaObjectRef {
                    utime_t     timeStart;

                    const char *buffer;
                    size_t      length;

                    luaObjectHelper::Callback f;
	                GROUP__S(lua_State *L, const char *b, size_t l)
                        : luaObjectRef("broker::signal::GROUP__S")
                        , timeStart(utimeNow(NULL))
                        , buffer(b)
                        , length(l) { this->stage = lua__stage::ref(L); }

                    virtual ~GROUP__S() {
	                    {
	                    	lua__stage::unref(this->stage);
	                    } if (length == 0) bson_free((void *)buffer);
                    }

                    virtual void Dispose() { delete this; }

	                GROUP_fdSet fds;
	                luaObjectRef *stage;
                } *in = new GROUP__S(L, buffer, length);

                for (lua_pushnil(L); lua_next(L, 1); lua_pop(L, 1))
                    switch (lua_type(L, -2))
                    {
                        case LUA_TNUMBER: switch (lua_type(L, -1))
                        {
                            case LUA_TFUNCTION: in->f.dump(L, -1); break;
                            case LUA_TSTRING:
                            {
                                size_t length;
                                struct sockaddr *v = (struct sockaddr *)luaL_tolstring(L, -1, &length);
                                switch (v->sa_family)
                                {
                                    case AF_INET:
                                    case AF_INET6: in->fds.push_back(GROUP_fdStage(v, length));
                                    default: break;
                                }
                            } break;

                            case LUA_TNUMBER: in->fds.push_back(GROUP_fdStage(lua_tointeger(L, -1))); /* socket */
                            default: break;
                        } break;

                        default: break;
                    }

                if (in->fds.empty() == false)
                    return stage_submit(0, [](utime_t timeClosure, void *__o) {
                        struct GROUP__S *o = (struct GROUP__S *) __o;

                        bson b_o; bson_init_finished_data(&b_o, (char *)o->buffer);
                        for (GROUP_fdSet::iterator i_it = o->fds.begin(); i_it != o->fds.end(); ++i_it)
                        {
                            if ((*i_it).f < 0)
                                stage_signal(-1, NULL, &b_o, 0, &(*i_it).addr);
                            else {
                                efile f = efile_get((*i_it).f);
                                if ((f != NULL) && efile_adapter(f))
                                {
                                    Adapter::Stage *stage =
                                            *((Adapter::Stage **)efile_udata(f));

                                    stage->commit(o->buffer, o->length);
                                }
                            }
                        }

                        if (o->f.length() > 0)
                        {
                        	lua_State *L = (lua_State *)stage_api("lua", (uintptr_t)o->stage);
	                        luaSandboxHelper::GC g__scope(NULL, L);

	                        {
		                        lua_pushnumber(L, ((double)(utimeNow(NULL) - o->timeStart)) / 1000.0);
		                        if (o->length == 0) luaL_pushBSON(L, 0, &b_o);
		                        else lua_pushlstring(L, o->buffer, o->length);
	                        } o->f.call(L, 2, 0);
                        }

                        o->Unref();
                        return 0;
                    }, in);
                else in->Unref();
                return 1;
            } break;
        } break;
    }

    if ((length == 0) && buffer) bson_free((void *)buffer);
    return 1;
}

/* */
struct SUBMIT_luaCallback: public luaObjectRef
                         , public luaObjectHelper::Callback
{
    SUBMIT_luaCallback(lua_State *L, Adapter::Stage *stage, int base)
            : luaObjectRef("broker::SUBMIT_luaCallback")
            , luaObjectHelper::Callback()
            , allowClone(0)
            , args(luaL_toargs(L, base, "", &allowClone))
            , stage(stage) { stage->Ref(); }
    virtual ~SUBMIT_luaCallback() { stage->Unref(); luaL_freeBSON(NULL, args, allowClone); }

    virtual int call(lua_State *L) {
        if (stage != NULL) stage->push(L);
        return luaObjectHelper::call(L, 0,
                (stage != NULL) + luaObjectHelper::args(L, this->args));
    }

    int allowClone;
    const char *args;
    Adapter::Stage *stage;
protected:
    virtual void Dispose() { delete this; }
};


/* broker.submit(socket, function (socket, arg...), arg...) */
static int __submit(lua_State *L)
{
    Adapter::Stage *stage = NULL;

    if (Adapter::unwrap(L, 1, &stage) == 0) switch (lua_type(L, 2))
    {
        default: return luaL_error(L, "%d: Unsupported data types [%d]", __LINE__, lua_type(L, 2));

        case LUA_TFUNCTION:
        {
            SUBMIT_luaCallback *buffer = new SUBMIT_luaCallback(L, stage, 3);

            buffer->dump(L, 2);
            if (stage_submit((uintptr_t)stage->f, [](utime_t timeClosure, void *__o) {
                SUBMIT_luaCallback *o = (SUBMIT_luaCallback *)__o;

                o->stage->adapter->wrap([&](lua_State *L) {
	                if (o->load(L) == LUA_OK)
	                	return o->call(L);
	                else _WARN("%d - LOAD[%s]", __LINE__, lua_tostring(L, -1))
	                return -1;
                }); o->Unref();
                return 0;
            }, buffer) >= 0) return 0;
            buffer->Unref();
        } return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
    } return 0;
}

/* */
static int __ntoa(lua_State *L)
{
    size_t addrlen = 0;
    struct sockaddr *addr = (struct sockaddr *)luaL_checklstring(L, 1, &addrlen);

    switch (lua_type(L, 2))
    {
        case LUA_TNONE:
        {
            luaL_Buffer buffer;

            luaL_buffinit(L, &buffer);
            switch (addr->sa_family)
            {
                case AF_INET:
                {
                    struct sockaddr_in *in4 = (struct sockaddr_in *)addr;
                    char buf[INET_ADDRSTRLEN + 1];

                    if (inet_ntop(addr->sa_family, &in4->sin_addr, buf, sizeof(buf)) != NULL)
                        luaL_addstring(&buffer, std::format("%s:%d", buf, ntohs(in4->sin_port)).c_str());
                } break;

                case AF_INET6:
                {
                    struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)addr;
                    char buf[INET6_ADDRSTRLEN + 1];

                    if (inet_ntop(addr->sa_family, &in6->sin6_addr, buf, sizeof(buf)) != NULL)
                        luaL_addstring(&buffer, std::format("[%s]:%d", buf, ntohs(in6->sin6_port)).c_str());
                } break;

                case AF_UNIX:
                {
                    luaL_addstring(&buffer, ((struct sockaddr_un *)addr)->sun_path);
                } break;
            }
            luaL_pushresult(&buffer);
        } break;

        case LUA_TTABLE: lua_pushvalue(L, -1);
        {
#define SET_FIELD(__N, __F) do {                        \
    lua_pushstring(L, __N); __F; lua_rawset(L, -3); \
} while (0)
            SET_FIELD("family", {
                lua_pushinteger(L, addr->sa_family);
            });

            switch (addr->sa_family)
            {
                case AF_INET:
                {
                    struct sockaddr_in *in4 = (struct sockaddr_in *)addr;

                    SET_FIELD("addr", {
                        char buf[INET_ADDRSTRLEN + 1];

                        if (inet_ntop(addr->sa_family, &in4->sin_addr, buf, sizeof(buf)) != NULL)
                            lua_pushstring(L, buf);
                        else lua_pushnil(L);
                    });

                    SET_FIELD("port", {
                        lua_pushinteger(L, ntohs(in4->sin_port));
                    });
                } break;

                case AF_INET6:
                {
                    struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)addr;

                    SET_FIELD("addr", {
                        char buf[INET6_ADDRSTRLEN + 1];

                        if (inet_ntop(addr->sa_family, &in6->sin6_addr, buf, sizeof(buf)) != NULL)
                            lua_pushstring(L, buf);
                        else lua_pushnil(L);
                    });

                    SET_FIELD("port", {
                        lua_pushinteger(L, ntohs(in6->sin6_port));
                    });

                    SET_FIELD("scope_id", {
                        lua_pushinteger(L, in6->sin6_scope_id);
                    });
                } break;

                case AF_UNIX:
                {
                    SET_FIELD("path", {
                        lua_pushstring(L, ((struct sockaddr_un *)addr)->sun_path);
                    });
                } break;
            }

#undef SET_FIELD
        } break;
    }
    return 1;
}

static int __aton(lua_State *L)
{
    auto luaL_pushaddrinfo = [](lua_State *L, int ai_family, int ai_socktype, char *host_or_ip) {
        struct addrinfo hints;
        int i__allow = lua_istable(L, -1) ? INT32_MAX: 0;

        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = ai_family;
        hints.ai_socktype = ai_socktype;

        if (i__allow) lua_newtable(L);
        {
            char *port = strchr(host_or_ip, ':');

            if (port != NULL) *port++ = 0;
            if (*(host_or_ip + 0) == '[')
            {
                char *x = strchr(host_or_ip, ']');
                if (x == NULL)
                    return luaL_error(L, "%d: address error", __LINE__);
                else {
                    *(x + 0) = 0;
                }
                ++host_or_ip;
            }

            struct addrinfo *entry;
            if (getaddrinfo(host_or_ip, port, &hints, &entry) < 0)
                ;
            else {
                int __i = 0;
                for (struct addrinfo *ai = entry; ai != NULL; ai = ai->ai_next)
                {
                    lua_pushlstring(L, (const char *)ai->ai_addr, ai->ai_addrlen);
                    if ((--i__allow) < 0) /* 테이블 반환이 아닌 경우 */
                        break;
                    else {
                        lua_rawseti(L, -2, ++__i);
                    }
                }
                freeaddrinfo(entry);
            }
        } return (i__allow == 0) ? 0: 1;
    };

    std::string __x;
    {
        struct sockaddr_storage x__addr;

        size_t addrlen = 0;
        uint8_t *__addr;

        switch (lua_type(L, 1))
        {
            case LUA_TNUMBER: __addr= (uint8_t *)&x__addr;
            {
                x__addr.ss_family = lua_tointeger(L, 1);
                switch (x__addr.ss_family)
                {
                    case AF_INET: addrlen = sizeof(struct sockaddr_in); break;
                    case AF_INET6: addrlen = sizeof(struct sockaddr_in6); break;
                    case AF_UNIX: addrlen = sizeof(struct sockaddr_un); break;
                    default: return luaL_error(L, "%d: %s", __LINE__, strerror(EINVAL));
                }
            }

_gS:        {
                struct sockaddr *addr = (struct sockaddr *)__addr;

                switch (lua_type(L, 2))
                {
                    case LUA_TTABLE: switch (addr->sa_family)
                    {
                        case AF_INET:
                        {
                            struct sockaddr_in *in = (struct sockaddr_in *)addr;

                            lua_pushnil(L);
                            for (; lua_next(L, 2); lua_pop(L, 1))
                            {
                                std::string prop = lua_tostring(L, -2);
                                if (prop == "port") in->sin_port = htons(lua_tointeger(L, -1));
                                else if (prop == "addr")
                                {
                                    if (inet_pton(in->sin_family, lua_tostring(L, -1), &in->sin_addr) < 0)
                                        return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
                                }
                            }
                        } break;
                        case AF_INET6:
                        {
                            struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)addr;

                            lua_pushnil(L);
                            for (; lua_next(L, 2); lua_pop(L, 1))
                            {
                                std::string prop = lua_tostring(L, -2);
                                if (prop == "port") in6->sin6_port = htons(lua_tointeger(L, -1));
                                else if (prop == "addr")
                                {
                                    if (inet_pton(in6->sin6_family, lua_tostring(L, -1), &in6->sin6_addr) < 0)
                                        return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
                                }
                                else if (prop == "scope_id") in6->sin6_scope_id = lua_tointeger(L, -1);
                            }
                        } break;
                        case AF_UNIX:
                        {
                            struct sockaddr_un *un = (struct sockaddr_un *)addr;

                            lua_pushnil(L);
                            for (; lua_next(L, 2); lua_pop(L, 1))
                            {
                                std::string prop = lua_tostring(L, -2);
                                if (prop == "path") strcpy(un->sun_path, lua_tostring(L, -1));
                            }
                        } break;
                    } break;

                    case LUA_TSTRING: __x.assign(lua_tostring(L, 2));
                    {
                        switch (x__addr.ss_family)
                        {
                            case AF_UNIX:
                            {
                                strcpy(((struct sockaddr_un *)&x__addr)->sun_path, __x.c_str());
                            } break;

                            default: return luaL_pushaddrinfo(L,
                                    x__addr.ss_family, SOCK_STREAM, (char *)__x.c_str());
                        }
                    } break;

                    default: return luaL_error(L, "%d: %s", __LINE__, strerror(EINVAL));
                }

                lua_pushlstring(L, (char *)__addr, addrlen);
            } return 1;

            default: __addr = (uint8_t *)luaL_checklstring(L, 1, &addrlen);
            {
                for (int __i = 0; __i < (int)addrlen; ++__i)
                    if ((*(__addr + __i) & 0x80) || (*(__addr + __i) <= ' '))
                    {
                        goto _gS;
                    }
            } break;
        }

        __x.assign((char *)__addr);
    }

    char *host_or_ip = (char *)strstr(__x.c_str(), "://");
    if (host_or_ip == NULL)
        return luaL_pushaddrinfo(L, AF_UNSPEC, SOCK_STREAM, (char *)__x.c_str());
    else {
        *(host_or_ip + 0) = 0;
        if (strcasecmp(__x.c_str(), "unix") == 0)
        {
            struct sockaddr_un un;

            un.sun_family = AF_UNIX;
            strcpy(un.sun_path, host_or_ip);
            lua_pushlstring(L, (char *)&un, sizeof(struct sockaddr_un));
            return 1;
        }

        return luaL_pushaddrinfo(L, AF_UNSPEC,
                (strcasecmp(__x.c_str(), "udp") == 0) ? SOCK_DGRAM: SOCK_STREAM, host_or_ip + 3);
    }
}

static int __ifaddr(lua_State *L)
{
    struct ifaddrs *ifaddr;

    if (getifaddrs(&ifaddr) == -1)
        return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
    else {
        const char *ifa_name = luaL_optstring(L, 1, "");
        typedef std::map<std::string, std::deque<struct sockaddr *>> nameDevicesSet;

        nameDevicesSet entry;
        for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
            if ((ifa->ifa_addr != NULL) &&
                    ((*(ifa_name + 0) == 0) || (strstr(ifa->ifa_name, ifa_name) != NULL)))
            {
                if (ifa->ifa_flags & IFF_LOOPBACK);
                else if (ifa->ifa_flags & (IFF_UP|IFF_RUNNING))
                {
                    nameDevicesSet::iterator it = entry.find(ifa->ifa_name);
                    if (it == entry.end())
                        it = entry.insert(
                                nameDevicesSet::value_type(ifa->ifa_name, std::deque<struct sockaddr *>())
                        ).first;

                    it->second.push_back(ifa->ifa_addr);
                }
            }

        lua_newtable(L);
        for (nameDevicesSet::iterator it = entry.begin(); it != entry.end(); ++it)
        {
            lua_pushstring(L, it->first.c_str());

            lua_newtable(L);
            for (int __i = 0; __i < (int)it->second.size(); ++__i)
            {
                struct sockaddr *addr = it->second[__i];

                switch (addr->sa_family)
                {
                    case AF_INET: lua_pushlstring(L, (char *)addr, sizeof(struct sockaddr_in)); break;
                    case AF_INET6: lua_pushlstring(L, (char *)addr, sizeof(struct sockaddr_in6)); break;
                    default: continue;
                }
                lua_rawseti(L, -2, __i + 1);
            }
            lua_rawset(L, -3);
        }
        freeifaddrs(ifaddr);
    }
    return 1;
}

/* */
static int helperCryptoAdapter(const char *cipher, std::function<int(CryptoAdapter *)> callback)
{
    std::stringset sep;

    if ((cipher == NULL) || (std::tokenize(cipher, sep, "/") == 0)) return -1;
#if 0
    else if (strcasecmp(sep[0].c_str(), "ZLIB") == 0) { ZLIB_CryptoAdapter __x; return callback(&__x); }
#endif
    else {
        /* NO_PADDING, ZEROS_PADDING, PKCS_PADDING, ONE_AND_ZEROS_PADDING, DEFAULT_PADDING */
        /* http://blog.daum.net/_blog/BlogTypeView.do?blogid=0Z2do&articleno=2 */
        // "DES/ECB/PKCS5Padding"
        CryptoPP::BlockPaddingSchemeDef::BlockPaddingScheme padding = CryptoPP::BlockPaddingSchemeDef::DEFAULT_PADDING;

        if (sep.size() > 3)
        {
            if (strcasecmp(sep[2].c_str(), "PKCS5Padding") == 0) ;
            else if (strcasecmp(sep[2].c_str(), "NoPadding") == 0)
                padding = CryptoPP::BlockPaddingSchemeDef::NO_PADDING;
            else if (strcasecmp(sep[2].c_str(), "PKCS1Padding") == 0)
                padding = CryptoPP::BlockPaddingSchemeDef::ONE_AND_ZEROS_PADDING;
            else throw __LINE__;
        }

#define ASSIGN_CIPHER(_K, __CIPHER) do {                                         \
if (strcasecmp(_K, "AES") == 0)           { __CIPHER<CryptoPP::AES> __v(padding);      return callback(&__v); } \
else if (strcasecmp(_K, "DES") == 0)      { __CIPHER<CryptoPP::DES> __v(padding);      return callback(&__v); } \
else if (strcasecmp(_K, "SEED") == 0)     { __CIPHER<CryptoPP::SEED> __v(padding);     return callback(&__v); } \
else if (strcasecmp(_K, "RIJNDAEL") == 0) { __CIPHER<CryptoPP::Rijndael> __v(padding); return callback(&__v); } \
else throw __LINE__;    \
} while (0)
        if ((sep.size() == 1) || (strcasecmp(sep[1].c_str(), "CBC") == 0))
            ASSIGN_CIPHER(sep[0].c_str(), CBC_CryptoAdapter);
        else if (strcasecmp(sep[1].c_str(), "CFB") == 0)
            ASSIGN_CIPHER(sep[0].c_str(), CFB_CryptoAdapter);
        else throw __LINE__;
#undef ASSIGN_CIPHER
    }
}

static int __encrypt(lua_State *L)
{
    auto f__callback = [&L](CryptoAdapter *crypto) {
        bson_scope b_i;

        if (luaL_toBSON(L, 2, "v", &b_i, NULL) < 0)
            return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
        else {
            uint8_t *o_buffer = NULL;
            bson_finish(&b_i); {
                int o_length = crypto->encrypt(NULL, (uint8_t *)
                        bson_data(&b_i), bson_size(&b_i), &o_buffer, sizeof(uint32_t));
                if (o_length == 0) lua_pushnil(L);
                else if (o_length < 0)
                    return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
                {
                    uint32_t l = o_length | 0x80000000;
                    bson_little_endian32( (uint8_t **)o_buffer, &l);

                    lua_pushlstring(L, (const char *)o_buffer, o_length);
                }
            } if (o_buffer != NULL) free(o_buffer);
        } return 1;
    };

    switch (lua_type(L, 1))
    {
        case LUA_TNIL: break;
        case LUA_TUSERDATA:
        {
            Adapter::Stage *stage = NULL;
            Adapter *adp = (Adapter *)luaObjectHelper::unwrap(L, 1, (void **)&stage);

            if ((adp == NULL) || (stage == NULL) || (stage->adapter != adp))
                return luaL_error(L, "%d: Encryption object not found.", __LINE__);
            else if (stage->crypto != NULL) return f__callback(stage->crypto);
        } return luaL_error(L, "%d: Encryption settings are missing.", __LINE__);

        case LUA_TTABLE: lua_pushnil(L); for (; lua_next(L, 1); lua_pop(L, 1))
        try {
            return helperCryptoAdapter(lua_tostring(L, -2), [&](CryptoAdapter *adp) {
                {
                    size_t length; const char *v = lua_tolstring(L, -1, &length);
                    adp->setDefaultKey((uint8_t *)v, length);
                } return f__callback(adp);
            });
        } catch (...) { }
        default:return luaL_error(L, "%d: Unsupported encryption policy.", __LINE__);

        case LUA_TSTRING:
        {
            CBC_CryptoAdapter<CryptoPP::Rijndael> __x;

            size_t length; const char *v = lua_tolstring(L, -1, &length);
            __x.setDefaultKey((uint8_t *)v, length);
            return f__callback((CryptoAdapter *)&__x);
        }
    }
    return 0;
}

static int __decrypt(lua_State *L)
{
    int top = lua_gettop(L);

    auto f__callback = [&L, &top](CryptoAdapter *crypto) {
        size_t i_length = 0;
        uint8_t *i_buffer = (uint8_t *)lua_tolstring(L, 2, &i_length);

        uint32_t __x = UINT32_MAX;
        if (i_length <= sizeof(uint32_t))
            ;
        else {
            bson_little_endian32(&__x, (char *)i_buffer);
            if ((__x & 0x80000000) != 0)
            {
                int o_length = crypto->decrypt(NULL,
                        i_buffer, (int)(__x & 0x7FFFFFFF), &i_buffer, sizeof(uint32_t));
                if (o_length > 0)
                    bson_little_endian32(((uint8_t *)i_buffer), &o_length);
            }

            if (luaL_pushBSON(L, 0, (char *)i_buffer) >= 0)
                return lua_gettop(L) - top;
            lua_settop(L, top);
        }
        lua_pushlstring(L, (const char *)i_buffer, i_length);
        return lua_gettop(L) - top;
    };

    switch (lua_type(L, 1))
    {
        case LUA_TNIL: break;
        case LUA_TUSERDATA:
        {
            Adapter::Stage *stage = NULL;
            Adapter *adp = (Adapter *)luaObjectHelper::unwrap(L, 1, (void **)&stage);

            if ((adp == NULL) || (stage == NULL) || (stage->adapter != adp))
                return luaL_error(L, "%d: Encryption object not found.", __LINE__);
            else if (stage->crypto != NULL) return f__callback(stage->crypto);
        } return luaL_error(L, "%d: Encryption settings are missing.", __LINE__);

        case LUA_TTABLE: lua_pushnil(L); for (; lua_next(L, 1); lua_pop(L, 1))
        try {
            return helperCryptoAdapter(lua_tostring(L, -2), [&](CryptoAdapter *adp) {
                {
                    size_t length; const char *v = lua_tolstring(L, -1, &length);
                    adp->setDefaultKey((uint8_t *)v, length);
                } lua_settop(L, top);
                return f__callback(adp);
            });
        } catch (...) { }
        default:return luaL_error(L, "%d: Unsupported decryption policy.", __LINE__);

        case LUA_TSTRING: lua_pushvalue(L, 1);
        {
            CBC_CryptoAdapter<CryptoPP::Rijndael> __x;

            size_t length; const char *v = lua_tolstring(L, -1, &length);
            __x.setDefaultKey((uint8_t *)v, length);
            return f__callback((CryptoAdapter *)&__x);
        }
    }
    return 0;
}

static const luaL_Reg brokerlib[] = {
        { "ready"  , __ready   },
        { "join"   , __join    },
        { "close"  , __close   },
        { "f"      , __itof    },
        { "signal" , __signal  },
        { "submit" , __submit  },

        { "ntoa"   , __ntoa    },
        { "aton"   , __aton    },
        { "ifaddr" , __ifaddr  },

        { "encrypt", __encrypt },
        { "decrypt", __decrypt },

        { NULL     , NULL      }
};

/* */
static pthread_once_t BROKER__initOnce = PTHREAD_ONCE_INIT;


static void BROKER__cleanup(void *__eadp)
{
    eadapter_destroy((eadapter)__eadp);
}

static void BROKER__worker(eadapter eadp)
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    pthread_cleanup_push(BROKER__cleanup, eadp);
_gW:{
    	efile  r_efd = NULL;
        size_t r_nwait = 0;
        int    r_stat;

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL); {
            pthread_testcancel();
        } pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

#define WAIT_TIMEOUT            180
        switch (r_stat = eadapter_dispatch(eadp, &r_efd, &r_nwait, WAIT_TIMEOUT))
        {
            case -1: _WARN(" ** EXCEPT.%d:%s **", errno, strerror(errno));
            {
                eadapter_destroy(eadp);
            } pthread_exit(NULL);

            case EPOLLRESET:
#define PURGE_TIME              1000
            case EPOLLIDLE: efile_purge(PURGE_TIME);    /* 소켓 close() 이후 설정된 시간이 지난 소켓의 메모리 해제 */
            {
#define MAX_FILES               32
                efile efds[MAX_FILES];
                utime_t timeNow = utimeNow(NULL);

#define FLUSH_TIME          30
                if ((r_nwait = eadapter_flush(eadp, efds, MAX_FILES, FLUSH_TIME)) > 0)
                    do {
	                    efile efd = efds[--r_nwait];
                        Adapter::Stage *stage = *((Adapter::Stage **)efile_udata(efd));

                        if ((timeNow - efile_futime(efd)) >= stage->timeFlush)
                            efile_flush(efd, FLUSH_TIME);
                    } while ((r_nwait > 0) && ((utimeNow(NULL) - timeNow) < FLUSH_TIME));
#undef FLUSH_TIME

#define REFUSE_TIME             1000
                /* shutdown()된 파일을 찾아서 제거한다. */
                if ((r_nwait = efile_refuse(efds, MAX_FILES, REFUSE_TIME)) > 0)
                    do {
                        ::close(efile_close(efds[--r_nwait]));
                    } while (r_nwait > 0);
#undef REFUSE_TIME

                if ((r_nwait = eadapter_alive(eadp, efds, MAX_FILES, PURGE_TIME)) > 0)
                    do {
                        efile efd = efds[--r_nwait];
                        Adapter::Stage *stage = *((Adapter::Stage **)efile_udata(efd));

                        if (stage->timeExpire == (utime_t)-1);
                        else if ((stage->events & EPOLLBAND) && (timeNow < stage->timeExpire))
                            ; /* connect() 중인 경우, 타임아웃을 별도로 처리 한다. */
                        else if ((timeNow - efile_lutime(efd)) >= stage->timeExpire)
                        {
                            stage->adapter->wrap([&](lua_State *L) {
                                stage->push(L);
                                lua_pushinteger(L, ETIMEDOUT);
                                return stage->call(L, "close", 2);
                            });
                            stage->close();
                        }
                    } while (r_nwait > 0);
#undef PURGE_TIME
#undef MAX_FILES
#define DISPATCH_TIMEOUT            1
                stage_dispatch(DISPATCH_TIMEOUT);
#undef DISPATCH_TIMEOUT
            } goto _gW;

            default:
            {
                struct RECEIVE__S: public luaObjectRef {
                    Adapter::Stage *stage;
                    void   *buffer;
                    ssize_t length;

                    struct sockaddr_stage addr;

                    Adapter::Stage *operator -> () { return stage; }
                    /* */
                    RECEIVE__S(Adapter::Stage *s, size_t n, int r)
                            : luaObjectRef("broker::worker::RECEIVE__S")
                            , stage(s)
                            , buffer(malloc(n))
                            , length(n) {

                        stage->Ref();
                        addr.ss_length = (r == EPOLLRAW) ? sizeof(struct sockaddr_storage): 0;
                    }
                    virtual ~RECEIVE__S() { free(buffer); stage->Unref(); }
                    virtual void Dispose() { delete this; }
                };

                Adapter::Stage *stage = *((Adapter::Stage **)efile_udata(r_efd));
                struct sockaddr_stage addr;

                addr.ss_length = sizeof(struct sockaddr_storage);
                switch (r_stat)
                {
                    case EPOLLHUP:
                    case EPOLLERR:
_gC:                {
                        stage->adapter->wrap([&](lua_State *L) {
                            stage->push(L);
                            lua_pushinteger(L, get_sockerr(efile_fileno(stage->f)));
                            return stage->call(L, "close", 2);
                        });
                        stage->close();
                    } break;

                    case EPOLLONESHOT:
                    {
	                    efile_setevent(stage->f, stage->events);
                    } break;

                    case EPOLLBAND:
                    {
                        if (r_nwait & EPOLLOUT)  /* connect */
                            switch (get_sockerr(efile_fileno(r_efd)))
                            {
                                case 0: stage->events &= ~(EPOLLOUT | EPOLLBAND);
                                {
                                    stage->timeExpire = (utime_t)-1; /* 연결 유지 */
                                    stage->adapter->wrap([&](lua_State *L) {
                                        stage->push(L);
                                        return stage->call(L, "connect", 1);
                                    });

                                    efile_setevent(r_efd, stage->events);
                                } break;
                                default: goto _gC;
                            }
                        else {  /* accept */
                            int fd;

                            if ((fd = accept(efile_fileno(r_efd), (struct sockaddr *)&addr, &addr.ss_length)) < 0)
                                ;
                            else {
                                Adapter::Stage *clone =
                                        new Adapter::Stage(stage->adapter, EPOLLET|EPOLLIN|EPOLLONESHOT|EPOLLALIVE);

                                if ((set_sockattr(fd, SOCK_NDELAY) < 0) ||
                                        !(clone->f = efile_open(fd, clone->events, clone)))
                                    close(fd);
                                else {
                                    Adapter *adapter = clone->adapter;

                                    if ((efile_setcache(clone->f, adapter->cache.inK,
                                    		    adapter->cache.combine, 0, adapter->cache.outK) >= 0) &&
                                            (ESYS->setadapter(clone->f) >= 0) &&
                                            (stage->adapter->wrap([&](lua_State *L) {
                                                clone->Ref(); /* efile_open() */

                                                clone->dump(stage, "receive");
                                                {
                                                    clone->timeExpire = stage->timeExpire;
                                                    clone->timeFlush = stage->timeFlush;
                                                    if ((clone->crypto = stage->crypto) != NULL)
                                                        clone->crypto->Ref();
                                                } clone->dump(stage, "close");

                                                clone->push(L);
                                                lua_pushlstring(L, (char *)&addr, addr.ss_length);
                                                if (stage->call(L, "accept", 2) == LUA_OK)
                                                    switch (lua_type(L, -1))
                                                    {
                                                        case LUA_TBOOLEAN:
                                                        {
                                                            if (lua_toboolean(L, -1) == false) break;
                                                            else
                                                        default:{
                                                                if (clone->call(NULL, "receive", 0) == LUA_OK)
                                                                    goto _gD;
                                                            }
                                                            _WARN("proper function does not exist");
                                                        } break;

                                                        case LUA_TFUNCTION: clone->dump(L, -1, "receive");
_gD:                                                    {
                                                            ;
                                                        } return LUA_OK;
                                                    }
                                                return LUA_ERRERR;
                                            }) == LUA_OK)) goto _gW;
                                }
                                clone->Unref();
                            }
                        }
                    } break;

                    case EPOLLRAW:
                    case EPOLLCACHE: stage_submit((uintptr_t)r_efd, [](utime_t timeClosure, void *__o) {
                        struct RECEIVE__S *o = (struct RECEIVE__S *)__o;

                        if ((o->length = (o->addr.ss_length == 0) ? efile_read((*o)->f, o->buffer, o->length)
                                : recvfrom(efile_fileno((*o)->f), o->buffer, o->length, 0,
                                        (struct sockaddr *)&o->addr, &o->addr.ss_length)) > 0)
                            (*o)->adapter->wrap([&](lua_State *L) {
                                int top = lua_gettop(L);

                                (*o)->push(L);
                                if ((*o)->adapter->cache.combine == &BSON_cachein)
                                    try {
                                        if (o->length <= (int)sizeof(uint32_t)) throw __LINE__;
                                        {
                                            uint8_t *o__buffer = (uint8_t *)o->buffer;

                                            uint32_t __x; bson_little_endian32(&__x, (char *)o->buffer);
                                            if ((__x & 0x7FFFFFFF) != o->length) throw __LINE__;
                                            if ((__x & 0x80000000) == 0);
                                            else if ((*o)->crypto == NULL) throw __LINE__;
                                            else {
                                                int o__length = (*o)->crypto->decrypt(NULL,
                                                        o__buffer, __x & 0x7FFFFFFF, &o__buffer, sizeof(uint32_t));
                                                if (o__length > 0)
                                                    bson_little_endian32(((uint8_t *)o__buffer), &o__length);
                                            }

                                            if (luaL_pushBSON(L, 0, (char *)o__buffer) <= 0)
                                                throw __LINE__;
                                        }
                                    } catch (int x__i) {
                                    	_WARN("%d: Decode failed - %d", x__i, efile_fileno((*o)->f)); return ::shutdown(efile_fileno((*o)->f), SHUT_RD);
                                    }
                                else lua_pushlstring(L, (const char *) o->buffer, o->length);

                                if (o->addr.ss_length > 0)
                                    lua_pushlstring(L, (const char *)&o->addr, o->addr.ss_length);

                                (*o)->call(L, "receive", lua_gettop(L) - top);
                                for (int __i = 0; (++__i) <= lua_gettop(L);)
                                {
                                    if ((*o)->commit(L, __i, (struct sockaddr *)&o->addr, o->addr.ss_length) < 0)
                                        return -1;
                                }
                                return 0;
                            });
                        else _WARN("RECV: %d - '%s'", o->length, strerror(errno))
                        efile_setevent((*o)->f, (*o)->events);

                        o->Unref();
                        return 0;
                    }, new RECEIVE__S(stage, r_nwait, r_stat));
                }
            } break;
        }
#undef WAIT_TIMEOUT
    } goto _gW;
    pthread_cleanup_pop(0);
}



static void BROKER__atExit() { delete ESYS; }
static void BROKER__atInit()
{
    try {
        ESYS = new lua__io::BROKER__SYS();

        pthread_attr_t attr; if (pthread_attr_init(&attr) < 0)
            _EXIT("%d: %s", __LINE__, strerror(errno));
        else {
            int ncups = std::max(sysconf(_SC_NPROCESSORS_ONLN) / 2, 1L);

            pthread_attr_setschedpolicy(&attr, SCHED_RR);
            _INFO("BROKER %d: READY - %d cpu's", getpid(), ncups);
            do {
                eadapter eadp;

                if (!(eadp = eadapter_create(0, ESYS)))
                    _EXIT("%d: %s", __LINE__, strerror(errno));
                else {
                    pthread_t thread;
                    {
                        guard::lock __lock(&ESYS->lock);

                        if (pq_push(ESYS->adapters, eadp) != NULL)
                            _EXIT("%d: %s", __LINE__, strerror(errno));
                    }
                    if (pthread_create(&thread, &attr,(void *(*)(void *)) BROKER__worker, eadp) < 0)
                        _EXIT("%d: %s", __LINE__, strerror(errno));
                }
            } while ((--ncups) > 0);
            pthread_attr_destroy(&attr);
        }
    } catch (...) { _EXIT("%d: BROKER", __LINE__); }
    atexit(BROKER__atExit);
}

};


LUALIB_API "C" int luaopen_broker(lua_State *L)
{
    pthread_once(&lua__io::BROKER__initOnce, lua__io::BROKER__atInit);
	luaL_newlib(L, lua__io::brokerlib);
	return 1;
}