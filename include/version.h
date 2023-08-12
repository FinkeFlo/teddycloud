#ifndef _VERSION_H
#define _VERSION_H

#define BUILD_NAME "TeddyCloud"

#ifndef BUILD_VERSION
#define BUILD_VERSION "vX.X.X"
#endif

#ifndef BUILD_GIT_SHORT_SHA
#define BUILD_GIT_SHORT_SHA "00000000"
#endif

#ifndef BUILD_GIT_SHA
#define BUILD_GIT_SHA "0000000000000000000000000000000000000000"
#endif

#ifndef BUILD_GIT_TAG
#define BUILD_GIT_TAG "unknown"
#endif

#ifndef BUILD_GIT_DATETIME
#define BUILD_GIT_DATETIME "1970-01-01 00:00:00 +0000"
#endif

#if BUILD_GIT_IS_DIRTY == 1
#define BUILD_GIT_DIRTY "-dirty"
#define BUILD_DATETIME BUILD_RAW_DATETIME
#else
#define BUILD_GIT_DIRTY ""
#define BUILD_DATETIME BUILD_GIT_DATETIME
#endif

#define BUILD_FULL_NAME_SHORT BUILD_NAME " " BUILD_VERSION
#define BUILD_FULL_NAME_LONG BUILD_FULL_NAME_SHORT " (" BUILD_GIT_SHORT_SHA BUILD_GIT_DIRTY ") - " BUILD_DATETIME
#define BUILD_FULL_NAME_FULL BUILD_FULL_NAME_SHORT " (" BUILD_GIT_SHA BUILD_GIT_DIRTY ") - " BUILD_DATETIME

#endif