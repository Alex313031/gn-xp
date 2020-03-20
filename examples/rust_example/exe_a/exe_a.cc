#include <stdio.h>

int native_lib_a_thing();

int main(void) {
  int x = native_lib_a_thing();
  printf("x is %d\n", x);
}
