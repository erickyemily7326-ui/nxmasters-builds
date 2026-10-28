#include "../../source/network.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;

static void check(int condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    exit(1);
  }
  checks++;
}

int main(void) {
  struct ifaddrs *list = (struct ifaddrs *)(uintptr_t)1;
  errno = 0;
  check(net_getifaddrs(&list) == -1 && list == NULL && errno == ENOSYS,
        "unavailable enumeration must fail before the core iterates the list");
  check(net_getifaddrs(NULL) == -1 && errno == EINVAL,
        "null interface output must be rejected");
  errno = EBUSY;
  net_freeifaddrs(NULL);
  check(errno == EBUSY, "freeing a null list must be harmless");
  list = (struct ifaddrs *)(uintptr_t)1;
  check(net_getifaddrs(&list) == -1 && list == NULL && errno == ENOSYS,
        "repeated interface enumeration returned a stale list");
  printf("%u network checks passed\n", checks);
  return 0;
}
