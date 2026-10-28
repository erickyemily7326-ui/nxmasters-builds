#include "network.h"
#include <errno.h>
#include <stddef.h>

volatile int g_net_ready;

int net_getifaddrs(struct ifaddrs **out) {
  if (!out) {
    errno = EINVAL;
    return -1;
  }
  *out = NULL;
  errno = ENOSYS;
  return -1;
}

void net_freeifaddrs(struct ifaddrs *list) {
  (void)list;
}
