#include "tty_observer.h"

using namespace contra;

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
    flags |= is_ideogram_decoration_mask;
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


void tty_observer::update_sgrflag1(
  aflags_t aflagsNew, aflags_t aflagsOld,
  termcap_sgrflag1 const& sgrflag
) {
  // when the attribute is not changed
  if (((aflagsNew ^ aflagsOld) & sgrflag.bit) == 0) return;

  // when the attribute is not supported by TERM
  if (!sgrflag.on) return;

  if (aflagsNew&sgrflag.bit) {
    sgr_put(sgrflag.on);
  } else {
    sgr_put(sgrflag.off);
  }
}

void tty_observer::update_sgrflag2(
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

void tty_observer::update_ideogram_decoration(xflags_t xflagsNew, xflags_t xflagsOld, termcap_sgrideogram const& cap) {
  if (((xflagsNew ^ xflagsOld) & is_ideogram_decoration_mask) == 0) return;

  int sgr;
  if (xflagsNew & is_ideogram_decoration_mask) {
    if (xflagsNew & (is_ideogram_single_rb_set | is_ideogram_double_rb_set))
      sgr = xflagsNew & is_ideogram_single_rb_set? cap.single_rb: cap.double_rb;
    else if (xflagsNew & (is_ideogram_single_lt_set | is_ideogram_double_lt_set))
      sgr = xflagsNew & is_ideogram_single_lt_set? cap.single_lt: cap.double_lt;
    else if (xflagsNew & (is_ideogram_single_lb_set | is_ideogram_double_lb_set))
      sgr = xflagsNew & is_ideogram_single_lb_set? cap.single_lb: cap.double_lb;
    else if (xflagsNew & (is_ideogram_single_rt_set | is_ideogram_double_rt_set))
      sgr = xflagsNew & is_ideogram_single_rt_set? cap.single_rt: cap.double_rt;
    else if (xflagsNew & is_ideogram_stress_set)
      sgr = cap.stress;
    else
      mwg_assert(0, "is_ideogram_decoration_mask is wrong");

    if (sgr == 0
      || (xflagsOld & is_ideogram_decoration_mask && cap.is_decoration_exclusive)
    )
      sgr_put(cap.reset);
  } else
    sgr = cap.reset;

  if (sgr) sgr_put(sgr);
}

void tty_observer::update_sgrcolor(
  int colorSpaceNew, color_t colorNew,
  int colorSpaceOld, color_t colorOld,
  termcap_sgrcolor const& sgrcolor
) {
  if (colorSpaceNew == colorSpaceOld && colorNew == colorOld) return;

  // sgrAnsiColor, sgrAixColor
  if (colorSpaceNew == color_spec_indexed) {
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

  if (colorSpaceNew == color_spec_default && sgrcolor.off) {
    sgr_put(sgrcolor.off);
    return;
  }

  // sgrISO8613_6Color
  if (sgrcolor.iso8613.sgr
    && (sgrcolor.iso8613.color_specs & 1 << colorSpaceNew)
  ) {
    sgr_put(sgrcolor.iso8613.sgr);
    put(sgrcolor.iso8613.separater);
    put_unsigned(colorSpaceNew);

    if (colorSpaceNew == color_spec_indexed) {
      if (colorNew <= sgrcolor.iso8613.max_index) {
        put(sgrcolor.iso8613.separater);
        put_unsigned(colorNew);
      }
      return;
    } else {
      int numberOfComponents = 0;
      switch (colorSpaceNew) {
      case color_spec_rgb:
      case color_spec_cmy:  numberOfComponents = 3; break;
      case color_spec_cmyk: numberOfComponents = 4; break;
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

void tty_observer::apply_attr(board const& w, attribute_t newAttr) {
  if (this->m_attr == newAttr) return;

  if (newAttr == 0) {
    sgr_clear();
    this->m_attr = 0;
    this->m_xattr.clear();
  } else {
    this->sgr_isOpen = false;

    extended_attribute _xattr;
    if (newAttr & has_extended_attribute)
      _xattr = *w.m_xattr_data.get(newAttr);
    else
      _xattr.load(newAttr);

    aflags_t const aremoved = ~_xattr.aflags & m_xattr.aflags;
    xflags_t const xremoved = ~_xattr.xflags & m_xattr.xflags;
    if (aremoved & sgrcap->aflagsNotResettable
      || xremoved & sgrcap->xflagsNotResettable
    ) {
      sgr_put(0);
      m_xattr.aflags = 0;
      m_xattr.xflags &= non_sgr_xflags_mask;
    }

    update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap->cap_bold     );
    update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap->cap_italic   );
    update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap->cap_underline);
    update_sgrflag2(_xattr.aflags, m_xattr.aflags, sgrcap->cap_blink    );

    update_sgrflag1(_xattr.aflags, m_xattr.aflags, sgrcap->cap_inverse  );
    update_sgrflag1(_xattr.aflags, m_xattr.aflags, sgrcap->cap_invisible);
    update_sgrflag1(_xattr.aflags, m_xattr.aflags, sgrcap->cap_strike   );

    if ((newAttr | this->m_attr) & has_extended_attribute) {
      update_sgrflag2(_xattr.xflags, m_xattr.xflags, sgrcap->cap_framed);
      update_sgrflag1(_xattr.xflags, m_xattr.xflags, sgrcap->cap_proportional);
      update_sgrflag1(_xattr.xflags, m_xattr.xflags, sgrcap->cap_overline);
      update_ideogram_decoration(_xattr.xflags, m_xattr.xflags, sgrcap->cap_ideogram);
    }

    update_sgrcolor(
      _xattr.fg_space(),  _xattr.fg,
      m_xattr.fg_space(), m_xattr.fg,
      sgrcap->cap_fg);

    update_sgrcolor(
      _xattr.bg_space(),  _xattr.bg,
      m_xattr.bg_space(), m_xattr.bg,
      sgrcap->cap_bg);

    this->m_attr = newAttr;
    this->m_xattr = _xattr;

    if (this->sgr_isOpen) put(ascii_m);
  }
}
