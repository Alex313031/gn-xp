#include <stdio.h>

#include "foo/foo.h"
// This should fail to compile:
// #include "foo/foo_internal.h"
#include "bar/bar.h"

void bar() {
  foo();
  printf("bar\n");
  // This should fail to compile:
  // foo_internal();
}
