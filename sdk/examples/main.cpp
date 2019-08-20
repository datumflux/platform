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
#include "single.h"
#include <atomic>
#include <pthread.h>

DECLARE_LOGGER("examples");

namespace examples__stage {

struct STAGE {

	STAGE(const char *s, Json::Value &config)
		: sign((s && *(s + 0)) ? s: "examples")
		, ref_(0) { }
	virtual ~STAGE() { }

	virtual STAGE *Ref() { ++ref_; return this; }
	virtual void Unref() { if ((--ref_) <= 0) delete this; }

	std::string sign;
private:
	std::atomic<std::int16_t> ref_;
};


static int messageCallback(utime_t timeBreak,
        bson_iterator *bson_it, struct sockaddr_stage *addr, STAGE **__stage)
{
    if (addr == NULL) (*__stage)->Unref();
    else if (bson_it == NULL)
		fprintf(stderr, " ** IDLE: %lu\n", utimeNow(NULL));
    else if (utimeNow(NULL) < timeBreak)
	{
		const char *__i = bson_iterator_key(bson_it);

        if ((*__stage)->sign.length() == 0);
        else if (strncmp(__i, (*__stage)->sign.c_str(), (*__stage)->sign.length()) != 0) return 0; /* NOT */
        else if (*(__i += (*__stage)->sign.length()) != ':') return 0;
        else ++__i;

		fprintf(stderr, " ** IN: '%s'\n", __i);
	}
	return 1000;
}

static void __atExit() { }
static void __atInit()
{
	atexit(__atExit);
}

static pthread_once_t __initOnce = PTHREAD_ONCE_INIT;
};


EXPORT_STAGE(examples)
{
	pthread_once(&examples__stage::__initOnce, examples__stage::__atInit); {
		Json::Value &config = *((Json::Value *)j__config);
		char *x__i = (char *)strchr(stage_id, '+');
		{
			examples__stage::STAGE *stage = 
				new examples__stage::STAGE(x__i ? x__i + 1: NULL, config);

			stage->Ref();
			(*udata) = stage;
		}
	} (*r__callback) = (stage_callback)examples__stage::messageCallback;
	return 0;
}
