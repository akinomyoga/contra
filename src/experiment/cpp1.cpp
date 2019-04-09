#include <cstdio>

// 参照メンバは CopyAssignable ではない。

struct A {
  int& x;
  A(int& x): x(x) {}
};

int main() {
  int a;
  A c1(a);
  A c2 = c1;
  c2 = c1;
  return 0;
}
