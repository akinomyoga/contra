#include "tty_player.h"
#include <algorithm>
#include <cstdio>


namespace contra {
namespace {

  static void reverse_line_content(board* b, curpos_t y) {
    board_cell* const cells = b->cell(0, y);
    board_cell* const cellN = cells + b->m_width;
    std::reverse(cells, cells + b->m_width);

    // correct wide characters
    for (board_cell* cell = cells; cell < cellN - 1; cell++) {
      board_cell* beg = cell;
      while (cell + 1 < cellN && cell->character & is_wide_extension) cell++;
      if (cell != beg) std::swap(*beg, *cell);
    }

    // ToDo: directed strings
  }

  bool do_spd(tty_player& play, csi_parameters& params) {
    csi_single_param_t direction;
    params.read_param(direction, 0);
    csi_single_param_t update;
    params.read_param(update, 0);
    if (direction > 7 || update > 2) return false;

    tty_state* const s = play.state();
    board* const b = play.board();

    /* update content
     *
     * Note: update == 2 の場合、見た目が変わらない様にデータを更新する。
     *   但し、縦書き・横書きの変更の場合には、
     *   横書きの各行が縦書きの各行になる様にする。
     *   また、line progression の変更も適用しない。
     *   単に、character path ltor/ttob <-> rtol/btot の間の変換だけ行う。
     *   今後この動作は変更する可能性がある。
     *
     */
    bool const oldRToL = is_charpath_rtol(s->presentation_direction);
    bool const newRToL = is_charpath_rtol((presentation_direction) direction);
    if (oldRToL != newRToL && update == 2) {
      for (curpos_t y = 0, yN = b->m_height; y < yN; y++) {
        if (!(b->line(y)->lflags & character_path_mask))
          reverse_line_content(b, y);
      }
    }

    s->presentation_direction = (presentation_direction) direction;

    // update position
    b->cur.y = 0;
    if (update == 2) {
      board_line* const line = b->line(b->cur.y);
      b->cur.x = line->to_data_position(0, line->is_rtol(s->presentation_direction));
    } else {
      b->cur.x = 0;
    }

    return true;
  }

  bool do_scp(tty_player& play, csi_parameters& params) {
    csi_single_param_t charPath;
    params.read_param(charPath, 0);
    csi_single_param_t update;
    params.read_param(update, 0);
    if (charPath > 2 || update > 2) return false;

    tty_state* const s = play.state();
    board* const b = play.board();
    board_line* const line = b->line(b->cur.y);

    bool const oldRToL = line->is_rtol(s->presentation_direction);

    // update line attributes
    play.apply_line_attribute(line);
    line->lflags &= ~(line_attr_t) character_path_mask;
    s->lflags &= ~(line_attr_t) character_path_mask;
    switch (charPath) {
    case 1:
      line->lflags |= is_character_path_ltor;
      s->lflags |= is_character_path_ltor;
      break;
    case 2:
      line->lflags |= is_character_path_rtol;
      s->lflags |= is_character_path_rtol;
      break;
    }

    bool const newRToL = line->is_rtol(s->presentation_direction);

    // update line content
    if (oldRToL != newRToL && update == 2)
      reverse_line_content(b, b->cur.y);

    if (update == 2)
      b->cur.x = line->to_data_position(0, newRToL);
    else
      b->cur.x = 0;

    return true;
  }

  bool do_simd(tty_player& play, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 1) return false;
    play.state()->set_mode(mode_simd, param != 0);
    return true;
  }

  bool do_slh(tty_player& play, csi_parameters& params) {
    tty_state * const s = play.state();
    board     * const b = play.board();
    board_line* const line = b->line(b->cur.y);

    csi_single_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const x = (curpos_t) param - 1;
      line->home = x;
      if (line->limit >= 0 && line->limit < x)
        line->limit = x;

      s->line_home = x;
      if (s->line_limit >= 0 && s->line_limit < x)
        s->line_limit = x;
    } else {
      line->home = -1;
      s->line_home = -1;
    }

    return true;
  }

  bool do_sll(tty_player& play, csi_parameters& params) {
    tty_state * const s = play.state();
    board     * const b = play.board();
    board_line* const line = b->line(b->cur.y);

    csi_single_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const x = (curpos_t) param - 1;
      line->limit = x;
      if (line->home >= 0 && line->home > x)
        line->home = x;

      s->line_limit = x;
      if (s->line_home >= 0 && s->line_home > x)
        s->line_home = x;
    } else {
      line->limit = -1;
      s->line_limit = -1;
    }

    return true;
  }

  bool do_sph(tty_player& play, csi_parameters& params) {
    tty_state * const s = play.state();

    csi_single_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const y = (curpos_t) param - 1;
      s->page_home = y;
      if (s->page_limit >= 0 && s->page_limit < y)
        s->page_limit = y;
    } else {
      s->page_home = -1;
    }
    return true;
  }

  bool do_spl(tty_player& play, csi_parameters& params) {
    tty_state * const s = play.state();

    csi_single_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const y = (curpos_t) param - 1;
      s->page_limit = y;
      if (s->page_home >= 0 && s->page_home > y)
        s->page_home = y;
    } else {
      s->page_limit = -1;
    }
    return true;
  }

  enum do_cux_direction {
    do_cux_prec_char = 0,
    do_cux_succ_char = 1,
    do_cux_prec_line = 2,
    do_cux_succ_line = 3,

    do_cux_shift = 2,
    do_cux_mask  = 0x3,
  };

  static constexpr std::uint32_t do_cux_vec_construct(presentation_direction dir, do_cux_direction value) {
    return value << do_cux_shift * dir;
  }
  static do_cux_direction do_cux_vec_select(std::uint32_t vec, presentation_direction value) {
    return do_cux_direction(vec >> do_cux_shift * (0 <= value && value < 8? value: 0) & do_cux_mask);
  }

  static bool do_cux(tty_player& play, csi_parameters& params, do_cux_direction direction, bool isData) {
    tty_state * const s = play.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s->get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }

    board* const b = play.board();

    curpos_t y = b->cur.y;
    switch (direction) {
    case do_cux_prec_char:
      {
        board_line* line = b->line(y);
        curpos_t x = b->cur.x;
        if (!isData)
          x = line->to_presentation_position(x, s->presentation_direction);
        x = std::max((curpos_t) 0, x - (curpos_t) param);
        if (!isData)
          x = line->to_data_position(x, s->presentation_direction);
        b->cur.x = x;
      }
      break;
    case do_cux_succ_char:
      {
        board_line* line = b->line(y);
        curpos_t x = b->cur.x;
        if (!isData)
          x = line->to_presentation_position(x, s->presentation_direction);
        x = std::min(play.board()->m_width - 1, x + (curpos_t) param);
        if (!isData)
          x = line->to_data_position(x, s->presentation_direction);
        b->cur.x = x;
      }
      break;
    case do_cux_prec_line:
      b->cur.y = std::max((curpos_t) 0, y - (curpos_t) param);
      break;
    case do_cux_succ_line:
      b->cur.y = std::min(play.board()->m_height - 1, y + (curpos_t) param);
      break;
    }

    return true;
  }

  bool do_cuu(tty_player& play, csi_parameters& params) {
    constexpr std::uint32_t vec
      = do_cux_vec_construct(presentation_direction_tblr, do_cux_prec_char)
      | do_cux_vec_construct(presentation_direction_tbrl, do_cux_prec_char)
      | do_cux_vec_construct(presentation_direction_btlr, do_cux_succ_char)
      | do_cux_vec_construct(presentation_direction_btrl, do_cux_succ_char)
      | do_cux_vec_construct(presentation_direction_lrtb, do_cux_prec_line)
      | do_cux_vec_construct(presentation_direction_rltb, do_cux_prec_line)
      | do_cux_vec_construct(presentation_direction_lrbt, do_cux_succ_line)
      | do_cux_vec_construct(presentation_direction_rlbt, do_cux_succ_line);

    return do_cux(
      play, params,
      do_cux_vec_select(vec, play.state()->presentation_direction),
      false);
  }
  bool do_cud(tty_player& play, csi_parameters& params) {
    constexpr std::uint32_t vec
      = do_cux_vec_construct(presentation_direction_btlr, do_cux_prec_char)
      | do_cux_vec_construct(presentation_direction_btrl, do_cux_prec_char)
      | do_cux_vec_construct(presentation_direction_tblr, do_cux_succ_char)
      | do_cux_vec_construct(presentation_direction_tbrl, do_cux_succ_char)
      | do_cux_vec_construct(presentation_direction_lrbt, do_cux_prec_line)
      | do_cux_vec_construct(presentation_direction_rlbt, do_cux_prec_line)
      | do_cux_vec_construct(presentation_direction_lrtb, do_cux_succ_line)
      | do_cux_vec_construct(presentation_direction_rltb, do_cux_succ_line);

    return do_cux(
      play, params,
      do_cux_vec_select(vec, play.state()->presentation_direction),
      false);
  }
  bool do_cuf(tty_player& play, csi_parameters& params) {
    constexpr std::uint32_t vec
      = do_cux_vec_construct(presentation_direction_rltb, do_cux_prec_char)
      | do_cux_vec_construct(presentation_direction_rlbt, do_cux_prec_char)
      | do_cux_vec_construct(presentation_direction_lrtb, do_cux_succ_char)
      | do_cux_vec_construct(presentation_direction_lrbt, do_cux_succ_char)
      | do_cux_vec_construct(presentation_direction_tbrl, do_cux_prec_line)
      | do_cux_vec_construct(presentation_direction_btrl, do_cux_prec_line)
      | do_cux_vec_construct(presentation_direction_tblr, do_cux_succ_line)
      | do_cux_vec_construct(presentation_direction_btlr, do_cux_succ_line);

    return do_cux(
      play, params,
      do_cux_vec_select(vec, play.state()->presentation_direction),
      false);
  }
  bool do_cub(tty_player& play, csi_parameters& params) {
    constexpr std::uint32_t vec
      = do_cux_vec_construct(presentation_direction_lrtb, do_cux_prec_char)
      | do_cux_vec_construct(presentation_direction_lrbt, do_cux_prec_char)
      | do_cux_vec_construct(presentation_direction_rltb, do_cux_succ_char)
      | do_cux_vec_construct(presentation_direction_rlbt, do_cux_succ_char)
      | do_cux_vec_construct(presentation_direction_tblr, do_cux_prec_line)
      | do_cux_vec_construct(presentation_direction_btlr, do_cux_prec_line)
      | do_cux_vec_construct(presentation_direction_tbrl, do_cux_succ_line)
      | do_cux_vec_construct(presentation_direction_btrl, do_cux_succ_line);

    return do_cux(
      play, params,
      do_cux_vec_select(vec, play.state()->presentation_direction),
      false);
  }

  bool do_hpb(tty_player& play, csi_parameters& params) {
    return do_cux(play, params, do_cux_prec_char, true);
  }
  bool do_hpr(tty_player& play, csi_parameters& params) {
    return do_cux(play, params, do_cux_succ_char, true);
  }
  bool do_vpb(tty_player& play, csi_parameters& params) {
    return do_cux(play, params, do_cux_prec_line, true);
  }
  bool do_vpr(tty_player& play, csi_parameters& params) {
    return do_cux(play, params, do_cux_succ_line, true);
  }

  static bool do_cup(board* b, curpos_t x, curpos_t y, presentation_direction presentationDirection) {
    b->cur.x = b->line(y)->to_data_position(x, presentationDirection);
    b->cur.y = y;
    return true;
  }

  bool do_cnl(tty_player& play, csi_parameters& params) {
    tty_state * const s = play.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0 && s->get_mode(mode_zdm)) param = 1;

    board* const b = play.board();
    curpos_t const y = std::min(b->cur.y + (curpos_t) param, b->m_width - 1);
    return do_cup(b, 0, y, s->presentation_direction);
  }

  bool do_cpl(tty_player& play, csi_parameters& params) {
    tty_state * const s = play.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0 && s->get_mode(mode_zdm)) param = 1;

    board* const b = play.board();
    curpos_t const y = std::max(b->cur.y - (curpos_t) param, 0);
    return do_cup(b, 0, y, s->presentation_direction);
  }

  bool do_cup(tty_player& play, csi_parameters& params) {
    tty_state * const s = play.state();
    csi_single_param_t param1, param2;
    params.read_param(param1, 1);
    params.read_param(param2, 1);
    if (s->get_mode(mode_zdm)) {
      if (param1 == 0) param1 = 1;
      if (param2 == 0) param2 = 1;
    }

    if (param1 == 0 || param2 == 0) return false;

    return do_cup(play.board(), (curpos_t) param2 - 1, (curpos_t) param1 - 1, s->presentation_direction);
  }

  bool do_cha(tty_player& play, csi_parameters& params) {
    tty_state * const s = play.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s->get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    board* const b = play.board();
    return do_cup(b, param - 1, b->cur.y, s->presentation_direction);
  }

  bool do_hpa(tty_player& play, csi_parameters& params) {
    tty_state * const s = play.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s->get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    play.board()->cur.x = param - 1;
    return true;
  }

  bool do_vpa(tty_player& play, csi_parameters& params) {
    tty_state * const s = play.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s->get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    play.board()->cur.y = param - 1;
    return true;
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

    if (10 <= param && param <= 19) {
      xflags = (xflags & ~(xflags_t) ansi_font_mask)
        | ((param - 10) << ansi_font_shift & ansi_font_mask);
      b->cur.xattr_dirty = true;
      return;
    }

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

  bool do_sgr(tty_player& play, csi_parameters& params) {
    if (params.size() == 0 || !play.state()->get_mode(mode_grcm))
      do_sgr_parameter(play, 0, params);

    csi_single_param_t value;
    while (params.read_param(value, 0))
      do_sgr_parameter(play, value, params);

    return true;
  }

}

  void tty_player::process_control_sequence(sequence const& seq) {
    if (seq.is_private_csi()) {
      print_unrecognized_sequence(seq);
      return;
    }

    csi_parameters params(seq);
    if (!params) {
      print_unrecognized_sequence(seq);
      return;
    }

    bool result = false;
    std::int32_t const intermediateSize = seq.intermediateSize();
    if (intermediateSize == 0) {
      switch (seq.final()) {
      case ascii_m:          result = do_sgr(*this, params); break;

      case ascii_A:          result = do_cuu(*this, params); break;
      case ascii_B:          result = do_cud(*this, params); break;
      case ascii_C:          result = do_cuf(*this, params); break;
      case ascii_D:          result = do_cub(*this, params); break;
      case ascii_j:          result = do_hpb(*this, params); break;
      case ascii_a:          result = do_hpr(*this, params); break;
      case ascii_k:          result = do_vpb(*this, params); break;
      case ascii_e:          result = do_vpr(*this, params); break;

      case ascii_E:          result = do_cnl(*this, params); break;
      case ascii_F:          result = do_cpl(*this, params); break;

      case ascii_G:          result = do_cha(*this, params); break;
      case ascii_H:          result = do_cup(*this, params); break;
      case ascii_back_quote: result = do_hpa(*this, params); break;
      case ascii_d:          result = do_vpa(*this, params); break;

      case ascii_circumflex: result = do_simd(*this, params); break;
      }
    } else if (intermediateSize == 1) {
      mwg_assert(seq.intermediate()[0] <= 0xFF);
      switch (compose_bytes((byte) seq.intermediate()[0], seq.final())) {
      case compose_bytes(ascii_sp, ascii_U): result = do_slh(*this, params); break;
      case compose_bytes(ascii_sp, ascii_V): result = do_sll(*this, params); break;
      case compose_bytes(ascii_sp, ascii_S): result = do_spd(*this, params); break;
      case compose_bytes(ascii_sp, ascii_k): result = do_scp(*this, params); break;
      case compose_bytes(ascii_sp, ascii_i): result = do_sph(*this, params); break;
      case compose_bytes(ascii_sp, ascii_j): result = do_spl(*this, params); break;
      }
    }

    if (!result)
      print_unrecognized_sequence(seq);
  }

}
