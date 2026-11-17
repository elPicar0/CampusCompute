#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define Q_SIZE 5                 // bounded buffer capacity
#define MESSAGE_SIZE 128
#define NUM_PRODUCERS 3
#define MSGS_PER_PRODUCER 8
#define TOTAL_MESSAGES (NUM_PRODUCERS * MSGS_PER_PRODUCER)
#define LOG_FILE "campus.log"
#define CONSUMER_DELAY_US 15000  // slow writer so the buffer visibly fills up

#endif
