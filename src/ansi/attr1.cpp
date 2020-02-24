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

  class attribute_table {
    struct entry {
      attribute attr;
      attr_t listp;
      entry(attribute const& attr): attr(attr), listp(0) {}
    };

    std::vector<entry> table;
    attr_t freep = 0;

    std::uint32_t max_size = 0x1000;

  public:
    attr_t save(attribute const& attr) {
      if (freep) {
        attr_t const ret = freep;
        entry& ent = table[(std::uint32_t) (ret & ~attr_extended)];
        freep = ent.listp;
        ent.attr = attr;
        ent.listp = 0;
        return ret;
      } else if (table.size() < max_size) {
        attr_t const ret = attr_extended | (std::uint32_t) table.size();
        table.emplace_back(attr);
        return ret;
      } else {
        // todo: 容量不足
        return 0;
      }
    }

    // todo: mark/sweep
  };

  struct attribute_builder {
    attribute_table& m_table;
    attribute     m_attribute;
    std::uint32_t m_attribute_version = 0;
    mutable attr_t        m_attr = 0;
    mutable std::uint32_t m_attr_version = 0;

  public:
    attribute_builder(attribute_table& table): m_table(table) {}

  public:
    attr_t attr() const {
      if (!(m_attr & attr_extended))
        return m_attr;
      if (m_attribute_version == m_attr_version)
        return m_attr;

      if ((m_attribute.aflags & aflags_extension_mask) || m_attribute.xflags) {
        m_attr = m_table.save(m_attribute);
        m_attr_version = m_attribute_version;
      } else {
        reduce();
      }
      return m_attr;
    }

  private:
    void extend() {
      m_attribute.aflags = (std::uint32_t) m_attr & attr_common_mask;
      m_attribute.xflags = 0;
      if (m_attr & attr_fg_set) {
        m_attribute.aflags |= color_space_indexed << aflags_fg_space_shift;
        m_attribute.fg = (std::uint32_t) (m_attr & attr_fg_mask) >> attr_fg_shift;
      }
      if (m_attr & attr_bg_set) {
        m_attribute.aflags |= color_space_indexed << aflags_bg_space_shift;
        m_attribute.bg = (std::uint32_t) (m_attr & attr_bg_mask) >> attr_bg_shift;
      }
      m_attr_version = 0;
      m_attribute_version = 0;
    }
    void reduce() const {
      m_attr = (std::uint32_t) m_attribute.aflags & attr_common_mask;
      if (m_attribute.aflags & aflags_fg_space_mask)
        m_attr |= attr_fg_set | m_attribute.fg << attr_fg_shift;
      if (m_attribute.aflags & aflags_bg_space_mask)
        m_attr |= attr_bg_set | m_attribute.bg << attr_bg_shift;
    }

  public:
    void set_fg(color_t fg, int space) {
      if (!(m_attr & attr_extended)) {
        if (space == color_space_default) {
          m_attr &= ~(attr_fg_set | attr_fg_mask);
          return;
        } else if (space == color_space_indexed) {
          m_attr = m_attr & ~attr_fg_mask | attr_fg_set | fg << attr_fg_shift;
          return;
        }
        extend();
      }

      m_attribute.fg = fg;
      m_attribute.aflags.reset(aflags_fg_space_mask, space << aflags_fg_space_shift);
      m_attribute_version++;
    }
    void set_bg(color_t bg, int space) {
      if (!(m_attr & attr_extended)) {
        if (space == color_space_default) {
          m_attr &= ~(attr_bg_set | attr_bg_mask);
          return;
        } else if (space == color_space_indexed) {
          m_attr = m_attr & ~attr_bg_mask | attr_bg_set | bg << attr_bg_shift;
          return;
        }
        extend();
      }

      m_attribute.bg = bg;
      m_attribute.aflags.reset(aflags_bg_space_mask, space << aflags_bg_space_shift);
      m_attribute_version++;
    }
    void set_dc(color_t dc, int space) {
      if (!(m_attr & attr_extended)) {
        if (space == color_space_default) return;
        extend();
      }

      m_attribute.dc = dc;
      m_attribute.aflags.reset(aflags_dc_space_mask, space << aflags_dc_space_shift);
      m_attribute_version++;
    }

  private:
    void reset_common_attr(attr0_t mask, attr0_t value) {
      if (m_attr & attr_extended) {
        m_attribute.aflags.reset(mask, value);
        m_attribute_version++;
      } else {
        m_attr.reset(mask, value);
      }
    }
    void set_common_attr(attr0_t bit) {
      if (m_attr & attr_extended) {
        m_attribute.aflags |= bit;
        m_attribute_version++;
      } else {
        m_attr |= bit;
      }
    }
    void clear_common_attr(attr0_t bit) {
      if (m_attr & attr_extended) {
        m_attribute.aflags &= ~bit;
        m_attribute_version++;
      } else {
        m_attr &= ~bit;
      }
    }

    void reset_aflags(aflags_t mask, aflags_t value) {
      if (!(m_attr & attr_extended)) extend();
      m_attribute.aflags.reset(mask, value);
      m_attribute_version++;
    }
    void clear_aflags(aflags_t mask) {
      if (!(m_attr & attr_extended)) return;
      m_attribute.aflags &= ~mask;
      m_attribute_version++;
    }

    void reset_xflags(xflags_t mask, xflags_t value) {
      if (!(m_attr & attr_extended)) extend();
      m_attribute.xflags.reset(mask, value);
      m_attribute_version++;
    }
    void set_xflags(xflags_t value) {
      if (!(m_attr & attr_extended)) extend();
      m_attribute.xflags |= value;
      m_attribute_version++;
    }
    void clear_xflags(xflags_t mask) {
      if (!(m_attr & attr_extended)) return;
      m_attribute.xflags &= ~mask;
      m_attribute_version++;
    }

  public:
    void set_weight(attr0_t weight)          { reset_common_attr(attr_weight_mask, weight); }
    void clear_weight()                      { clear_common_attr(attr_weight_mask); }
    void set_shape(attr0_t shape)            { reset_common_attr(attr_shape_mask, shape); }
    void clear_shape()                       { clear_common_attr(attr_shape_mask); }
    void set_underline(attr0_t underline)    { reset_common_attr(attr_underline_mask, underline); }
    void clear_underline()                   { clear_common_attr(attr_underline_mask); }
    void set_blink(attr0_t blink)            { reset_common_attr(attr_blink_mask, blink); }
    void clear_blink()                       { clear_common_attr(attr_blink_mask); }
    void set_inverse()                       { set_common_attr(attr_inverse_set); }
    void clear_inverse()                     { clear_common_attr(attr_inverse_set); }
    void set_invisible()                     { set_common_attr(attr_invisible_set); }
    void clear_invisible()                   { clear_common_attr(attr_invisible_set); }
    void set_strike()                        { set_common_attr(attr_strike_set); }
    void clear_strike()                      { clear_common_attr(attr_strike_set); }

    void set_font(aflags_t font)             { reset_aflags(aflags_font_mask, font); }
    void clear_font()                        { clear_aflags(aflags_font_mask); }

    void set_proportional()                  { set_xflags(xflags_proportional_set); }
    void clear_proportional()                { clear_xflags(xflags_proportional_set); }
    void set_frame(xflags_t frame)           { reset_xflags(xflags_frame_mask, frame); }
    void clear_frame()                       { clear_xflags(xflags_frame_mask); }
    void set_overline()                      { set_xflags(xflags_overline_set); }
    void clear_overline()                    { clear_xflags(xflags_overline_set); }
    void set_ideogram(xflags_t ideogram)     { reset_xflags(xflags_ideogram_mask, ideogram); }
    void clear_ideogram()                    { clear_xflags(xflags_ideogram_mask); }

    void set_decdhl(xflags_t decdhl)         { reset_xflags(xflags_decdhl_mask, decdhl); }
    void clear_decdhl()                      { clear_xflags(xflags_decdhl_mask); }

    void set_rlogin_rline(xflags_t ideogram) { reset_xflags(xflags_rlogin_rline_mask, ideogram); }
    void set_rlogin_lline(xflags_t ideogram) { reset_xflags(xflags_rlogin_lline_mask, ideogram); }
    void set_rlogin_double_strike()          { set_xflags(xflags_rlogin_double_strike); }
    void clear_rlogin_ideogram()             { clear_xflags(xflags_rlogin_ideogram_mask); }

    void set_mintty_subsup(xflags_t subsup)  { reset_xflags(xflags_mintty_subsup_mask, subsup); }
    void clear_mintty_subsup()               { clear_xflags(xflags_mintty_subsup_mask); }

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
