#include "../include/logger.h"
#include "../include/campus.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

// bounded circular buffer shared by the producer threads and the consumer thread
static char messages[Q_SIZE][MESSAGE_SIZE];
static unsigned long seqs[Q_SIZE];

static int head = 0;
static int tail = 0;
static int count = 0;
static int producersRemaining = 0;
static unsigned long seq = 0;
static unsigned long fullWaits = 0;
static unsigned long emptyWaits = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t notFull = PTHREAD_COND_INITIALIZER;
pthread_cond_t notEmpty = PTHREAD_COND_INITIALIZER;

// add one message; blocks while the buffer is full
static void produce(const char* message) {
  pthread_mutex_lock(&mutex);

  while (count == Q_SIZE) {
    fullWaits++;
    printf("  [produce] buffer FULL (%d/%d), producer waiting\n", count, Q_SIZE);
    pthread_cond_wait(&notFull, &mutex);
  }

  snprintf(messages[tail], MESSAGE_SIZE, "%s", message);
  seqs[tail] = ++seq;
  tail = (tail + 1) % Q_SIZE;
  count++;

  printf("  [produce] #%04lu %s  (buffer %d/%d)\n", seq, message, count, Q_SIZE);

  pthread_cond_signal(&notEmpty);
  pthread_mutex_unlock(&mutex);
}

// remove one message; blocks while the buffer is empty.
// Returns 0 once all producers are finished and the buffer is drained.
static int consume(char* buf, size_t size, unsigned long* seqOut) {
  pthread_mutex_lock(&mutex);

  while (count == 0 && producersRemaining > 0) {
    emptyWaits++;
    pthread_cond_wait(&notEmpty, &mutex);
  }

  if (count == 0) {
    pthread_mutex_unlock(&mutex);
    return 0;
  }

  snprintf(buf, size, "%s", messages[head]);
  *seqOut = seqs[head];
  head = (head + 1) % Q_SIZE;
  count--;

  pthread_cond_signal(&notFull);
  pthread_mutex_unlock(&mutex);

  return 1;
}

static void* produceThread(void* arg) {
  int id = (int)(intptr_t)arg;
  char buffer[MESSAGE_SIZE];

  for (int i = 1; i <= MSGS_PER_PRODUCER; i++) {
    snprintf(buffer, sizeof(buffer), "[P%d] job %d completed successfully", id, i);
    produce(buffer);
    usleep(1000);
  }

  pthread_mutex_lock(&mutex);
  producersRemaining--;
  if (producersRemaining <= 0) pthread_cond_broadcast(&notEmpty);   // wake the consumer to finish
  pthread_mutex_unlock(&mutex);

  printf("  [producer %d] finished\n", id);
  return NULL;
}

static void* consumeThread(void* arg) {
  FILE* file = (FILE*)arg;
  char buffer[MESSAGE_SIZE];
  unsigned long n;

  while (consume(buffer, sizeof(buffer), &n)) {
    char stamp[32];
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);

    fprintf(file, "%s #%04lu %s\n", stamp, n, buffer);
    fflush(file);
    printf("  [consume] #%04lu written to %s\n", n, LOG_FILE);
    usleep(CONSUMER_DELAY_US);
  }

  printf("  [consumer] buffer drained, finished\n");
  return NULL;
}

static int countLines(const char* path) {
  FILE* fp = fopen(path, "r");
  if (fp == NULL) return -1;
  int lines = 0, c;
  while ((c = fgetc(fp)) != EOF)
    if (c == '\n') lines++;
  fclose(fp);
  return lines;
}

int runLogger(void) {
  pthread_t producers[NUM_PRODUCERS];
  pthread_t consumer;

  FILE* fp = fopen(LOG_FILE, "w");
  if (fp == NULL) {
    perror("fopen");
    return 1;
  }

  head = 0;
  tail = 0;
  count = 0;
  seq = 0;
  fullWaits = 0;
  emptyWaits = 0;
  producersRemaining = NUM_PRODUCERS;

  printf("%d producer threads x %d messages, buffer capacity %d, log file %s\n",
      NUM_PRODUCERS, MSGS_PER_PRODUCER, Q_SIZE, LOG_FILE);

  int rc = pthread_create(&consumer, NULL, consumeThread, fp);
  if (rc != 0) {
    fprintf(stderr, "error: cannot create consumer thread: %s\n", strerror(rc));
    fclose(fp);
    return 1;
  }

  int started = 0;
  for (int i = 0; i < NUM_PRODUCERS; i++) {
    rc = pthread_create(&producers[i], NULL, produceThread, (void*)(intptr_t)(i + 1));
    if (rc != 0) {
      fprintf(stderr, "error: cannot create producer thread %d: %s\n", i + 1, strerror(rc));
      break;
    }
    started++;
  }

  if (started < NUM_PRODUCERS) {               // producers that never started will never finish
    pthread_mutex_lock(&mutex);
    producersRemaining -= NUM_PRODUCERS - started;
    if (producersRemaining <= 0) pthread_cond_broadcast(&notEmpty);
    pthread_mutex_unlock(&mutex);
  }

  for (int i = 0; i < started; i++) pthread_join(producers[i], NULL);
  pthread_join(consumer, NULL);                // consumer exits after draining the buffer

  if (fclose(fp) != 0) {
    perror("fclose");
    return 1;
  }

  int lines = countLines(LOG_FILE);
  int expected = started * MSGS_PER_PRODUCER;
  printf("All %d producer threads and the consumer thread terminated cleanly\n", started);
  printf("Buffer-full waits: %lu, buffer-empty waits: %lu\n", fullWaits, emptyWaits);
  printf("%s has %d lines (expected %d) -> %s\n", LOG_FILE, lines, expected,
      lines == expected ? "OK" : "MISMATCH");

  return (started == NUM_PRODUCERS && lines == expected) ? 0 : 1;
}

int main(void) {
  int result = runLogger();

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&notFull);
  pthread_cond_destroy(&notEmpty);

  return result;
}
