#pragma once

#include <usr/vex/feed.h>

#define FEED_ALIGN       __k_FEED_ALIGN
#define FEED_TYPE_LOST   __k_FEED_TYPE_LOST
#define FEED_TYPE_DEF(n) __k_FEED_TYPE_DEF(n)

typedef struct __k_feed_header feed_header_t;
typedef struct __k_feed_lost feed_lost_t;

static_assert(sizeof(feed_header_t) % FEED_ALIGN == 0);
static_assert(sizeof(feed_lost_t) % FEED_ALIGN == 0);
