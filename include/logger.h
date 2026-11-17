#ifndef LOGGER_H
#define LOGGER_H

/* Concurrent activity logger (FR3).
 *
 * Producers (any thread) call loggerLog(); a single consumer thread drains a
 * bounded circular buffer and writes to the log file.
 *   - mutex protects the buffer
 *   - notFull  condition variable: producers wait while the buffer is full
 *   - notEmpty condition variable: consumer waits while the buffer is empty
 * loggerClose() sets a "closed" flag, wakes everybody, lets the consumer
 * drain what is left, and joins the thread. */
typedef struct Logger Logger;

/* verbose: echo [produce]/[consume] lines to stdout.
 * consumerDelayUs: artificial consumer slowness (0 = none), used by the demo
 * to make the buffer fill up. Returns NULL on error (message on stderr). */
Logger* loggerOpen(const char* path, int capacity, int verbose, int consumerDelayUs);

/* Thread-safe. A NULL logger is accepted and ignored so that modules can be
 * used with or without logging. Returns 0 on success. */
int loggerLog(Logger* lg, const char* source, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

/* Drains, joins the consumer thread, closes the file, frees the logger.
 * Returns 0 if every produced entry was consumed. */
int loggerClose(Logger* lg);

/* Standalone demo: `producers` threads each log `items` events. Returns 0 if
 * the log file contains exactly producers*items lines. */
int runLoggerDemo(int producers, int items, int capacity, const char* logPath);

#endif
