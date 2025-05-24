/* desocket library by Marc "vanHauser" Heuse <vh@thc.org> */

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void *handle;
static bool debug, running;
static int listen_fd = -1;

struct myin_addr {
  unsigned int s_addr; // IPv4 address in network byte order
};

struct mysockaddr {
  unsigned short int sin_family; // Address family: AF_INET
  unsigned short int sin_port;   // Port number (network byte order)
  struct myin_addr sin_addr;     // Internet address
  char sin_zero[8];              // Padding (unused)
};

#define RTLD_LAZY 0x00001

unsigned short int htons(unsigned short int hostshort) {
  return (hostshort << 8) | (hostshort >> 8);
}

static void __get_handle() {

  if (!(handle = dlopen("libc.so", RTLD_NOW))) {
    if (!(handle = dlopen("libc.so.6", RTLD_NOW))) {
      if (!(handle = dlopen("libc-orig.so", RTLD_LAZY))) {
        if (!(handle = dlopen("cygwin1.dll", RTLD_LAZY))) {
          if (!(handle = dlopen("libc.so", RTLD_NOW))) {
            fprintf(stderr, "DESOCK: can not find libc!\n");
            exit(-1);
          }
        }
      }
    }
  }

  if (getenv("DESOCK_DEBUG")) {
    debug = true;
  }
}

int shutdown(int socket, int how) {
  _exit(0);
}

int (*o_accept)(int sockfd, struct mysockaddr *addr,
                unsigned long int *addrlen);
int accept(int sockfd, struct mysockaddr *addr, unsigned long int *addrlen) {
  if (!handle) {
    __get_handle();
  }
  if (!o_accept) {
    o_accept = dlsym(handle, "accept");
  }
  if (!running && sockfd == listen_fd) {
    if (debug)
      fprintf(stderr, "DESOCK: intercepted accept on %d\n", sockfd);
    if (addr && addrlen) {
      // we need to fill this!
      memset(addr, 0, *addrlen);
      addr->sin_family = 2;         // AF_INET
      addr->sin_port = htons(1023); // Port 1023 in network byte order
      addr->sin_addr.s_addr = 0x0100007f;
    }
    running = true;
    return 0;
  }
  return o_accept(sockfd, addr, addrlen);
}

int accept4(int sockfd, struct mysockaddr *addr, unsigned long int *addrlen,
            int flags) {
  return accept(sockfd, addr, addrlen); // ignore flags
}

int (*o_listen)(int sockfd, int backlog);
int listen(int sockfd, int backlog) {
  if (!handle) {
    __get_handle();
  }
  if (!o_listen) {
    o_listen = dlsym(handle, "listen");
  }
  if (sockfd == listen_fd) {
    if (debug)
      fprintf(stderr, "DESOCK: intercepted listen on %d\n", sockfd);
    return 0;
  }
  return o_listen(sockfd, backlog);
}

int (*o_bind)(int sockfd, const struct mysockaddr *addr,
              unsigned long int addrlen);
int bind(int sockfd, const struct mysockaddr *addr, unsigned long int addrlen) {
  if (!handle) {
    __get_handle();
  }
  if (!o_bind) {
    o_bind = dlsym(handle, "bind");
  }
  if (addr->sin_port == htons(80)) {
    if (debug)
      fprintf(stderr, "DESOCK: intercepted bind on %d\n", sockfd);
    listen_fd = sockfd;
    return 0;
  }
  return o_bind(sockfd, addr, addrlen);
}

int (*o_setsockopt)(int sockfd, int level, int optname, const void *optval,
                    unsigned long int optlen);
int setsockopt(int sockfd, int level, int optname, const void *optval,
               unsigned long int optlen) {
  if (!handle) {
    __get_handle();
  }
  if (!o_setsockopt) {
    o_setsockopt = dlsym(handle, "setsockopt");
  }
  if (listen_fd == sockfd) {
    if (debug)
      fprintf(stderr, "DESOCK: intercepted setsockopt on %d for %d\n", sockfd,
              optname);
    return 0;
  }
  return o_setsockopt(sockfd, level, optname, optval, optlen);
}

int (*o_getsockopt)(int sockfd, int level, int optname, void *optval,
                    unsigned long int optlen);
int getsockopt(int sockfd, int level, int optname, void *optval,
               unsigned long int optlen) {
  if (!handle) {
    __get_handle();
  }
  if (!o_getsockopt) {
    o_getsockopt = dlsym(handle, "getsockopt");
  }
  if (listen_fd == sockfd) {
    if (debug)
      fprintf(stderr, "DESOCK: intercepted getsockopt on %d for %d\n", sockfd,
              optname);
    char *o = (char *)optval;
    if (o != NULL) {
      *o = 1; // let's hope this is fine
    }
    return 0;
  }
  return o_getsockopt(sockfd, level, optname, optval, optlen);
}

int (*o_getpeername)(int sockfd, struct mysockaddr *addr,
                     unsigned long int *addrlen);
int getpeername(int sockfd, struct mysockaddr *addr,
                unsigned long int *addrlen) {
  if (!handle) {
    __get_handle();
  }
  if (!o_getpeername) {
    o_getpeername = dlsym(handle, "getpeername");
  }
  if (sockfd == 0) {
    if (debug)
      fprintf(stderr, "DESOCK: getpeername\n");
    if (addr && addrlen) {
      // we need to fill this!
      memset(addr, 0, *addrlen);
      addr->sin_family = 2;         // AF_INET
      addr->sin_port = htons(1023); // Port 1023 in network byte order
      addr->sin_addr.s_addr = 0x0100007f;
    }
    return 0;
  }
  return o_getpeername(sockfd, addr, addrlen);
}

int (*o_getsockname)(int sockfd, struct mysockaddr *addr,
                     unsigned long int *addrlen);
int getsockname(int sockfd, struct mysockaddr *addr,
                unsigned long int *addrlen) {
  if (!handle) {
    __get_handle();
  }
  if (!o_getsockname) {
    o_getsockname = dlsym(handle, "getsockname");
  }
  if (sockfd == 0) {
    if (debug)
      fprintf(stderr, "DESOCK: getsockname\n");
    if (addr && addrlen) {
      // we need to fill this!
      memset(addr, 0, *addrlen);
      addr->sin_family = 2; // AF_INET
      addr->sin_port = htons(80);
      addr->sin_addr.s_addr = 0x0100007f;
    }
    return 0;
  }
  return o_getsockname(sockfd, addr, addrlen);
}

static FILE *(*o_fdopen)(int fd, const char *mode);
FILE *fdopen(int fd, const char *mode) {

  if (!o_fdopen) {

    if (!handle) { __get_handle; }

    o_fdopen = dlsym(handle, "fdopen");
    if (!o_fdopen) {
      fprintf(stderr, "%s(): can not find fdopen\n", dlerror());
      exit(-1);
    }
  }

  if (fd == 0 && strcmp(mode, "r") != 0) {
    if (debug) fprintf(stderr, "DESOCK: intercepted fdopen(r+) for %d\n", fd);
    return o_fdopen(fd, "r");
  }
  return o_fdopen(fd, mode);
}

/* TARGET SPECIFIC HOOKS */

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
