#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "core/net.h"

int urldecode(char *dest, const char *src);
bool queryGet(const char *query, const char *key, char *data, size_t data_len);
char_t *ipAddrToString(const IpAddr *ipAddr, char_t *str);
char_t *ipv6AddrToString(const Ipv6Addr *ipAddr, char_t *str);
char_t *ipv4AddrToString(Ipv4Addr ipAddr, char_t *str);
