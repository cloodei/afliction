#include "http_target.hpp"

#include <cstdlib>

void bug_null_deref() {
  volatile int *ptr = nullptr;
  *ptr = 1337;
}

void bug_divide_by_zero() {
  volatile int zero = 0;
  volatile int value = 99 / zero;
  (void)value;
}

void bug_abort() { std::abort(); }

void bug_bad_function_pointer() {
  using Fn = void (*)();
  Fn fn = reinterpret_cast<Fn>(0x41);
  fn();
}

void bug_oob_stack_write() {
  volatile char buf[8] = {0};
  for (int i = 0; i < 64; ++i) {
    const_cast<char &>(buf[i]) = static_cast<char>(i);
  }
}

void bug_use_after_free() {
  int *ptr = new int[4];
  delete[] ptr;
  ptr[32] = 7;
  volatile int *null_ptr = nullptr;
  *null_ptr = ptr[32];
}
