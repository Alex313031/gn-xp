#include <stdio.h>

int native_lib_a_thing();
int native_lib_b_thing();

int main(void) {
  int x = native_lib_a_thing();
  int y = native_lib_b_thing();
  printf("product is %d\n", x*y);
}
