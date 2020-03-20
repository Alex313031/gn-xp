#include <stdio.h>

extern "C" int rust_lib_c_thing(int);

int native_lib_b_thing() {
  return rust_lib_c_thing(12) * 2;
}

extern "C" int callback_into_c() {
  return 7;
}

