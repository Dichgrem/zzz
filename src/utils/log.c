#include "utils/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

static int use_color(void) {
  static int checked = 0;
  static int color = 1;
  if (!checked) {
    checked = 1;
    if (!isatty(fileno(stdout)))
      color = 0;
    if (getenv("NO_COLOR"))
      color = 0;
  }
  return color;
}

void log_message(LogLevel level, const char *message, ...) {
  char time_buffer[20];
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);

  // Format the timestamp
  strftime(time_buffer, sizeof(time_buffer), "%Y/%m/%d %H:%M:%S", tm_info);

  // Select log level color and label
  const char *level_str;
  const char *level_color;
  const char *dimmed_color;
  const char *reset_color;
  FILE *output_stream;

  int color = use_color();

  switch (level) {
  case LOG_LEVEL_INFO:
    level_str = "INFO";
    level_color = color ? INFO_COLOR : "";
    output_stream = stdout;
    break;
  case LOG_LEVEL_WARN:
    level_str = "WARN";
    level_color = color ? WARN_COLOR : "";
    output_stream = stderr;
    break;
  case LOG_LEVEL_ERROR:
    level_str = "ERRO";
    level_color = color ? ERROR_COLOR : "";
    output_stream = stderr;
    break;
  default:
    level_str = "UNKW";
    level_color = "";
    output_stream = stderr;
    break;
  }

  dimmed_color = color ? DIMMED_COLOR : "";
  reset_color = color ? RESET_COLOR : "";

  // Print timestamp and level
  fprintf(output_stream, "%s %s%s%s %s", time_buffer, level_color, level_str,
          reset_color, message);

  // Process variadic arguments
  va_list args;
  va_start(args, message);
  while (1) {
    const char *key = va_arg(args, const char *);
    if (key == NULL)
      break;

    const int value = va_arg(args, const int);
    if (value < 0 || value > 255)
      break;
    fprintf(output_stream, " %s%s=%s%d", dimmed_color, key, reset_color, value);
  }
  va_end(args);

  fprintf(output_stream, "\n");
}