#include <stdio.h>

extern "C" int rust_lib_a_thing(int);

int native_lib_a_thing() {
  return rust_lib_a_thing(45) * 2;
}
