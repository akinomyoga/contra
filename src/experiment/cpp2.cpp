#include <cstdio>

// C++14 では undefined reference になってしまう
// C++17 では実態を定義しなくても大丈夫

struct hoge {
  static constexpr int value = 1;
};

template<typename T>
void func(T const&) {}

int main() {
  func(hoge::value);
  return 0;
}
