#include <cstdint>
#include <vector>
#include <type_traits>
#include "../util.hpp"
#include "attr.hpp"

namespace contra {
namespace ansi {

  struct attribute {
    aflags_t aflags = 0;
    xflags_t xflags = 0;
    color_t fg = 0;
    color_t bg = 0;
    color_t dc = 0;
  };

}
}

namespace test_flags {
  using namespace contra::util;
  typedef flags_t<std::uint32_t, struct f1_tag> f1_t;
  typedef flags_t<std::uint32_t, struct f2_tag> f2_t;
  void test1() {
    static_assert(flags_detail::is_flags<f1_t>);
    static_assert(flags_detail::is_rhs<f1_t, int>);
    constexpr f1_t f1_hello = 1;
    constexpr f2_t f2_hello = 1;

    constexpr f1_t f1_hello_world = f1_hello | 2;
    constexpr f1_t f1_hello_ringo = f1_hello & 3;
    constexpr f1_t f1_hello_orange = f1_hello ^ 4;

    f1_t v1 = f1_hello_world | f1_hello_ringo;
    v1 |= f1_hello_orange;
    v1 &= ~f1_hello_orange;
    //v1 &= f2_hello; // Error
  }
}

using namespace contra::ansi;

int main() {
  attribute_table table;
  attribute_builder buffer(table);
  buffer.set_fg(10, 5);
  buffer.set_bg(0, 0);

  return 0;
}
