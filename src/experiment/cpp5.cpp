#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <iomanip>

// 以下の様な処理をする時に実際に変更があったか
// どうかを判定するにはどうするのが高速だろうか。
//
// while (i < A) arr[i++] &= ~flag;
// while (i < B) arr[i++] |= flag;
// while (i < C) arr[i++] &= ~flag;
//
// 実際に -march=native -O3 で実測してみると
// 確かに一番最後の物 (updateC) が一番速い。これを使う。

bool updateA(std::uint32_t* arr, std::size_t A, std::size_t B, std::size_t C, std::uint32_t flag) {
  std::size_t i = 0;
  bool dirty = false;
  while (i < A) {
    if (arr[i] & flag) {
      dirty = true;
      arr[i] &= ~flag;
    }
    i++;
  }
  while (i < B) {
    if (!(arr[i] & flag)) {
      dirty = true;
      arr[i] |=  flag;
    }
    i++;
  }
  while (i < C) {
    if (arr[i] & flag) {
      dirty = true;
      arr[i] &= ~flag;
    }
    i++;
  }
  return dirty;
}

bool updateB(std::uint32_t* arr, std::size_t A, std::size_t B, std::size_t C, std::uint32_t flag) {
  std::size_t i = 0;
  for (; i < A; i++) if (arr[i] & flag) goto phase1;
  for (; i < B; i++) if (!(arr[i] & flag)) goto phase2;
  for (; i < C; i++) if (arr[i] & flag) goto phase3;
  return false;

 phase1: while (i < A) arr[i++] &= ~flag;
 phase2: while (i < B) arr[i++] |=  flag;
 phase3: while (i < C) arr[i++] &= ~flag;
  return true;
}

bool updateC(std::uint32_t* arr, std::size_t A, std::size_t B, std::size_t C, std::uint32_t flag) {
  std::size_t i = 0;
  std::uint32_t dirty = 0;
  while (i < A) {
    dirty |= arr[i];
    arr[i++] &= ~flag;
  }
  while (i < B) {
    dirty |= ~arr[i];
    arr[i++] |= flag;
  }
  while (i < C) {
    dirty |= arr[i];
    arr[i++] &= ~flag;
  }
  return dirty & flag;
}

//-----------------------------------------------------------------------------

int main() {
  std::vector<std::uint32_t> buffer;
  auto _measure = [&] (std::vector<std::uint32_t> const& arr0) {
                    {
                      buffer = arr0;
                      auto time0 = std::chrono::high_resolution_clock::now();
                      for (int i = 0; i < 16; i++)
                        updateA(&buffer[0], 30, 70, 100, 1 << i);
                      auto time1 = std::chrono::high_resolution_clock::now();
                      std::cout << "updateA " << std::setw(10)
                                << std::chrono::duration_cast<std::chrono::nanoseconds>(time1 - time0).count() << "nsec" << std::endl;
                    }

                    {
                      buffer = arr0;
                      auto time0 = std::chrono::high_resolution_clock::now();
                      for (int i = 0; i < 16; i++)
                        updateB(&buffer[0], 30, 70, 100, 1 << i);
                      auto time1 = std::chrono::high_resolution_clock::now();
                      std::cout << "updateB " << std::setw(10)
                                << std::chrono::duration_cast<std::chrono::nanoseconds>(time1 - time0).count() << "nsec" << std::endl;
                    }

                    {
                      buffer = arr0;
                      auto time0 = std::chrono::high_resolution_clock::now();
                      for (int i = 0; i < 16; i++)
                        updateC(&buffer[0], 30, 70, 100, 1 << i);
                      auto time1 = std::chrono::high_resolution_clock::now();
                      std::cout << "updateC " << std::setw(10)
                                << std::chrono::duration_cast<std::chrono::nanoseconds>(time1 - time0).count() << "nsec" << std::endl;
                    }
                  };

  std::vector<std::uint32_t> arr1((std::size_t) 100);

  // case1
  std::generate(arr1.begin(), arr1.end(), std::rand);
  std::printf("case1\n");
  _measure(arr1);
  _measure(arr1);

  // case2
  std::printf("case2\n");
  std::fill(arr1.begin(), arr1.end(), 0u);
  for (int i = 0; i < 5; i++)
    arr1[std::rand() % 100] = std::rand();
  _measure(arr1);
  _measure(arr1);

  // case3
  std::printf("case3\n");
  std::fill(arr1.begin(), arr1.end(), 0u);
  std::fill(arr1.begin() + 30, arr1.begin() + 70, 0xFFFF);
  _measure(arr1);
  _measure(arr1);

  return 0;
}
