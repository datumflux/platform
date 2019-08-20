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
#include <fstream>
#include <unordered_map>
#include <sys/un.h>
#include <curl/curl.h>
#include <zlib.h>
#include "c++/tls.hpp"
#include "c++/object.hpp"
#include "siocp.h"
#include "pqueue.h"

DECLARE_LOGGER("curl");

namespace curl__stage {

struct _CURL: public ObjectRef {
    CURL *curl;
    struct {
        std::string callback_id;
        struct {
            char *buffer;
            int   length;
        } data;

        bson_type   type;
        std::string value;

        struct sockaddr_stage addr;
    } r;

    utime_t     timeBreak;
    std::string url;
    curl_slist *custom_header;
    std::string postfield;

    _CURL(utime_t timeBreak, struct sockaddr_stage *a)
            : ObjectRef("_CURL")
            , curl(curl_easy_init())
            , timeBreak(timeBreak)
            , custom_header(NULL) {
        if (curl == NULL)
            throw __LINE__;
        else {
            r.data.buffer = NULL;
            r.data.length = 0;

            if (a == NULL)
                memset(&r.addr, 0, sizeof(struct sockaddr_stage));
            else memcpy(&r.addr, a, sizeof(struct sockaddr_stage));
        }

        r.type = BSON_UNDEFINED;
    }

    virtual ~_CURL() {
        {
            if (r.data.buffer != NULL) free(r.data.buffer);
            if (this->custom_header != NULL) curl_slist_free_all(this->custom_header);
        } curl_easy_cleanup(this->curl);
    }
    /* */
    static size_t curl_write(void *ptr, size_t size, size_t nmemb, _CURL *cb)
    {
        size_t length = size * nmemb;

        size = cb->r.data.length + length;
        char *buffer = (char *)realloc( cb->r.data.buffer, size + 1);
        if (buffer == NULL)
            return 0;
        else {
            memcpy(buffer + cb->r.data.length, ptr, length);
            *(buffer + size) = 0;

            cb->r.data.buffer = buffer;
            cb->r.data.length = size;
        } return length;
    }

    virtual _CURL *Ref() { ObjectRef::Ref(); return this; }
protected:
	virtual void Dispose() { delete this; }
}; typedef std::unordered_map<CURL *, struct _CURL *> CURLAllowSet;


struct CURL__STAGE: public ObjectRef {
#define CURL_TIMEOUT            (10 * 1000)
    CURL__STAGE(const char *s, Json::Value &V)
            : ObjectRef("CURL__STAGE")
            , sign((s && *(s + 0)) ? s: "curl")
            , timeOut(CURL_TIMEOUT)
            , verbose(0) {
        if (V.isString()) agent = V.asCString();
        else if (V.isObject())
        {
            Json::Value::Members members = V.getMemberNames();
            for (Json::Value::Members::iterator it = members.begin(); it != members.end(); ++it)
            {
                Json::Value v = V[*it];
                const char *k = (*it).c_str();

                if (strcasecmp(k, "timeout") == 0) this->timeOut = v.asInt();
                else if (strcasecmp(k, "user-agent") == 0) this->agent = v.asCString();
                else if (strcasecmp(k, "verbose") == 0) this->verbose = v.asInt();
            }
        }
    }
#undef CURL_TIMEOUT
    virtual ~CURL__STAGE() { }

    std::string sign;
    std::string agent;

    long timeOut;
    long verbose;
protected:
    virtual void Dispose() { delete this; }
};

/* */
struct CURL__SYS: public ObjectRef {
    CURL__SYS(int ncpus = 0)
        : ObjectRef("CURL__SYS") {
        if (!(this->scp = scp_attach(
                std::format(".__%d.%u_curl", getpid(), pthread_self()).c_str(), 0, NULL, NULL)))
            _EXIT("%d: %s", __LINE__, strerror(errno));
        else {
            pthread_attr_t attr;

            if (pthread_attr_init(&attr) < 0)
                _EXIT("%d: %s", __LINE__, strerror(errno));
            else {
                if (ncpus <= 0) ncpus = sysconf(_SC_NPROCESSORS_ONLN) * 2;

                pthread_attr_setschedpolicy(&attr, SCHED_RR);
                do {
                    pthread_t thread;

                    if (pthread_create(&thread, &attr, (void *(*)(void *))__worker, this) < 0)
                        _EXIT("%d: %s", __LINE__, strerror(errno));
                    else {
                        this->threads.push_back(thread);
                    }
                } while ((--ncpus) > 0);
                pthread_attr_destroy(&attr);
            }
        }
    }
    virtual ~CURL__SYS() {
        for (ThreadSet::iterator t_it = threads.begin(); t_it != threads.end(); t_it++)
            if ((*t_it) == 0)
                ;
            else {
                pthread_cancel(*t_it);
                pthread_join(*t_it, NULL);
            }
        scp_detach(this->scp);
    }

    siocp  scp;
protected:
	virtual void Dispose() { delete this; }

    typedef std::deque<pthread_t> ThreadSet;

    ThreadSet threads;
    static void __worker(CURL__SYS *_this);
} *ESYS = NULL;


void CURL__SYS::__worker(CURL__SYS *_this)
{
    CURLAllowSet curl_entry;
    int still_running;

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    CURLM *curlm = curl_multi_init();
#define SIO_TIMEOUT             2
#define SIO_LATENCY             50
#define CURL_TIMEOUT            1000
    for (int r; (r = scp_try(_this->scp, SIO_TIMEOUT * (curl_entry.empty() ? SIO_LATENCY: 1))) >= 0; )
    {
        sobject sobj = NULL;

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL); {
            pthread_testcancel();
        } pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        if (r == 0);
        else if (scp_catch(_this->scp, &sobj, SOBJ_WEAK) < 0) break;
        else {
            _CURL *o = (_CURL *)sobj->o;

            scp_release(_this->scp, sobj);
            if (o == (void *)-1) break;
            else if (o != NULL)
            {
                {
                    curl_easy_setopt(o->curl, CURLOPT_WRITEFUNCTION, _CURL::curl_write);
                    curl_easy_setopt(o->curl, CURLOPT_WRITEDATA, o);
                    /* {
                        char *effective_url = NULL;
                        curl_easy_getinfo(o->curl, CURLINFO_EFFECTIVE_URL, &effective_url);
                    } */
                    curl_entry.insert(CURLAllowSet::value_type(o->curl, o));
                }
                curl_multi_add_handle(curlm, o->curl);
            }
        }

        CURLMcode mc = curl_multi_perform(curlm, &still_running);
        if (mc == CURLM_OK) mc = curl_multi_wait(curlm, NULL, 0, CURL_TIMEOUT, &r);
        if (mc != CURLM_OK) _WARN("%d: curlm - %s", __LINE__, curl_multi_strerror(mc))
        else if (still_running == 0)
        {
            int msgs_in_queue;
            CURLMsg *m;

            while ((m = curl_multi_info_read(curlm, &msgs_in_queue)) != NULL)
                if (m->msg != CURLMSG_DONE)
                    _WARN("%d: curlm - %d", __LINE__, m->msg)
                else {
                    CURL *curl = m->easy_handle;
                    char *effective_url = NULL;
                    int   status = 0;

                    curl_multi_remove_handle(curlm, curl);
                    {
                        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
                        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
                    }

                    /* */
                    {
                        CURLAllowSet::iterator c_it = curl_entry.find(curl);

                        /*
                         * CALLBACK_ID: {
                         *    "url": "",
                         *    "respcode": 200,
                         *    "data": ""
                         * }
                         */
                        if (c_it == curl_entry.end())
                            curl_easy_cleanup(curl);
                        else {
                            _CURL *o = c_it->second;

                            if (o->r.callback_id.empty() == false)
                            {
                                bson_scope b_o;

                                bson_append_start_object(&b_o, o->r.callback_id.c_str()); {
                                    bson_append_string(&b_o, "url", o->url.c_str());
                                    {
                                        char *redirect_url = NULL;

                                        curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &redirect_url);
                                        if (redirect_url != NULL)
                                            bson_append_string(&b_o, "redirect_url", redirect_url);
                                    }
                                    if (effective_url != NULL)
                                        bson_append_string(&b_o, "effective_url", effective_url);
                                    bson_append_int(&b_o, "status", status);

                                    {
                                        struct curl_slist *cookies = NULL;
                                        CURLcode r = curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies);
                                        if (r != CURLE_OK) _WARN("'%s': %s", effective_url, curl_easy_strerror(r))
                                        else if (cookies != NULL)
                                        {
                                            int x__i = 0;

                                            bson_append_start_array(&b_o, "cookie");
                                            for (struct curl_slist *each = cookies; each != NULL; each = each->next)
                                                bson_append_string(&b_o, std::format("#%d", ++x__i).c_str(), each->data);
                                            bson_append_finish_array(&b_o);

                                            curl_slist_free_all(cookies);
                                        }
                                    }

                                    bson_append_start_object(&b_o, "data"); {
                                        char  *content_type = NULL;
                                        double length = 0.0, speed = 0.0;

                                        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
                                        curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &length);
                                        curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD, &speed);

                                        if (content_type == NULL)
                                            /* bson_append_string(&out, "type", "text/plain") */;
                                        else {
                                            char *p = strchr(content_type, ';');
                                            if (p == NULL)
                                                bson_append_string(&b_o, "type", content_type);
                                            else {
                                                std::string t(content_type, p - content_type);

                                                while (*(++p) == ' ')
                                                    ;
                                                {
                                                    std::string c_p(p);
                                                    if ((p = strchr((char *)c_p.c_str(), '=')) != NULL)
                                                    {
                                                        *p++ = 0;
                                                        bson_append_string(&b_o, c_p.c_str(), p);
                                                    }
                                                }
                                                bson_append_string(&b_o, "type", t.c_str());
                                            }
                                        }
                                        bson_append_long(&b_o, "length", (int64_t)length);
                                        bson_append_double(&b_o, "bps", speed);
                                        if (o->r.data.buffer != NULL)
                                        {
                                            auto bson_append_gzip = [](bson *b_o, const char *k, const char *i__buffer, size_t i__length)
                                            {
                                                z_stream strm;

                                                strm.zalloc = Z_NULL;
                                                strm.zfree = Z_NULL;
                                                strm.opaque = Z_NULL;
#define windowBits              15
#define GZIP_ENCODING           16
                                                if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                                                                 windowBits | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY) < 0)
_gC:                                                bson_append_string_n(b_o, k, (const char *)i__buffer, i__length);
                                                else {
                                                    std::basic_string<unsigned char> o__buffer;

#define ZIP_CHUNK               0x4000
                                                    unsigned char x__out[ZIP_CHUNK];

                                                    strm.next_in = (unsigned char *)i__buffer;
                                                    strm.avail_in = i__length;
                                                    do {
                                                        strm.avail_out = ZIP_CHUNK;
                                                        strm.next_out = x__out;
                                                        if (deflate(&strm, Z_FINISH) < 0)
                                                            goto _gC;
                                                        else {
                                                            o__buffer.append(x__out, ZIP_CHUNK - strm.avail_out);
                                                        }
                                                    } while (strm.avail_out == 0);
                                                    bson_append_binary(b_o, k, BSON_BIN_USER - BSON_STRING,
                                                            (const char *)o__buffer.data(), o__buffer.length());
                                                }
                                                deflateEnd(&strm);
                                            };

                                            if ((content_type == NULL) ||
                                                    strstr(content_type, "text/") || strstr(content_type, "/json"))
                                                bson_append_gzip(&b_o, "content", o->r.data.buffer, o->r.data.length);
                                            else {
                                                bson_append_binary(&b_o, "content",
                                                                   BSON_BIN_BINARY, o->r.data.buffer, o->r.data.length);
                                            }
                                        }

                                        bson_append_int(&b_o, "result", m->data.result);
                                        bson_append_string(&b_o, "message", curl_easy_strerror(m->data.result));
                                        {
                                            double total_time = 0.0;

                                            if (curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time) == CURLE_OK)
                                                bson_append_double(&b_o, "time", total_time);
                                            _INFO("CURL '%s' %g sec => %d: '%s'", effective_url,
                                                    total_time, m->data.result, curl_easy_strerror(m->data.result))
                                        }
                                    } bson_append_finish_object(&b_o);

                                    if (o->r.type != BSON_UNDEFINED)
                                        bson_append_value(&b_o, "extra",
                                                o->r.type, (char *)o->r.value.data(), o->r.value.length());
                                } bson_append_finish_object(&b_o);

                                bson_finish(&b_o);
                                if (stage_signal(0, NULL, &b_o, 0, &o->r.addr) < 0)
                                    _WARN("%d: %s", __LINE__, strerror(errno));
                            }

                            o->Unref();
                            curl_entry.erase(c_it);
                        }
                    }
                }
        }
    }
#undef CURL_TIMEOUT
    _WARN("CURL: BREAK");
}
/* */
static pthread_once_t CURL__initOnce = PTHREAD_ONCE_INIT;

static void CURL__atExit() { ESYS->Unref(); }
static void CURL__atInit()
{
    try {
	    (ESYS = new CURL__SYS())->Ref();
    } catch (...) { }
    atexit(CURL__atExit);
}


static void dump(std::ostringstream &oss, const char *text, unsigned char *ptr, size_t size)
{
    unsigned int width = 0x10;

    oss << std::format("%s, %.10ld bytes (0x%8.8lx)\n", text, (long)size, (long)size);
    for (size_t i = 0; i < size; i += width)
    {
        oss << std::format("%4.4lx: ", (long)i);

        /* show hex to the left */
        for (size_t c = 0; c < width; c++)
        {
            if ((i + c) < size)
                oss << std::format("%02x ", ptr[i + c]);
            else
                oss << "   ";
        }

        /* show data on the right */
        for (size_t c = 0; (c < width) && ((i + c) < size); c++)
            oss << (char)(((ptr[i + c] >= 0x20) && (ptr[i + c] < 0x80)) ? ptr[i + c] : '.');

        oss << '\n'; /* newline */
    }
}


static int log4cxx__trace(CURL *__x, curl_infotype type, char *data, size_t size, void *userp)
{
    if (type == CURLINFO_TEXT)
        _TRACE("CURL %p == Info: %s", __x, data)
    else {
        const char *text;

        switch (type)
        {
            case CURLINFO_HEADER_OUT  : text = "=> Send header"; goto _gS;
            case CURLINFO_DATA_OUT    : text = "=> Send data"; goto _gS;
            case CURLINFO_SSL_DATA_OUT: text = "=> Send SSL data"; goto _gS;
            case CURLINFO_HEADER_IN   : text = "<= Recv header"; goto _gS;
            case CURLINFO_DATA_IN     : text = "<= Recv data"; goto _gS;
            case CURLINFO_SSL_DATA_IN : text = "<= Recv SSL data";
_gS:        {
                std::ostringstream oss;

                dump(oss, text, (unsigned char *)data, size);
                _TRACE("CURL %p %s", __x, oss.str().c_str());
            }
            default: break;
        }
    }
    return 0;
}


static int messageCallback(utime_t timeBreak, bson_iterator *bson_it, struct sockaddr_stage *addr, CURL__STAGE **__stage)
{
#define IDLE_TIMEOUT              1000
    if (addr == NULL) (*__stage)->Unref();
    else if (bson_it == NULL) return IDLE_TIMEOUT;
    else if (utimeNow(NULL) < timeBreak)
    {
        /*
         * "STAGE_ID:RETURN_ID": {
         *       ...
         * }
         *
         * "STAGE_ID:REQUEST_ID=RETURN_ID": { ... }
         */
        const char *__i = bson_iterator_key(bson_it);

        if (strncmp(__i, (*__stage)->sign.c_str(), (*__stage)->sign.length()) != 0) return 0;
        else if (*(__i += (*__stage)->sign.length()) != ':') return 0;
        {
            _CURL *i = new _CURL(timeBreak, addr);
            if (!(i->curl = curl_easy_init()))
                _WARN("%d: %s", __LINE__, strerror(errno))
            else {
                long timeOut = std::min((*__stage)->timeOut,
                        (i->timeBreak >= INT64_MAX) ? INT64_MAX: (long)(i->timeBreak - utimeNow(NULL)));

                ++__i;
                {
                    const char *p = strchr(__i, '=');

                    i->r.callback_id =  (p == NULL) ? __i: p + 1;
                    if (!i->r.callback_id.empty() &&
                            *(addr->ss_stage + 0) && !strchr(__i + 1, ':'))
                        i->r.callback_id = addr->ss_stage + (":" + i->r.callback_id);
                }

                switch ((int)bson_iterator_type(bson_it))
                {
                    case BSON_OBJECT:
                    {
                        bson_iterator sub_it; bson_iterator_subiterator(bson_it, &sub_it);
                        while (bson_iterator_next(&sub_it) != BSON_EOO)
                        {
                            const char *k = bson_iterator_key(&sub_it);

                            if (strcmp(k, "url") == 0) i->url = bson_iterator_string(&sub_it);
                            else if (strcmp(k, "header") == 0)
                            {
                                bson_iterator header_it;
                                bson_iterator_from_buffer(&header_it, bson_iterator_value(&sub_it));

                                while (bson_iterator_next(&header_it) != BSON_EOO)
                                    i->custom_header = curl_slist_append(i->custom_header, std::format("%s: %s",
                                            bson_iterator_key(&header_it), bson_iterator_string(&header_it)).c_str());

                                curl_easy_setopt(i->curl, CURLOPT_HTTPHEADER, i->custom_header);
                            }
                            else if (strcmp(k, "data") == 0)
                            {
                                switch (bson_iterator_type(&sub_it))
                                {
                                    case BSON_BINDATA:
                                    {
                                        i->postfield.assign(
                                                bson_iterator_bin_data(&sub_it), bson_iterator_bin_len(&sub_it)
                                        );
                                    } break;
                                    case BSON_CODE:
                                    case BSON_STRING: i->postfield = bson_iterator_string(&sub_it); break;
                                    case BSON_OBJECT:
                                    {
                                        bson_iterator post_it;
                                        bson_iterator_from_buffer(&post_it, bson_iterator_value(&sub_it));

                                        for (bson_type type; (type = bson_iterator_next(&post_it)) != BSON_EOO; )
                                        {
                                            if (i->postfield.length() > 0) i->postfield += "&";

                                            i->postfield += curl_escape(bson_iterator_string(&post_it), 0);
                                            i->postfield += "=";
                                            if (type == BSON_STRING) i->postfield += curl_escape(bson_iterator_string(&post_it), 0);
                                            else if (type == BSON_CODE) i->postfield += bson_iterator_code(&post_it);
                                            else if (type == BSON_OBJECT)
                                            {
                                                bson_iterator field_it;
                                                bson_iterator_from_buffer(&field_it, bson_iterator_value(&post_it));

                                                for (int f = 0; (type = bson_iterator_next(&field_it)) != BSON_EOO; f++)
                                                {
                                                    if (f > 0) i->postfield += ",";

                                                    i->postfield += curl_escape(bson_iterator_key(&field_it), 0);
                                                    i->postfield += "-";

                                                    switch (type)
                                                    {
                                                        case BSON_STRING: i->postfield += curl_escape(bson_iterator_string(&field_it), 0); break;
                                                        case BSON_INT: i->postfield += std::format("%d", bson_iterator_int(&field_it)); break;
                                                        default: _WARN("%d: Unsupport value type - '%s'", __LINE__, bson_iterator_key(&field_it));
                                                    }
                                                }
                                            }
                                            else _WARN("%d: Unsupport field type - '%s'", __LINE__, bson_iterator_key(&post_it));
                                        }
                                    } break;
                                    default: _WARN("%d: Unsupport data type - '%s'", __LINE__, bson_iterator_key(&sub_it));
                                }

                                curl_easy_setopt(i->curl, CURLOPT_POST, 1L);

                                curl_easy_setopt(i->curl, CURLOPT_POSTFIELDSIZE, i->postfield.length());
                                curl_easy_setopt(i->curl, CURLOPT_POSTFIELDS, i->postfield.c_str());

                                curl_easy_setopt(i->curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
                            }
                            else if (strcmp(k, "timeout") == 0) timeOut = bson_iterator_int(&sub_it);
                            else if (strcmp(k, "extra") == 0)
                            {
                                i->r.type = bson_iterator_type(&sub_it);
                                i->r.value.assign(bson_iterator_value(&sub_it), bson_iterator_size(&sub_it));
                            }
                        }
                    } break;
                    case BSON_STRING: i->url = bson_iterator_string(bson_it);
                }

                curl_easy_setopt(i->curl, CURLOPT_VERBOSE, (*__stage)->verbose);
                curl_easy_setopt(i->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
                curl_easy_setopt(i->curl, CURLOPT_USERAGENT, (*__stage)->agent.c_str());

                curl_easy_setopt(i->curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(i->curl, CURLOPT_TIMEOUT_MS, timeOut);
                curl_easy_setopt(i->curl, CURLOPT_CONNECTTIMEOUT_MS, std::max(1000L, timeOut / 2L));

                curl_easy_setopt(i->curl, CURLOPT_URL, i->url.c_str());

                curl_easy_setopt(i->curl, CURLOPT_NOSIGNAL, 1L);
                curl_easy_setopt(i->curl, CURLOPT_NOPROGRESS, 1L);

                curl_easy_setopt(i->curl, CURLOPT_DEBUGFUNCTION, log4cxx__trace);
                if (scp_put(ESYS->scp, i->Ref(), 0) < 0)
                    _WARN("%d: %s - '%s'", __LINE__, strerror(errno), i->url.c_str())
                else {
                    return 0;
                }
            }
            i->Unref();
        }
    }
    return 0;
}

};

EXPORT_STAGE(curl)
{
    pthread_once(&curl__stage::CURL__initOnce, curl__stage::CURL__atInit);
    {
        Json::Value &startup = *((Json::Value *)j__config);
        char *__i = (char *)strchr(stage_id, '+');

        {
            curl__stage::CURL__STAGE *stage =
                    new curl__stage::CURL__STAGE(__i ? __i + 1: NULL, startup);

            stage->Ref();
            (*udata) = stage;
        }
    }
    (*r__callback) = (stage_callback )curl__stage::messageCallback;
    return 0;
}