#include "tty_player.h"

namespace contra {

  bool do_slh(tty_player& play, sequence const& seq) {
    if (seq.is_private_csi()) {
      std::fprintf(stderr, "private SLH not supported\n");
      return false;
    }

    // TODO
    return false;
  }
  bool do_sll(tty_player& play, sequence const& seq) {
    if (seq.is_private_csi()) {
      std::fprintf(stderr, "private SLH not supported\n");
      return false;
    }

    // TODO
    return false;
  }

  static void do_sgr_iso8613_colors(tty_player& play, csi_parameters& params, bool isfg) {
    csi_single_param_t colorSpace;
    params.read_arg(colorSpace, true, 0);
    color_t color;
    switch (colorSpace) {
    case color_spec_default:
    default:
      if (isfg)
        play.reset_fg();
      else
        play.reset_bg();
      break;

    case color_spec_transparent:
      color = 0;
      goto set_color;

    case color_spec_rgb:
    case color_spec_cmy:
    case color_spec_cmyk:
      {
        int const ncomp = colorSpace == color_spec_cmyk ? 4: 3;
        csi_single_param_t comp;
        for (int i = 0; i < ncomp; i++) {
          params.read_arg(comp, true, 0);
          if (comp > 255) comp = 255;
          color |= comp << i * 8;
        }

        if (params.read_arg(comp, false, 0)) {
          // If there are more than `ncomp` colon-separated arguments,
          // switch to ISO 8613-6 compatible mode expecting sequences like
          //
          //     CSI 38:2:CS:R:G:B:dummy:tol:tolCS m
          //
          // In this mode, discard the first byte (CS = color space)
          // and append a new byte containing the last color component.
          color = color >> 8 | comp << (ncomp - 1) * 8;
        } else {
          // If there are no more than `ncomp` arguments,
          // switch to konsole/xterm compatible mode expecting sequences like
          //
          //   CSI 38;2;R;G;B m
          //
          // In this mode, do nothing here; We have already correct `color`.
        }
        goto set_color;
      }

    case color_spec_indexed:
      if (params.read_arg(color, true, 0))
        goto set_color;
      else
        std::fprintf(stderr, "missing argument for SGR 38:5\n");
      break;

    set_color:
      if (isfg)
        play.set_fg(color, colorSpace);
      else
        play.set_bg(color, colorSpace);
      break;
    }
  }

  static void do_sgr_parameter(tty_player& play, csi_single_param_t param, csi_parameters& rest) {
    if (30 <= param && param < 40) {
      if (param < 38) {
        play.set_fg(param - 30);
      } else if (param == 38) {
        do_sgr_iso8613_colors(play, rest, true);
      } else {
        play.reset_fg();
      }
      return;
    } else if (40 <= param && param < 50) {
      if (param < 48) {
        play.set_bg(param - 40);
      } else if (param == 48) {
        do_sgr_iso8613_colors(play, rest, false);
      } else {
        play.reset_bg();
      }
      return;
    } else if (90 <= param && param <= 97) {
      play.set_fg(8 + (param - 90));
      return;
    } else if (100 <= param && param <= 107) {
      play.set_bg(8 + (param - 100));
      return;
    }

    board* b = play.board();
    aflags_t& aflags = b->cur.xattr_edit.aflags;
    xflags_t& xflags = b->cur.xattr_edit.xflags;

    switch (param) {
    case 0:
      b->cur.xattr_edit.aflags = 0;
      b->cur.xattr_edit.xflags &= non_sgr_xflags_mask;
      b->cur.xattr_edit.fg = 0;
      b->cur.xattr_edit.bg = 0;
      break;

    case 1 : aflags = (aflags & ~(attribute_t) is_faint_set) | is_bold_set; break;
    case 2 : aflags = (aflags & ~(attribute_t) is_bold_set) | is_faint_set; break;
    case 22: aflags = aflags & ~(attribute_t) (is_bold_set | is_faint_set); break;

    case 3 : aflags = (aflags & ~(attribute_t) is_fraktur_set) | is_italic_set; break;
    case 20: aflags = (aflags & ~(attribute_t) is_italic_set) | is_fraktur_set; break;
    case 23: aflags = aflags & ~(attribute_t) (is_italic_set | is_fraktur_set); break;

    case 4 : aflags = (aflags & ~(attribute_t) is_double_underline_set) | is_underline_set; break;
    case 21: aflags = (aflags & ~(attribute_t) is_underline_set) | is_double_underline_set; break;
    case 24: aflags = aflags & ~(attribute_t) (is_underline_set | is_double_underline_set); break;

    case 5 : aflags = (aflags & ~(attribute_t) is_rapid_blink_set) | is_blink_set; break;
    case 6 : aflags = (aflags & ~(attribute_t) is_blink_set) | is_rapid_blink_set; break;
    case 25: aflags = aflags & ~(attribute_t) (is_blink_set | is_rapid_blink_set); break;

    case 7 : aflags = aflags | is_inverse_set; break;
    case 27: aflags = aflags & ~(attribute_t) is_inverse_set; break;

    case 8 : aflags = aflags | is_invisible_set; break;
    case 28: aflags = aflags & ~(attribute_t) is_invisible_set; break;

    case 9 : aflags = aflags | is_strike_set; break;
    case 29: aflags = aflags & ~(attribute_t) is_strike_set; break;

    case 26: xflags = xflags | is_proportional_set; break;
    case 50: xflags = xflags & ~(attribute_t) is_proportional_set; break;

    case 51: xflags = (xflags & ~(attribute_t) is_circle_set) | is_frame_set; break;
    case 52: xflags = (xflags & ~(attribute_t) is_frame_set) | is_circle_set; break;
    case 54: xflags = xflags & ~(attribute_t) (is_frame_set | is_circle_set); break;

    case 53: xflags = xflags | is_overline_set; break;
    case 55: xflags = xflags & ~(attribute_t) is_overline_set; break;

    case 60: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_single_rb_set; break;
    case 61: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_double_rb_set; break;
    case 62: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_single_lt_set; break;
    case 63: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_double_lt_set; break;
    case 66: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_single_lb_set; break;
    case 67: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_double_lb_set; break;
    case 68: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_single_rt_set; break;
    case 69: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_double_rt_set; break;
    case 64: xflags = (xflags & ~(attribute_t) is_ideogram_decoration_mask) | is_ideogram_stress_set   ; break;
    case 65: xflags = xflags & ~(attribute_t) is_ideogram_decoration_mask; break;

    default:
      std::fprintf(stderr, "unrecognized SGR value %d\n", param);
      break;
    }

    b->cur.xattr_dirty = true;
  }

  bool do_sgr(tty_player& play, sequence const& seq) {
    if (seq.is_private_csi()) {
      std::fprintf(stderr, "private SGR not supported\n");
      // todo print escape sequences
      return false;
    }

    csi_parameters params(seq);
    if (!params) return false;

    if (params.size() == 0)
      params.push_back(csi_param_type {0, false, true});

    csi_single_param_t value;
    while (params.read_param(value, 0))
      do_sgr_parameter(play, value, params);

    return true;
  }

}
