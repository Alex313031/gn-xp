#include <stdio.h>

int base_c_call();
int native_lib_a_thing();

int main(void) {
  int x = base_c_call();
  int y = native_lib_a_thing();
  printf("x is %d\n", x*y);
}
