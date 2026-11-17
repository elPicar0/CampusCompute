#include "../include/logger.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/util.h"

#define SRC_LEN 16
#define MSG_LEN 160

typedef struct {
  time_t ts;
  unsigned long seq;
  char source[SRC_LEN];
  char msg[MSG_LEN];
} LogEntry;

struct Logger {
  FILE* fp;
  LogEntry* buf;
  int cap, head, tail, count;
  int closed, verbose, delayUs;
  unsigned long seq, produced, consumed;
  unsigned long fullWaits, emptyWaits;
  pthread_t consumer;
  pthread_mutex_t lock;
  pthread_cond_t notFull, notEmpty;
};

static void writeEntry(Logger* lg, const LogEntry* e) {
  char stamp[32];
  struct tm tmv;
  localtime_r(&e->ts, &tmv);
  strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);
  fprintf(lg->fp, "%s #%04lu [%s] %s\n", stamp, e->seq, e->source, e->msg);
  fflush(lg->fp);
  if (lg->verbose)
    printf("  [consume] #%04lu wrote [%s] %s\n", e->seq, e->source, e->msg);
}

/* Consumer thread: runs until the logger is closed AND the buffer is empty. */
static void* consumerMain(void* arg) {
  Logger* lg = arg;
  for (;;) {
    LogEntry e;

    pthread_mutex_lock(&lg->lock);
    while (lg->count == 0 && !lg->closed) {          /* buffer empty: wait */
      lg->emptyWaits++;
      pthread_cond_wait(&lg->notEmpty, &lg->lock);
    }
    if (lg->count == 0) {                            /* closed and fully drained */
      pthread_mutex_unlock(&lg->lock);
      break;
    }
    e = lg->buf[lg->head];
    lg->head = (lg->head + 1) % lg->cap;
    lg->count--;
    pthread_cond_signal(&lg->notFull);
    pthread_mutex_unlock(&lg->lock);

    writeEntry(lg, &e);                              /* slow I/O outside the lock */
    lg->consumed++;                                  /* only this thread writes it */
    if (lg->delayUs > 0) usleep((useconds_t)lg->delayUs);
  }
  return NULL;
}

Logger* loggerOpen(const char* path, int capacity, int verbose, int consumerDelayUs) {
  if (path == NULL || capacity < 1) {
    fprintf(stderr, "error: logger needs a file path and capacity >= 1\n");
    return NULL;
  }
  Logger* lg = calloc(1, sizeof(Logger));
  if (lg == NULL) return NULL;

  lg->buf = calloc((size_t)capacity, sizeof(LogEntry));
  lg->fp = fopen(path, "w");
  if (lg->buf == NULL || lg->fp == NULL) {
    fprintf(stderr, "error: cannot open log '%s': %s\n", path, strerror(errno));
    goto fail;
  }
  lg->cap = capacity;
  lg->verbose = verbose;
  lg->delayUs = consumerDelayUs;
  pthread_mutex_init(&lg->lock, NULL);
  pthread_cond_init(&lg->notFull, NULL);
  pthread_cond_init(&lg->notEmpty, NULL);

  int rc = pthread_create(&lg->consumer, NULL, consumerMain, lg);
  if (rc != 0) {
    fprintf(stderr, "error: pthread_create: %s\n", strerror(rc));
    pthread_mutex_destroy(&lg->lock);
    pthread_cond_destroy(&lg->notFull);
    pthread_cond_destroy(&lg->notEmpty);
    goto fail;
  }
  return lg;

fail:
  if (lg->fp) fclose(lg->fp);
  free(lg->buf);
  free(lg);
  return NULL;
}

int loggerLog(Logger* lg, const char* source, const char* fmt, ...) {
  if (lg == NULL) return 0;

  LogEntry e;
  e.ts = time(NULL);
  snprintf(e.source, sizeof(e.source), "%s", source ? source : "?");
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(e.msg, sizeof(e.msg), fmt, ap);
  va_end(ap);

  pthread_mutex_lock(&lg->lock);
  while (lg->count == lg->cap && !lg->closed) {      /* buffer full: wait */
    lg->fullWaits++;
    if (lg->verbose)
      printf("  [produce] %s: buffer FULL (%d/%d), waiting\n", e.source, lg->count, lg->cap);
    pthread_cond_wait(&lg->notFull, &lg->lock);
  }
  if (lg->closed) {
    pthread_mutex_unlock(&lg->lock);
    return -1;
  }
  e.seq = ++lg->seq;
  lg->buf[lg->tail] = e;
  lg->tail = (lg->tail + 1) % lg->cap;
  lg->count++;
  lg->produced++;
  if (lg->verbose)
    printf("  [produce] #%04lu %s: %s   (buffer %d/%d)\n", e.seq, e.source, e.msg, lg->count, lg->cap);
  pthread_cond_signal(&lg->notEmpty);
  pthread_mutex_unlock(&lg->lock);
  return 0;
}

int loggerClose(Logger* lg) {
  if (lg == NULL) return 0;

  pthread_mutex_lock(&lg->lock);
  lg->closed = 1;
  pthread_cond_broadcast(&lg->notEmpty);
  pthread_cond_broadcast(&lg->notFull);
  pthread_mutex_unlock(&lg->lock);

  pthread_join(lg->consumer, NULL);                  /* consumer drains, then exits */

  int rc = (lg->produced == lg->consumed) ? 0 : -1;
  if (lg->verbose) {
    printf("  [logger] consumer thread joined. produced=%lu consumed=%lu "
           "producerFullWaits=%lu consumerEmptyWaits=%lu\n",
           lg->produced, lg->consumed, lg->fullWaits, lg->emptyWaits);
  }
  fclose(lg->fp);
  pthread_mutex_destroy(&lg->lock);
  pthread_cond_destroy(&lg->notFull);
  pthread_cond_destroy(&lg->notEmpty);
  free(lg->buf);
  free(lg);
  return rc;
}

/* ---------- demo ---------- */

typedef struct {
  Logger* lg;
  int id, items;
} ProducerArg;

static void* producerMain(void* arg) {
  ProducerArg* a = arg;
  char src[SRC_LEN];
  snprintf(src, sizeof(src), "P%d", a->id);
  for (int k = 1; k <= a->items; k++) {
    loggerLog(a->lg, src, "job-event %d of %d from producer %d", k, a->items, a->id);
    usleep(2000);
  }
  return NULL;
}

int runLoggerDemo(int producers, int items, int capacity, const char* logPath) {
  if (ensureDir("logs") != 0) return -1;

  printf("Logger demo: %d producer threads x %d events, buffer capacity %d, log file %s\n",
         producers, items, capacity, logPath);
  Logger* lg = loggerOpen(logPath, capacity, 1, 30000);   /* slow consumer -> buffer fills */
  if (lg == NULL) return -1;

  pthread_t tid[16];
  ProducerArg args[16];
  int started = 0;
  for (int i = 0; i < producers; i++) {
    args[i] = (ProducerArg){lg, i + 1, items};
    int rc = pthread_create(&tid[i], NULL, producerMain, &args[i]);
    if (rc != 0) {
      fprintf(stderr, "error: pthread_create: %s\n", strerror(rc));
      break;
    }
    started++;
  }
  for (int i = 0; i < started; i++) pthread_join(tid[i], NULL);
  printf("  [logger] all %d producer threads joined\n", started);

  int rc = loggerClose(lg);
  int lines = countLines(logPath);
  int expected = producers * items;
  printf("Log file '%s' has %d lines (expected %d) -> %s\n", logPath, lines, expected,
         (rc == 0 && lines == expected && started == producers) ? "OK" : "MISMATCH");
  return (rc == 0 && lines == expected && started == producers) ? 0 : -1;
}
