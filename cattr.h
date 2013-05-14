// -*- mode:C++ -*-
#pragma once
#ifndef CONTRA_CATTR_H
#define CONTRA_CATTR_H
#include <mwg/defs.h>

struct char_attr{
  // 8
  mwg::u4t bold      :1;
  mwg::u4t faint     :1;
  mwg::u4t style     :2; // none, italic, fraktur
  mwg::u4t underline :2; // none, single, double
  mwg::u4t blink     :2; // none, slow, rapid

  // 9
  mwg::u4t nega      :1;
  mwg::u4t invis     :1;
  mwg::u4t strike    :1;
  mwg::u4t font      :4; // default, font1-font9
  mwg::u4t frame     :2; // none, frame, circle

  // 8
  mwg::u4t overline  :1;
  mwg::u4t cjk_rline :3; // none, single under, double under, single over, double over
  mwg::u4t cjk_lline :3; // none, single over, double over, single under, double under
  mwg::u4t cjk_stress:1;
};

struct char_color{
  mwg::byte r;
  mwg::byte g;
  mwg::byte b;
  mwg::byte type; // 0: default, 1: 16 color, 2: 256 color, 3: rgb color
};

struct char_value{
  mwg::u4t val;
  mwg::u1t width;
  mwg::u1t acs;
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
