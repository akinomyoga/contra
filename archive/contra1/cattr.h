// -*- mode: c++ -*-
#ifndef contra_cattr_h
#define contra_cattr_h
#include "defs.hpp"

struct char_attr{
  // 8
  contra1::u4t bold      :1;
  contra1::u4t faint     :1;
  contra1::u4t style     :2; // none, italic, fraktur
  contra1::u4t underline :2; // none, single, double
  contra1::u4t blink     :2; // none, slow, rapid

  // 9
  contra1::u4t nega      :1;
  contra1::u4t invis     :1;
  contra1::u4t strike    :1;
  contra1::u4t font      :4; // default, font1-font9
  contra1::u4t frame     :2; // none, frame, circle

  // 8
  contra1::u4t overline  :1;
  contra1::u4t cjk_rline :3; // none, single under, double under, single over, double over
  contra1::u4t cjk_lline :3; // none, single over, double over, single under, double under
  contra1::u4t cjk_stress:1;
};

struct char_color{
  contra1::byte r;
  contra1::byte g;
  contra1::byte b;
  contra1::byte type; // 0: default, 1: 16 color, 2: 256 color, 3: rgb color
};

struct char_value{
  contra1::u4t val;
  contra1::u1t width;
  contra1::u1t acs;
};

enum char_attr_values{
  char_attr_off=0,
  char_attr_on =1,

  char_attr_style_none   =0,
  char_attr_style_italic =1,
  char_attr_style_fraktur=2,

  char_attr_underline_none  =0,
  char_attr_underline_single=1,

  char_attr_blink_off  =0,
  char_attr_blink_slow =1,
  char_attr_blink_rapid=2,

  char_attr_font_default   =0,
  char_attr_font_alternate1=1,
  char_attr_font_alternate2=2,
  char_attr_font_alternate3=3,
  char_attr_font_alternate4=4,
  char_attr_font_alternate5=5,
  char_attr_font_alternate6=6,
  char_attr_font_alternate7=7,
  char_attr_font_alternate8=8,
  char_attr_font_alternate9=9,

  char_attr_frame_none=0,
  char_attr_frame_rect=1,
  char_attr_frame_circ=2,

  char_attr_cjkline_none        =0,
  char_attr_cjkline_single_under=1,
  char_attr_cjkline_double_under=2,
  char_attr_cjkline_single_over =3,
  char_attr_cjkline_double_over =4,

  char_color_type_default=0,
  char_color_type_16color=1,
  char_color_type_x11    =2,
  char_color_type_rgb    =3,
};

#endif
