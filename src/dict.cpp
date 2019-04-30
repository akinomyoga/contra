#include "dict.hpp"

using namespace ::contra::dict;

typedef attribute_t _at;

static void initialize_unresettable_flags(aflags_t& flags, termcap_sgrflag1 const& sgrflag) {
  if (!sgrflag.off && sgrflag.on) flags |= sgrflag.bit;
}

static void initialize_unresettable_flags(aflags_t& flags, termcap_sgrflag2 const& sgrflag) {
  if (!sgrflag.off) {
    if (sgrflag.on1) flags |= sgrflag.bit1;
    if (sgrflag.on2) flags |= sgrflag.bit2;
  }
}

static void initialize_unresettable_flags(aflags_t& flags, termcap_sgrcolor const& sgrcolor) {
  if (!sgrcolor.off && (sgrcolor.base || sgrcolor.iso8613.sgr))
    flags = sgrcolor.bit;
}

static void initialize_unresettable_flags(aflags_t& flags, termcap_sgrideogram const& capIdeogram) {
  if (!capIdeogram.reset)
    flags |= _at::is_ideogram_decoration_mask;
}

void termcap_sgr_type::initialize() {
  aflagsNotResettable = 0;
  initialize_unresettable_flags(aflagsNotResettable, cap_bold);
  initialize_unresettable_flags(aflagsNotResettable, cap_italic);
  initialize_unresettable_flags(aflagsNotResettable, cap_underline);
  initialize_unresettable_flags(aflagsNotResettable, cap_blink);
  initialize_unresettable_flags(aflagsNotResettable, cap_inverse);
  initialize_unresettable_flags(aflagsNotResettable, cap_invisible);
  initialize_unresettable_flags(aflagsNotResettable, cap_strike);
  initialize_unresettable_flags(aflagsNotResettable, cap_fg);
  initialize_unresettable_flags(aflagsNotResettable, cap_bg);

  xflagsNotResettable = 0;
  initialize_unresettable_flags(xflagsNotResettable, cap_framed);
  initialize_unresettable_flags(xflagsNotResettable, cap_proportional);
  initialize_unresettable_flags(xflagsNotResettable, cap_overline);
  initialize_unresettable_flags(xflagsNotResettable, cap_ideogram);
}


void tty_writer::update_sgrflag1(
  aflags_t aflagsNew, aflags_t aflagsOld,
  termcap_sgrflag1 const& sgrflag
) {
  // when the attribute is not changed
  if (((aflagsNew ^ aflagsOld) & sgrflag.bit) == 0) return;

  // when the attribute is not supported by TERM
  if (!sgrflag.on) return;

  if (aflagsNew & sgrflag.bit) {
    sgr_put(sgrflag.on);
  } else {
    sgr_put(sgrflag.off);
  }
}

void tty_writer::update_sgrflag2(
  aflags_t aflagsNew, aflags_t aflagsOld,
  termcap_sgrflag2 const& sgrflag
) {
  // when the attribute is not changed
  aflags_t bit1 = sgrflag.bit1;
  aflags_t bit2 = sgrflag.bit2;
  if (((aflagsNew ^ aflagsOld) & (bit1 | bit2)) == 0) return;

  // when the attribute is not supported by TERM
  unsigned const sgr1 = sgrflag.on1;
  unsigned const sgr2 = sgrflag.on2;
  if (!sgr1) bit1 = 0;
  if (!sgr2) bit2 = 0;
  if (!(bit1 | bit2)) return;

  aflags_t const added = aflagsNew & ~aflagsOld;
  aflags_t const removed = ~aflagsNew & aflagsOld;

  if (removed & (bit1 | bit2)) {
    sgr_put(sgrflag.off);
    if (aflagsNew & bit2)
      sgr_put(sgr2);
    if (aflagsNew & bit1)
      sgr_put(sgr1);
  } else {
    if (added & bit2)
      sgr_put(sgr2);
    if (added & bit1)
      sgr_put(sgr1);
  }
}

void tty_writer::update_ideogram_decoration(xflags_t xflagsNew, xflags_t xflagsOld, termcap_sgrideogram const& cap) {
  if (((xflagsNew ^ xflagsOld) & _at::is_ideogram_decoration_mask) == 0) return;

  int sgr;
  if (xflagsNew & _at::is_ideogram_decoration_mask) {
    if (xflagsNew & (_at::is_ideogram_single_rb_set | _at::is_ideogram_double_rb_set))
      sgr = xflagsNew & _at::is_ideogram_single_rb_set ? cap.single_rb : cap.double_rb;
    else if (xflagsNew & (_at::is_ideogram_single_lt_set | _at::is_ideogram_double_lt_set))
      sgr = xflagsNew & _at::is_ideogram_single_lt_set ? cap.single_lt : cap.double_lt;
    else if (xflagsNew & (_at::is_ideogram_single_lb_set | _at::is_ideogram_double_lb_set))
      sgr = xflagsNew & _at::is_ideogram_single_lb_set ? cap.single_lb : cap.double_lb;
    else if (xflagsNew & (_at::is_ideogram_single_rt_set | _at::is_ideogram_double_rt_set))
      sgr = xflagsNew & _at::is_ideogram_single_rt_set ? cap.single_rt : cap.double_rt;
    else if (xflagsNew & _at::is_ideogram_stress_set)
      sgr = cap.stress;
    else
      mwg_assert(0, "is_ideogram_decoration_mask is wrong");

    if (sgr == 0
      || (xflagsOld & _at::is_ideogram_decoration_mask && cap.is_decoration_exclusive)
    )
      sgr_put(cap.reset);
  } else
    sgr = cap.reset;

  if (sgr) sgr_put(sgr);
}

void tty_writer::update_sgrcolor(
  int colorSpaceNew, color_t colorNew,
  int colorSpaceOld, color_t colorOld,
  termcap_sgrcolor const& sgrcolor
) {
  if (colorSpaceNew == colorSpaceOld && colorNew == colorOld) return;

  // sgrAnsiColor, sgrAixColor
  if (colorSpaceNew == _at::color_space_indexed) {
    if (colorNew < 8) {
      if (sgrcolor.base) {
        // e.g \e[31m
        if (sgrcolor.high_intensity_off)
          sgr_put(sgrcolor.high_intensity_off);
        sgr_put(sgrcolor.base + colorNew);
        return;
      }
    } else if (colorNew < 16) {
      if (sgrcolor.aixterm_color) {
        // e.g. \e[91m
        sgr_put(sgrcolor.aixterm_color + (colorNew & 7));
        return;
      } else if (sgrcolor.base && sgrcolor.high_intensity_on) {
        // e.g. \e[1;31m \e[2;42m
        sgr_put(sgrcolor.high_intensity_on);
        sgr_put(sgrcolor.base + (colorNew & 7));
        return;
      }
    }
  }

  if (colorSpaceNew == _at::color_space_default && sgrcolor.off) {
    sgr_put(sgrcolor.off);
    return;
  }

  // sgrISO8613_6Color
  if (sgrcolor.iso8613.sgr
    && (sgrcolor.iso8613.color_spaces & 1 << colorSpaceNew)
  ) {
    sgr_put(sgrcolor.iso8613.sgr);
    put(sgrcolor.iso8613.separater);
    put_unsigned(colorSpaceNew);

    if (colorSpaceNew == _at::color_space_indexed) {
      if (colorNew <= sgrcolor.iso8613.max_index) {
        put(sgrcolor.iso8613.separater);
        put_unsigned(colorNew);
      }
      return;
    } else {
      int numberOfComponents = 0;
      switch (colorSpaceNew) {
      case _at::color_space_rgb:
      case _at::color_space_cmy:  numberOfComponents = 3; break;
      case _at::color_space_cmyk: numberOfComponents = 4; break;
      }

      // for ISO 8613-6 compatibility,
      // first print color space identifier
      if (sgrcolor.iso8613.exact) {
        put(sgrcolor.iso8613.separater);
        put_unsigned(0);
      }

      for (int icomp = 0; icomp < numberOfComponents; icomp++) {
        unsigned const comp = colorNew << icomp * 8 & 0xFF;
        put(sgrcolor.iso8613.separater);
        put_unsigned(comp);
      }

      return;
    }
  }

  // fallback
  // - ToDo: 色をクリアできない場合の対策。
  //   設定できる色の中で最も近い色を設定する。設定できる色が一つ
  //   もない時はそもそも何の色も設定されていない筈だから気にしな
  //   くて良い。
  if (sgrcolor.off)
    sgr_put(sgrcolor.off);
}

void tty_writer::apply_attr(attribute_t new_attr) {
  attribute_t& old_attr = this->m_attr;
  if (old_attr == new_attr) return;

  if (new_attr.is_default()) {
    sgr_clear();
    old_attr.clear();
  } else {
    this->sgr_isOpen = false;

    aflags_t const aremoved = ~new_attr.aflags & old_attr.aflags;
    xflags_t const xremoved = ~new_attr.xflags & old_attr.xflags;
    if (aremoved & sgrcap->aflagsNotResettable
      || xremoved & sgrcap->xflagsNotResettable
    ) {
      sgr_put(0);
      old_attr.clear_sgr();
    }

    update_sgrflag2(new_attr.aflags, old_attr.aflags, sgrcap->cap_bold     );
    update_sgrflag2(new_attr.aflags, old_attr.aflags, sgrcap->cap_italic   );
    update_sgrflag2(new_attr.aflags, old_attr.aflags, sgrcap->cap_underline);
    update_sgrflag2(new_attr.aflags, old_attr.aflags, sgrcap->cap_blink    );

    update_sgrflag1(new_attr.aflags, old_attr.aflags, sgrcap->cap_inverse  );
    update_sgrflag1(new_attr.aflags, old_attr.aflags, sgrcap->cap_invisible);
    update_sgrflag1(new_attr.aflags, old_attr.aflags, sgrcap->cap_strike   );

    update_sgrflag2(new_attr.xflags, old_attr.xflags, sgrcap->cap_framed);
    update_sgrflag1(new_attr.xflags, old_attr.xflags, sgrcap->cap_proportional);
    update_sgrflag1(new_attr.xflags, old_attr.xflags, sgrcap->cap_overline);
    update_ideogram_decoration(new_attr.xflags, old_attr.xflags, sgrcap->cap_ideogram);

    update_sgrcolor(
      new_attr.fg_space(), new_attr.fg_color(),
      old_attr.fg_space(), old_attr.fg_color(),
      sgrcap->cap_fg);

    update_sgrcolor(
      new_attr.bg_space(), new_attr.bg_color(),
      old_attr.bg_space(), old_attr.bg_color(),
      sgrcap->cap_bg);

    this->m_attr = new_attr;
    if (this->sgr_isOpen) put(ascii_m);
  }
}

#include "ansi/line.hpp"

using namespace ::contra::ansi;

void tty_writer::print_screen(board_t const& b) {
  for (curpos_t y = 0; y < b.m_height; y++) {
    line_t const& line = b.m_lines[y];
    curpos_t wskip = 0;
    curpos_t const ncell = (curpos_t) line.cells().size();
    for (curpos_t x = 0; x < ncell; x++) {
      cell_t const& cell = line.cells()[x];
      if (cell.character.is_wide_extension()) continue;
      if (cell.character.is_marker()) continue;
      if (cell.character.value == ascii_nul) {
        wskip += cell.width;
        continue;
      }

      if (wskip > 0) put_skip(wskip);
      apply_attr(cell.attribute);
      put_u32(cell.character.value);
    }

    put('\n');
  }
  apply_attr(attribute_t {});
}
