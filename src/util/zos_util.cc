///////////////////////////////////////////////////////////////////////////////
// Licensed Materials - Property of IBM
// (C) Copyright IBM Corp. 2021. All Rights Reserved.
// US Government Users Restricted Rights - Use, duplication
// or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
///////////////////////////////////////////////////////////////////////////////

#include "util/zos_util.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

// Thread entry constants for getexepath():
#define PGTH_CURRENT  1
#define PGTHAPATH     0x20

// semaphore functions:

int sem_init(sem_t *sem, int pshared, unsigned int value) {
  int err;
  if (sem == NULL)
    return ENOMEM;

  if ((err = pthread_mutex_init(&sem->mutex, NULL)) != 0) {
    errno = err;
    return -1;
  }

  if ((err = pthread_cond_init(&sem->cond, NULL)) != 0) {
    if (pthread_mutex_destroy(&sem->mutex))
      abort();
    errno = err;
    return -1;
  }
  sem->value = value;
  return 0;
}

int sem_destroy(sem_t *sem) {
  if (pthread_cond_destroy(&sem->cond))
    abort();

  if (pthread_mutex_destroy(&sem->mutex))
    abort();
  return 0;
}

int sem_wait(sem_t *sem) {
  if (pthread_mutex_lock(&sem->mutex))
    abort();

  while (sem->value == 0) {
    if (pthread_cond_wait(&sem->cond, &sem->mutex))
      abort();
  }
  sem->value--;

  if (pthread_mutex_unlock(&sem->mutex))
    abort();

  return 0;
}

int sem_post(sem_t *sem) {
  if (pthread_mutex_lock(&sem->mutex))
    abort();

  sem->value++;

  if (sem->value == 1) {
    if (pthread_cond_signal(&sem->cond))
      abort();
  }
  if (pthread_mutex_unlock(&sem->mutex))
    abort();

  return 0;
}

// getexepath() support functions:

#if 0

// TODO(gabylb) - zos: enable this when Woz compiler supports OS linkage.
#pragma linkage(BPX4GTH,OS)
void BPX4GTH(int *input_length, void **input_address,
             int *output_length, void **output_address,
             int *return_value, int *return_code, int *reason_code);

#else

static char *__ptr32 *__ptr32 __base(void) {
  // System control offsets to callable services are listed in:
  // https://www.ibm.com/docs/en/zos/2.3.0?topic=scocs-list-offsets
  //
  // To invoke the BPX4GTH (__getthent()) callable service (to get
  // a given process exe path), we need the base address of the
  // system control offsets.
  //
  // The following offsets are from the MVS Data Areas Volumes 1 & 3:
  // Volume 1: https://www-01.ibm.com/servers/resourcelink/svc00100.nsf/pags/
  //           zosv2r3ga320935/$file/iead100_v2r3.pdf
  // Volume 3: https://www-01.ibm.com/servers/resourcelink/svc00100.nsf/pages
  //           zosv2r3ga320938/$file/iead300_v2r3.pdf
  //
  // In Vol 1, CVTCSRT (Callable Service Request Table) is at decimal offset
  // 544 in CVT (Vol 1), which is at decimal offset 16 in PSA (Vol 3),
  // which is at address 0.
  //
  // The offsets base is at decimal offset 24 (see
  // https://www.ibm.com/docs/en/zos/2.3.0?topic=scocs-example-1)
  //
  // So, with the 32-bit pointer casts, the base address is at
  // 0[16/4][544/4][24/4], which is 0[4][136][6].
  //
  // Note: z/OS is always upward compatible, so offsets below won't change.

  static char *__ptr32 *__ptr32 res = 0;
  if (res == 0) {
    // PSA -> CVT -> CVTCSRT -> CSR
    res = ((char *__ptr32 *__ptr32 *__ptr32 *__ptr32 *)0)[4][136][6];
  }
  return res;
}

static void BPX4GTH(int *input_length, void **input_address,
                    int *output_length, void **output_address,
                    int *return_value, int *return_code, int *reason_code) {
  //
  // BPX4GTH is __getthent() callable service to get thread data; see
  // https://www.ibm.com/docs/en/zos/2.2.0?
  //         topic=descriptions-getthent-bpx1gth-bpx4gth-get-thread-data
  //
  // BPX[1|4]GTH offset is 1056, as specified in:
  // https://www.ibm.com/docs/en/zos/2.4.0?topic=scocs-list-offsets
  //
  void *reg15 = __base()[1056 / 4];

  void *argv[] = {input_length, input_address,
                  output_length, output_address,
                  return_value, return_code, reason_code};

  __asm volatile(" basr 14,%0\n"
                 : "+NR:r15"(reg15)
                 : "NR:r1"(&argv)
                 : "r0", "r14");
}

#endif  // #if 0

int getexepath(char *exepath, size_t buflen, pid_t pid) {
  // Get the exe path using the thread entry information in the address space.
  // See https://www.ibm.com/docs/en/zos/2.4.0?
  //             topic=31-bpxypgth-map-getthent-inputoutput-structure
  // for the structs mapping below.

#pragma pack(1)
  struct {
    int pid;
    unsigned long thid;
    char accesspid;
    char accessthid;
    char asid[2];
    char loginname[8];
    char flag;
    char len;
  } input_data;

  union {
    struct {
      char gthb[4];
      int pid;
      unsigned long thid;
      char accesspid;
      char accessthid[3];
      int lenused;
      int offsetProcess;
      int offsetConTTY;
      int offsetPath;
      int offsetCommand;
      int offsetFileData;
      int offsetThread;
    } output_data;
    char buf[PATH_MAX];
  } output_buf;

  struct output_path_type {
    char gthe[4];
    short int len;
    char path[PATH_MAX];
  };
#pragma pack()

  int input_length;
  int output_length;
  void* input_address;
  void* output_address;
  struct output_path_type* output_path;
  int rv;
  int rc;
  int rsn;

  input_length = sizeof(input_data);
  output_length = sizeof(output_buf);
  output_address = &output_buf;
  input_address = &input_data;
  memset(&input_data, 0, sizeof(input_data));
  input_data.flag = PGTHAPATH;
  input_data.pid = pid;
  input_data.accesspid = PGTH_CURRENT;

  BPX4GTH(&input_length, &input_address,
          &output_length, &output_address,
          &rv, &rc, &rsn); 

  if (rv == -1) {
    errno = rc;
    return -1;
  }

  // Check first byte (PGTHBLIMITE): A = the section was completely filled in
  __e2a_l((char*)&output_buf.output_data.offsetPath, 1);
  assert(((output_buf.output_data.offsetPath >>24) & 0xFF) == 'A');

  // Path offset is in the lowest 3 bytes (PGTHBOFFE):
  output_path = (struct output_path_type*) ((char*) (&output_buf) +
      (output_buf.output_data.offsetPath & 0x00FFFFFF));

  if (output_path->len >= (short int)buflen) {
    errno = EBUFLEN;
    return -1;
  }
  __e2a_l(output_path->path, output_path->len);

  strncpy(exepath, output_path->path, output_path->len);
  return 0;
}
