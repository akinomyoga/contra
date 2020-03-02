// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_dict_hpp
#define contra_dict_hpp
#include <cstdint>
#include <cstdio>
#include <mwg/except.h>
#include "contradef.hpp"
#include "enc.utf8.hpp"
#include "ansi/attr.hpp"

namespace contra {
namespace dict {
  using namespace ::contra::ansi;

  enum termcap_constants {
    color_space_default_bit     = 1 << color_space_default    ,
    color_space_transparent_bit = 1 << color_space_transparent,
    color_space_rgb_bit         = 1 << color_space_rgb        ,
    color_space_cmy_bit         = 1 << color_space_cmy        ,
    color_space_cmyk_bit        = 1 << color_space_cmyk       ,
    color_space_indexed_bit     = 1 << color_space_indexed    ,
  };

  //-----------------------------------------------------------------------------
  //
  // SGR capabilities
  //
  // Note:
  //   さすがにどの端末も SGR 0 は対応していると仮定している。
  //   (但し、端末が SGR 自体に対応していない場合はこの限りではない。)
  //

  template<typename Flags>
  struct termcap_sgrflag1 {
    Flags bit;
    unsigned on;
    unsigned off;
  };

  template<typename Flags>
  struct termcap_sgrflag2 {
    Flags bit1;
    unsigned on1;
    Flags bit2;
    unsigned on2;
    unsigned off;
  };

  struct termcap_sgrideogram {
    /*?lwiki
     * @var bool is_decoration_exclusive;
     *   現在のところ ideogram decorations はどれか一つだけが有効になる様に実装することにする。
     *   (但し、将来それぞれ独立に on/off する様に変更できる余地を残すために各項目 1bit 占有している。)
     *   但し、実際の端末では複数を有効にすることができる実装もあるだろう。
     *   その場合には、一貫した動作をさせる為には必ず既に設定されている装飾を解除する必要がある。
     *   この設定項目は出力先の端末で ideogram decorations が排他的かどうかを保持する。
     */
    unsigned single_rb { 60 };
    unsigned double_rb { 61 };
    unsigned single_lt { 62 };
    unsigned double_lt { 63 };
    unsigned single_lb { 66 };
    unsigned double_lb { 67 };
    unsigned single_rt { 68 };
    unsigned double_rt { 69 };
    unsigned stress    { 64 };
    unsigned reset     { 65 };
  };

  struct termcap_sgrcolor {
    aflags_t mask;
    unsigned base;
    unsigned off;

    // 90-97, 100-107
    unsigned aixterm_color;

    // 38:5:0-255
    struct {
      unsigned      sgr         ; //!< SGR number of ISO 8613-6 color specification
      unsigned char color_spaces; //!< supported color specs specified by bits
      char          separater   ; //!< the separator character of ISO 8613-6 SGR arguments
      bool          exact       ; //!< the separator character of ISO 8613-6 SGR arguments
      unsigned      max_index   ; //!< the number of colors available through 38:5:*
    } iso8613;

    // 1,22 / 5,25 で明るさを切り替える方式
    unsigned high_intensity_on ;
    unsigned high_intensity_off;
  };

  struct termcap_sgr_type {
    typedef attribute_t _at;
    // aflags
    termcap_sgrflag2<aflags_t> cap_bold         { attr_bold_set        , 1, attr_faint_set           ,  2, 22 };
    termcap_sgrflag2<aflags_t> cap_italic       { attr_italic_set      , 3, attr_fraktur_set         , 20, 23 };
    termcap_sgrflag2<aflags_t> cap_underline    { attr_underline_single, 4, attr_underline_double    , 21, 24 };
    termcap_sgrflag2<aflags_t> cap_blink        { attr_blink_set       , 5, attr_rapid_blink_set     ,  6, 25 };
    termcap_sgrflag1<aflags_t> cap_inverse      { attr_inverse_set     , 7, 27 };
    termcap_sgrflag1<aflags_t> cap_invisible    { attr_invisible_set   , 8, 28 };
    termcap_sgrflag1<aflags_t> cap_strike       { attr_strike_set      , 9, 29 };

    // xflags
    termcap_sgrflag2<xflags_t> cap_framed       { xflags_frame_set     , 51, xflags_circle_set, 52, 54 };

    termcap_sgrflag1<xflags_t> cap_proportional { xflags_proportional_set, 26, 50 };
    termcap_sgrflag1<xflags_t> cap_overline     { xflags_overline_set    , 53, 55 };

    termcap_sgrideogram cap_ideogram;

    // colors
    termcap_sgrcolor cap_fg {
      aflags_fg_space_mask, 30, 39,  90,
      { 38, color_space_indexed_bit | color_space_rgb_bit, ascii_semicolon, false, 255 },
      0, 0
    };
    termcap_sgrcolor cap_bg {
      aflags_bg_space_mask, 40, 49, 100,
      { 48, color_space_indexed_bit | color_space_rgb_bit, ascii_semicolon, false, 255 },
      0, 0
    };

    aflags_t aflagsNotResettable { 0 };
    xflags_t xflagsNotResettable { 0 };

  public:
    void initialize();
  };

  //-----------------------------------------------------------------------------

  struct tty_writer {
    attribute_table* atable;

    std::FILE* file;

    // 出力先端末の性質
    termcap_sgr_type const* sgrcap;
    bool termcap_bce = false;

    cattr_t m_attr = 0;
    attribute_t m_attribute = 0;
    bool sgr_isOpen;

    tty_writer(attribute_table& atable, std::FILE* file, termcap_sgr_type* sgrcap): atable(&atable), file(file), sgrcap(sgrcap) {
      m_attr = 0;
    }

  private:
    void sgr_clear() const {
      put(ascii_esc);
      put(ascii_left_bracket);
      put(ascii_m);
    }

    void sgr_put(unsigned value) {
      if (this->sgr_isOpen) {
        put(ascii_semicolon);
      } else {
        this->sgr_isOpen = true;
        put(ascii_esc);
        put(ascii_left_bracket);
      }

      if (value) put_unsigned(value);
    }

    template<typename Flags>
    void update_sgrflag1(
      Flags aflagsNew, Flags aflagsOld,
      termcap_sgrflag1<Flags> const& sgrflag);
    template<typename Flags>
    void update_sgrflag2(
      Flags aflagsNew, Flags aflagsOld,
      termcap_sgrflag2<Flags> const& sgrflag);

    void update_ideogram_decoration(
      xflags_t xflagsNew, xflags_t xflagsOld,
      termcap_sgrideogram const& cap);

    void update_sgrcolor(
      int colorSpaceNew, color_t colorNew,
      int colorSpaceOld, color_t colorOld,
      termcap_sgrcolor const& sgrcolor);

  public:
    void apply_attr(cattr_t const& newAttr);

    void put_u32(char32_t c) const { contra::encoding::put_u8(c, file); }
    void put(char c) const { std::fputc(c, file); }
    void put_str(const char32_t* s) const {
      while (*s) put(*s++);
    }
    void put_unsigned(unsigned value) const {
      if (value >= 10)
        put_unsigned(value/10);
      put((byte) (ascii_0 + value % 10));
    }
    void flush() const {
      std::fflush(file);
    }

  private:
    void put_skip(curpos_t& wskip) {
      if (atable->is_default(m_attr) && wskip <= 4) {
        while (wskip--) put(' ');
      } else {
        put(ascii_esc);
        put(ascii_left_bracket);
        put_unsigned(wskip);
        put(ascii_C);
      }
      wskip = 0;
    }
  public:
    // ToDo: output encoding
    void print_screen(board_t const& b);
  };



}
}

#endif
