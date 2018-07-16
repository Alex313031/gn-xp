#include <stdio.h>

#include "foo/foo.h"
#include "foo/foo_internal.h"

void foo() {
  printf("foo\n");
  foo_internal();
}
