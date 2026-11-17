#include "../include/util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int parseInt(const char* s, long min, long max, int* out) {
  if (s == NULL || *s == '\0') return -1;
  char* end = NULL;
  errno = 0;
  long v = strtol(s, &end, 10);
  if (errno != 0 || *end != '\0' || v < min || v > max) return -1;
  *out = (int)v;
  return 0;
}

void printBanner(const char* title) {
  printf("\n=== %s ===\n", title);
}

int ensureDir(const char* path) {
  if (mkdir(path, 0755) == 0 || errno == EEXIST) return 0;
  fprintf(stderr, "error: cannot create directory '%s': %s\n", path, strerror(errno));
  return -1;
}

int readIntFile(const char* path, int* buf, int max, int* count) {
  FILE* fp = fopen(path, "r");
  if (fp == NULL) {
    fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno));
    return -1;
  }

  char line[256];
  int n = 0, lineNo = 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    lineNo++;
    char* hash = strchr(line, '#');
    if (hash != NULL) *hash = '\0';

    char* save = NULL;
    for (char* tok = strtok_r(line, " \t\r\n", &save); tok != NULL;
         tok = strtok_r(NULL, " \t\r\n", &save)) {
      int v;
      if (parseInt(tok, -1000000, 1000000, &v) != 0) {
        fprintf(stderr, "error: %s line %d: '%s' is not a valid integer\n", path, lineNo, tok);
        fclose(fp);
        return -1;
      }
      if (n >= max) {
        fprintf(stderr, "error: %s: too many values (limit %d)\n", path, max);
        fclose(fp);
        return -1;
      }
      buf[n++] = v;
    }
  }
  fclose(fp);
  *count = n;
  return 0;
}

int countLines(const char* path) {
  FILE* fp = fopen(path, "r");
  if (fp == NULL) return -1;
  int lines = 0, c;
  while ((c = fgetc(fp)) != EOF)
    if (c == '\n') lines++;
  fclose(fp);
  return lines;
}
