#include <cstdio>
#include <type_traits>
#include <utility>

// deduction guide は完全転送してくれるのか?
// →実は deduction guide は型を決めてくれるだけで、
//   実際には直接コンストラクタが呼び出されるので、
//   転送などのことは中では行われていない。
// →従って T const& として推論してもちゃんと
//   rvalue を認識してコンストラクタが選択される。
//   寧ろ T const& とした方が remove_reference する手間が省ける。

template<typename T>
struct identity { using type = T; };

template<typename T>
struct A {
  A(typename identity<T>::type&&) {std::printf("rvalue\n");}
  A(typename identity<T>::type const&) {std::printf("clvalue\n");}
  A(typename identity<T>::type&) {std::printf("lvalue\n");}
};

// template<typename T> A(T&&) -> A<typename std::remove_reference<T>::type>;
template<typename T> A(T const&) -> A<T>;

int main() {
  int value = 1;
  A a(value);
  A b(std::move(value));
  A c(1234);
  return 0;
}
