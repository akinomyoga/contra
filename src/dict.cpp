#include "dict.hpp"

using namespace ::contra::dict;

typedef attribute_t _at;

static void initialize_unresettable_flags(aflags_t& flags, termcap_sgrflag1<aflags_t> const& sgrflag) {
  if (!sgrflag.off && sgrflag.on) flags |= sgrflag.bit;
}
static void initialize_unresettable_flags(aflags_t& flags, termcap_sgrflag2<aflags_t> const& sgrflag) {
  if (!sgrflag.off) {
    if (sgrflag.on1) flags |= sgrflag.bit1;
    if (sgrflag.on2) flags |= sgrflag.bit2;
  }
}

static void initialize_unresettable_flags(xflags_t& flags, termcap_sgrflag1<xflags_t> const& sgrflag) {
  if (!sgrflag.off && sgrflag.on) flags |= sgrflag.bit;
}
static void initialize_unresettable_flags(xflags_t& flags, termcap_sgrflag2<xflags_t> const& sgrflag) {
  if (!sgrflag.off) {
    if (sgrflag.on1) flags |= sgrflag.bit1;
    if (sgrflag.on2) flags |= sgrflag.bit2;
  }
}

static void initialize_unresettable_flags(aflags_t& flags, termcap_sgrcolor const& sgrcolor) {
  if (!sgrcolor.off && (sgrcolor.base || sgrcolor.iso8613.sgr))
    flags |= sgrcolor.mask;
}

static void initialize_unresettable_flags(xflags_t& flags, termcap_sgrideogram const& capIdeogram) {
  if (!capIdeogram.reset)
    flags |= xflags_ideogram_mask;
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


template<typename Flags>
void tty_writer::update_sgrflag1(
  Flags aflagsNew, Flags aflagsOld,
  termcap_sgrflag1<Flags> const& sgrflag
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
template void tty_writer::update_sgrflag1<aflags_t>(aflags_t aflagsNew, aflags_t aflagsOld, termcap_sgrflag1<aflags_t> const& sgrflag);
template void tty_writer::update_sgrflag1<xflags_t>(xflags_t aflagsNew, xflags_t aflagsOld, termcap_sgrflag1<xflags_t> const& sgrflag);

template<typename Flags>
void tty_writer::update_sgrflag2(
  Flags aflagsNew, Flags aflagsOld,
  termcap_sgrflag2<Flags> const& sgrflag
) {
  // when the attribute is not changed
  Flags bit1 = sgrflag.bit1;
  Flags bit2 = sgrflag.bit2;
  if (((aflagsNew ^ aflagsOld) & (bit1 | bit2)) == 0) return;

  // when the attribute is not supported by TERM
  unsigned const sgr1 = sgrflag.on1;
  unsigned const sgr2 = sgrflag.on2;
  if (!sgr1) bit1 = 0;
  if (!sgr2) bit2 = 0;
  if (!(bit1 | bit2)) return;

  Flags const added = aflagsNew & ~aflagsOld;
  Flags const removed = ~aflagsNew & aflagsOld;

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
template void tty_writer::update_sgrflag2<aflags_t>(aflags_t aflagsNew, aflags_t aflagsOld, termcap_sgrflag2<aflags_t> const& sgrflag);
template void tty_writer::update_sgrflag2<xflags_t>(xflags_t aflagsNew, xflags_t aflagsOld, termcap_sgrflag2<xflags_t> const& sgrflag);

void tty_writer::update_ideogram_decoration(xflags_t xflagsNew, xflags_t xflagsOld, termcap_sgrideogram const& cap) {
  if (((xflagsNew ^ xflagsOld) & xflags_ideogram_mask) == 0) return;

  int sgr = 0;
  if (xflagsNew & xflags_ideogram_mask) {
    switch ((xflagsNew & xflags_ideogram_mask).value()) {
    case xflags_ideogram_line_single_rb.value(): sgr = cap.single_rb; break;
    case xflags_ideogram_line_double_rb.value(): sgr = cap.double_rb; break;
    case xflags_ideogram_line_single_lt.value(): sgr = cap.single_lt; break;
    case xflags_ideogram_line_double_lt.value(): sgr = cap.double_lt; break;
    case xflags_ideogram_line_single_lb.value(): sgr = cap.single_lb; break;
    case xflags_ideogram_line_double_lb.value(): sgr = cap.double_lb; break;
    case xflags_ideogram_line_single_rt.value(): sgr = cap.single_rt; break;
    case xflags_ideogram_line_double_rt.value(): sgr = cap.double_rt; break;
    case xflags_ideogram_stress.value(): sgr = cap.stress; break;
    default:
      mwg_assert(0, "(xflagsNew & is_ideogram_mask) has invalid value");
    }

    if (sgr == 0) sgr_put(cap.reset);
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
  if (colorSpaceNew == color_space_indexed) {
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

  if (colorSpaceNew == color_space_default && sgrcolor.off) {
    sgr_put(sgrcolor.off);
    return;
  }

  // sgrISO8613_6Color
  if (sgrcolor.iso8613.sgr
    && (sgrcolor.iso8613.color_spaces & 1 << colorSpaceNew)
  ) {
    sgr_put(sgrcolor.iso8613.sgr);
    put(sgrcolor.iso8613.separater);
    put_unsigned(color_space_tosgr(colorSpaceNew));

    if (colorSpaceNew == color_space_indexed) {
      if (colorNew <= sgrcolor.iso8613.max_index) {
        put(sgrcolor.iso8613.separater);
        put_unsigned(colorNew);
      }
      return;
    } else {
      int numberOfComponents = 0;
      switch (colorSpaceNew) {
      case color_space_rgb:
      case color_space_cmy:  numberOfComponents = 3; break;
      case color_space_cmyk: numberOfComponents = 4; break;
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

void tty_writer::apply_attr(attr_t const& new_attr) {
  attr_t const& old_attr = this->m_attr;
  if (old_attr == new_attr) return;

  attribute_t new_attribute;
  m_atable->get_extended(new_attribute, new_attr);
  attribute_t& old_attribute = this->m_attribute;

  if (new_attribute.is_default()) {
    sgr_clear();
    old_attribute.clear();
  } else {
    this->sgr_isOpen = false;

    aflags_t const aremoved = ~new_attribute.aflags & old_attribute.aflags;
    xflags_t const xremoved = ~new_attribute.xflags & old_attribute.xflags;
    if (aremoved & sgrcap->aflagsNotResettable
      || xremoved & sgrcap->xflagsNotResettable
    ) {
      sgr_put(0);
      old_attribute.clear_sgr();
    }

    update_sgrflag2(new_attribute.aflags, old_attribute.aflags, sgrcap->cap_bold     );
    update_sgrflag2(new_attribute.aflags, old_attribute.aflags, sgrcap->cap_italic   );
    update_sgrflag2(new_attribute.aflags, old_attribute.aflags, sgrcap->cap_underline);
    update_sgrflag2(new_attribute.aflags, old_attribute.aflags, sgrcap->cap_blink    );

    update_sgrflag1(new_attribute.aflags, old_attribute.aflags, sgrcap->cap_inverse  );
    update_sgrflag1(new_attribute.aflags, old_attribute.aflags, sgrcap->cap_invisible);
    update_sgrflag1(new_attribute.aflags, old_attribute.aflags, sgrcap->cap_strike   );

    update_sgrflag2(new_attribute.xflags, old_attribute.xflags, sgrcap->cap_framed);
    update_sgrflag1(new_attribute.xflags, old_attribute.xflags, sgrcap->cap_proportional);
    update_sgrflag1(new_attribute.xflags, old_attribute.xflags, sgrcap->cap_overline);
    update_ideogram_decoration(new_attribute.xflags, old_attribute.xflags, sgrcap->cap_ideogram);

    update_sgrcolor(
      new_attribute.fg_space(), new_attribute.fg_color(),
      old_attribute.fg_space(), old_attribute.fg_color(),
      sgrcap->cap_fg);

    update_sgrcolor(
      new_attribute.bg_space(), new_attribute.bg_color(),
      old_attribute.bg_space(), old_attribute.bg_color(),
      sgrcap->cap_bg);

    this->m_attr = new_attr;
    this->m_attribute = new_attribute;
    if (this->sgr_isOpen) put(ascii_m);
  }
}

#include "ansi/line.hpp"
#include "ansi/term.hpp"

using namespace ::contra::ansi;

void tty_writer::print_screen(board_t const& b) {
  for (curpos_t y = 0; y < b.height(); y++) {
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
  apply_attr(0);
}
