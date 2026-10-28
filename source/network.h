#pragma once

struct ifaddrs;

extern volatile int g_net_ready;

int net_getifaddrs(struct ifaddrs **out);
void net_freeifaddrs(struct ifaddrs *list);
