// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_UTIL_ZOS_UTIL_H_
#define SRC_UTIL_ZOS_UTIL_H_

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/sem.h>

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  unsigned int value;
} sem_t;

int sem_init(sem_t *semid, int pshared, unsigned int value);
int sem_destroy(sem_t *semid);
int sem_wait(sem_t *semid);
int sem_post(sem_t *semid);

int getexepath(char *exepath, size_t buflen, pid_t pid);

#endif  // SRC_UTIL_ZOS_UTIL_H_
