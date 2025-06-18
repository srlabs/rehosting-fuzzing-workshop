#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *nvram_get(char *param_1) {
  const char debug_cprintf_file[] = "debug_cprintf_file";
  const char http_enable[] = "http_enable";
  const char x_Setting[] = "x_Setting";
  const char HTTPD_DBG[] = "HTTPD_DBG";

  // printf("nvram_get: %s\n", param_1);

  if (memcmp(param_1, http_enable, sizeof(http_enable)) == 0) {
    const char buf[] = "0";
    char *ret = (void *)malloc(sizeof(buf));
    memcpy(ret, buf, sizeof(buf));
    return ret;
  }

  if (memcmp(param_1, debug_cprintf_file, sizeof(debug_cprintf_file)) == 0 ||
      memcmp(param_1, x_Setting, sizeof(x_Setting)) == 0
      // memcmp(param_1, HTTPD_DBG, sizeof(HTTPD_DBG)) == 0
  ) {
    const char buf[] = "1";
    char *ret = (void *)malloc(sizeof(buf));
    memcpy(ret, buf, sizeof(buf));
    return ret;
  }

  return (char *)0;
}

int nvram_set(char *param_1, char *param_2, unsigned int param_3) {
  // printf("nvram_set: %s %s %d\n", param_1, param_2, param_3);
  return 0;
}

int nvram_unset(char *param_1) {
  // printf("nvram_unset: %s\n", param_1);
  return 1;
}

void notify_rc_and_wait(char *buf) { return; }
