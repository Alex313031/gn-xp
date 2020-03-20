#include <stdio.h>

extern "C" int base_rust_call(int);

int base_c_call() {
  return base_rust_call(45) * 2;
}
