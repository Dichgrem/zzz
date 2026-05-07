#include "auth.h"
#include "crypto/crypto.h"
#include "packet/packet.h"
#include "packet/send.h"
#include "utils/config.h"
#include "utils/device.h"
#include "utils/log.h"

#include <pcap/pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>

volatile int g_running = 1;
static volatile int g_reconnect = 0;
volatile int g_auth_ok = 0;

static BOOL WINAPI ctrl_handler(DWORD fdwCtrlType) {
  switch (fdwCtrlType) {
  case CTRL_C_EVENT:
  case CTRL_CLOSE_EVENT:
  case CTRL_SHUTDOWN_EVENT:
    g_running = 0;
    return TRUE;
  default:
    return FALSE;
  }
}

static void check_stdin(void) {
  while (_kbhit()) {
    char buf[64];
    if (!fgets(buf, sizeof(buf), stdin))
      break;
    buf[strcspn(buf, "\r\n")] = '\0';
    if (strcmp(buf, "quit") == 0) {
      g_running = 0;
    } else if (strcmp(buf, "reconnect") == 0) {
      g_reconnect = 1;
      g_running = 0;
    }
  }
}

static char *default_config_path(void) {
  static char path[MAX_PATH];
  char *appdata = getenv("APPDATA");
  if (appdata) {
    snprintf(path, sizeof(path), "%s\\zzz\\config.ini", appdata);
  } else {
    snprintf(path, sizeof(path), "config.ini");
  }
  return path;
}
#else
#include <signal.h>
#include <poll.h>
#include <unistd.h>

volatile int g_running = 1;
static volatile int g_reconnect = 0;
volatile int g_auth_ok = 0;

static void sig_handler(int sig) {
  (void)sig;
  g_running = 0;
}

static void check_stdin(void) {
  struct pollfd pfd;
  pfd.fd = STDIN_FILENO;
  pfd.events = POLLIN;
  if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
    char buf[64];
    if (fgets(buf, sizeof(buf), stdin)) {
      buf[strcspn(buf, "\n")] = '\0';
      if (strcmp(buf, "quit") == 0) {
        g_running = 0;
      } else if (strcmp(buf, "reconnect") == 0) {
        g_reconnect = 1;
        g_running = 0;
      }
    }
  }
}
#endif

static void cleanup(void) {
  if (g_device.handle) {
    if (!g_auth_ok)
      send_signoff_packet();
    pcap_close(g_device.handle);
    g_device.handle = NULL;
  }
  log_info("bye!", NULL);
}

static void print_usage(void) {
  printf("Usage:\n"
         "  zzz list\n"
         "  zzz run <device> [--config <path>]\n"
         "  zzz <config_path>                  (legacy)\n");
}

static void cmd_list(void) {
  device_list();
}

static void cmd_run(char *device, char *config_path) {
#ifdef _WIN32
  SetConsoleCtrlHandler(ctrl_handler, TRUE);
  if (!config_path)
    config_path = default_config_path();
#else
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  if (!config_path)
    config_path = "config.ini";
#endif

  if (device) {
    // device from command line overrides config
    config_init(config_path);
    g_config.device = device;
  } else {
    // legacy: device comes from config ini
    config_init(config_path);
  }

  device_init(g_config.device);
  packet_init_default();
  crypto_init();

  log_info("device opened", NULL);
  log_info("authing", NULL);

  while (1) {
    g_running = 1;
    g_reconnect = 0;

    auth_handshake();
    while (g_running) {
      int ret = auth_loop();
      if (ret != 0) {
        log_error("auth loop error", "code", ret);
        g_running = 0;
        break;
      }
      check_stdin();
    }

    if (!g_reconnect)
      break;
    log_info("reconnecting", NULL);
  }

  cleanup();
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage();
    exit(EXIT_FAILURE);
  }

  if (strcmp(argv[1], "list") == 0) {
    cmd_list();
    return 0;
  }

  if (strcmp(argv[1], "run") == 0) {
    if (argc < 3) {
      print_usage();
      exit(EXIT_FAILURE);
    }
    char *device = argv[2];
    char *config_path = NULL;

    for (int i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
        config_path = argv[++i];
      }
    }

    cmd_run(device, config_path);
    return 0;
  }

  // Legacy: zzz <config_path>
  cmd_run(NULL, argv[1]);
  return 0;
}