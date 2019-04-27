#include "term.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <mwg/except.h>
#include "../sequence.h"

namespace contra {
namespace ansi {
namespace {

  class csi_parameters;

  typedef bool control_function_t(term_t& term, csi_parameters& params);

  struct csi_param_type {
    csi_single_param_t value;
    bool isColon;
    bool isDefault;
  };

  class csi_parameters {
    std::vector<csi_param_type> m_data;
    std::size_t m_index {0};
    bool m_result;

  public:
    csi_parameters(char32_t const* s, std::size_t n) { extract_parameters(s, n); }
    csi_parameters(sequence const& seq) {
      extract_parameters(seq.parameter(), seq.parameter_size());
    }

    // csi_parameters(std::initializer_list<csi_single_param_t> args) {
    //   for (csi_single_param_t value: args)
    //     this->push_back({value, false, false});
    // }

    operator bool() const {return this->m_result;}

  public:
    std::size_t size() const {return m_data.size();}
    void push_back(csi_param_type const& value) {m_data.push_back(value);}

  private:
    bool extract_parameters(char32_t const* str, std::size_t len) {
      m_data.clear();
      m_index = 0;
      m_isColonAppeared = false;
      m_result = false;

      bool isSet = false;
      csi_param_type param {0, false, true};
      for (std::size_t i = 0; i < len; i++) {
        char32_t const c = str[i];
        if (!(ascii_0 <= c && c <= ascii_semicolon)) {
          std::fprintf(stderr, "invalid value of CSI parameter values.\n");
          return false;
        }

        if (c <= ascii_9) {
          std::uint32_t const newValue = param.value * 10 + (c - ascii_0);
          if (newValue < param.value) {
            // overflow
            std::fprintf(stderr, "a CSI parameter value is too large.\n");
            return false;
          }

          isSet = true;
          param.value = newValue;
          param.isDefault = false;
        } else {
          m_data.push_back(param);
          isSet = true;
          param.value = 0;
          param.isColon = c == ascii_colon;
          param.isDefault = true;
        }
      }

      if (isSet) m_data.push_back(param);
      return m_result = true;
    }

  public:
    bool m_isColonAppeared;

    bool read_param(csi_single_param_t& result, std::uint32_t defaultValue) {
      while (m_index < m_data.size()) {
        csi_param_type const& param = m_data[m_index++];
        if (!param.isColon) {
          m_isColonAppeared = false;
          if (!param.isDefault)
            result = param.value;
          else
            result = defaultValue;
          return true;
        }
      }
      result = defaultValue;
      return false;
    }

    bool read_arg(csi_single_param_t& result, bool toAllowSemicolon, csi_single_param_t defaultValue) {
      if (m_index <m_data.size()
        && (m_data[m_index].isColon || (toAllowSemicolon && !m_isColonAppeared))
      ) {
        csi_param_type const& param = m_data[m_index++];
        if (param.isColon) m_isColonAppeared = true;
        if (!param.isDefault)
          result = param.value;
        else
          result = defaultValue;
        return true;
      }

      result = defaultValue;
      return false;
    }
  };
}

  //---------------------------------------------------------------------------
  // Modes

  void tty_state::initialize_mode() {
    std::fill(std::begin(m_mode_flags), std::end(m_mode_flags), 0);
#include "../../out/gen/term.mode_init.hpp"
  }

  struct mode_dictionary_t {
    std::unordered_map<std::uint32_t, mode_t> data_ansi;
    std::unordered_map<std::uint32_t, mode_t> data_dec;

  public:
    mode_dictionary_t() {
#include "../../out/gen/term.mode_register.hpp"
    }

    void set_ansi_mode(tty_state& s, csi_single_param_t param, bool value) {
      auto const it = data_ansi.find(param);
      if (it != data_ansi.end())
        s.set_mode(it->second, value);
      else
        std::fprintf(stderr, "unrecognized ANSI mode %u\n", (unsigned) param);
    }
    void set_dec_mode(tty_state& s, csi_single_param_t param, bool value) {
      auto const it = data_dec.find(param);
      if (it != data_dec.end())
        s.set_mode(it->second, value);
      else
        std::fprintf(stderr, "unrecognized DEC mode %u\n", (unsigned) param);
    }
  };
  static mode_dictionary_t mode_dictionary;

  bool do_sm(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_ansi_mode(s, value, true);
    return true;
  }
  bool do_rm(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_ansi_mode(s, value, false);
    return true;
  }
  bool do_decset(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_dec_mode(s, value, true);
    return true;
  }
  bool do_decrst(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t value;
    while (params.read_param(value, 0))
      mode_dictionary.set_dec_mode(s, value, false);
    return true;
  }

  bool do_decscusr(term_t& term, csi_parameters& params) {
    csi_single_param_t spec;
    params.read_param(spec, 0);
    tty_state& s = term.state();
    auto _set = [&s] (bool blink, int shape) {
      s.set_mode(mode_attCursorBlink, blink);
      s.m_cursor_shape = shape;
    };

    switch (spec) {
    case 0:
    case 1: _set(true ,  0); break;
    case 2: _set(false,  0); break;
    case 3: _set(true ,  1); break;
    case 4: _set(false,  1); break;
    case 5: _set(true , -1); break;
    case 6: _set(false, -1); break;

    default:
      // Cygwin (mintty) percentage of cursor height
      if (spec >= 100)
        s.m_cursor_shape = 0; // block
      else
        s.m_cursor_shape = spec;
      break;
    }
    return true;
  }

  void do_altscreen(term_t& term, bool value) {
    tty_state& s = term.state();
    board_t& b = term.board();
    if (value != s.get_mode(mode_altscr)) {
      s.set_mode(mode_altscr, value);
      s.altscreen.reset_size(b.m_width, b.m_height);
      s.altscreen.cur = b.cur;
      s.altscreen.m_line_count = b.m_line_count;
      std::swap(s.altscreen, b);
    }
  }

  void do_deckpam(term_t& term) {
    term.state().set_mode(mode_decnkm, true);
  }
  void do_deckpnm(term_t& term) {
    term.state().set_mode(mode_decnkm, false);
  }

  void do_sm_deccolm(term_t& term, bool value) {
    tty_state& s = term.state();
    if (s.get_mode(mode_xtEnableColm)) {
      board_t& b = term.board();
      b.reset_size(value ? 132 : 80, b.m_height);
      if (!s.get_mode(mode_decncsm)) b.clear_screen();
      s.clear_margin();
    }
  }
  int do_rqm_deccolm(term_t& term) {
    return term.board().m_width == 132 ? 1 : 2;
  }
  bool do_decscpp(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    if (!s.cfg_decscpp_enabled) return true;

    csi_single_param_t param;
    params.read_param(param, 0);
    if (param == 0) param = 80;
    curpos_t cols = contra::clamp<curpos_t>(param, s.cfg_decscpp_min, s.cfg_decscpp_max);

    board_t& b = term.board();
    b.reset_size(cols, b.m_height);

    return true;
  }

  //---------------------------------------------------------------------------
  // Page and line settings

  bool do_spd(term_t& term, csi_parameters& params) {
    csi_single_param_t direction;
    params.read_param(direction, 0);
    csi_single_param_t update;
    params.read_param(update, 0);
    if (direction > 7 || update > 2) return false;

    board_t& b = term.board();

    bool const oldRToL = is_charpath_rtol(b.m_presentation_direction);
    bool const newRToL = is_charpath_rtol(b.m_presentation_direction = (presentation_direction) direction);

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
    if (oldRToL != newRToL && update == 2) {
      for (curpos_t y = 0, yN = b.m_height; y < yN; y++) {
        line_t& line = b.m_lines[y];
        if (!(line.lflags() & line_attr_t::character_path_mask))
          line.reverse(b.m_width);
      }
    }

    // update position
    b.cur.y = 0;
    if (update == 2) {
      b.cur.x = b.to_data_position(b.cur.y, 0);
    } else {
      b.cur.x = 0;
    }

    return true;
  }

  bool do_scp(term_t& term, csi_parameters& params) {
    csi_single_param_t charPath;
    params.read_param(charPath, 0);
    csi_single_param_t update;
    params.read_param(update, 0);
    if (charPath > 2 || update > 2) return false;

    tty_state& s = term.state();
    board_t& b = term.board();
    line_t& line = b.line();

    bool const oldRToL = line.is_r2l(b.m_presentation_direction);

    // update line attributes
    term.initialize_line(line);
    line.lflags() &= ~line_attr_t::character_path_mask;
    s.lflags &= ~line_attr_t::character_path_mask;
    switch (charPath) {
    case 1:
      line.lflags() |= line_attr_t::is_character_path_ltor;
      s.lflags |= line_attr_t::is_character_path_ltor;
      break;
    case 2:
      line.lflags() |= line_attr_t::is_character_path_rtol;
      s.lflags |= line_attr_t::is_character_path_rtol;
      break;
    }

    bool const newRToL = line.is_r2l(b.m_presentation_direction);

    // update line content
    if (oldRToL != newRToL && update == 2)
      b.line().reverse(b.m_width);

    if (update == 2)
      b.cur.x = b.to_data_position(b.cur.y, 0);
    else
      b.cur.x = 0;

    return true;
  }

  bool do_simd(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 1) return false;
    term.state().set_mode(mode_simd, param != 0);
    return true;
  }

  bool do_slh(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    board_t& b = term.board();
    line_t& line = b.line();

    csi_single_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const x = (curpos_t) param - 1;
      line.home() = x;
      if (line.limit() >= 0 && line.limit() < x)
        line.limit() = x;

      s.line_home = x;
      if (s.line_limit >= 0 && s.line_limit < x)
        s.line_limit = x;
    } else {
      line.home() = -1;
      s.line_home = -1;
    }

    return true;
  }

  bool do_sll(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    board_t& b = term.board();
    line_t& line = b.line();

    csi_single_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const x = (curpos_t) param - 1;
      line.limit() = x;
      if (line.home() >= 0 && line.home() > x)
        line.home() = x;

      s.line_limit = x;
      if (s.line_home >= 0 && s.line_home > x)
        s.line_home = x;
    } else {
      line.limit() = -1;
      s.line_limit = -1;
    }

    return true;
  }

  bool do_sph(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();

    csi_single_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const y = (curpos_t) param - 1;
      s.page_home = y;
      if (s.page_limit >= 0 && s.page_limit < y)
        s.page_limit = y;
    } else {
      s.page_home = -1;
    }
    return true;
  }

  bool do_spl(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();

    csi_single_param_t param;
    if (params.read_param(param, 0) && param) {
      curpos_t const y = (curpos_t) param - 1;
      s.page_limit = y;
      if (s.page_home >= 0 && s.page_home > y)
        s.page_home = y;
    } else {
      s.page_limit = -1;
    }
    return true;
  }

  bool do_decstbm(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    board_t& b = term.board();

    csi_single_param_t param1, param2;
    params.read_param(param1, 0);
    params.read_param(param2, 0);

    curpos_t const home = param1 == 0 ? 0 : std::min((curpos_t) param1 - 1, b.m_height);
    curpos_t const limit = param2 == 0 ? b.m_height : std::min((curpos_t) param2, b.m_height);
    if (home + 2 <= limit) {
      s.dec_tmargin = home == 0 ? -1 : home;
      s.dec_bmargin = limit == b.m_height ? -1 : limit;
      b.cur.x = 0;
      b.cur.y = 0;
    }
    return true;
  }

  bool do_decslrm(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    board_t& b = term.board();

    csi_single_param_t param1, param2;
    params.read_param(param1, 0);
    params.read_param(param2, 0);

    curpos_t const _min = param1 == 0 ? 0 : std::min((curpos_t) param1 - 1, b.m_width);
    curpos_t const _max = param2 == 0 ? b.m_width : std::min((curpos_t) param2, b.m_width);
    if (_min + 2 <= _max) {
      s.dec_lmargin = _min == 0 ? -1 : _min;
      s.dec_rmargin = _max == b.m_width ? -1 : _max;
      b.cur.x = 0;
      b.cur.y = 0;
    }
    return true;
  }

  //---------------------------------------------------------------------------
  // Strings

  bool do_sds(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 2) return false;
    term.insert_marker(
      param == 1 ? character_t::marker_sds_l2r :
      param == 2 ? character_t::marker_sds_r2l :
      character_t::marker_sds_end);
    return true;
  }
  bool do_srs(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 2) return false;
    term.insert_marker(
      param == 1 ? character_t::marker_srs_beg :
      character_t::marker_srs_end);
    return true;
  }

  //---------------------------------------------------------------------------
  // Cursor

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
    return do_cux_direction(vec >> do_cux_shift * (0 <= value && value < 8 ? value : 0) & do_cux_mask);
  }

  static void do_cux(term_t& term, csi_single_param_t param, do_cux_direction direction, bool isPresentation, bool check_stbm) {
    board_t& b = term.board();

    curpos_t y = b.cur.y;
    switch (direction) {
    case do_cux_prec_char:
      {
        curpos_t x = b.cur.x;
        if (isPresentation)
          x = b.to_presentation_position(y, x);
        x = std::max(x - (curpos_t) param, check_stbm ? term.lmargin() : (curpos_t) 0);
        if (isPresentation)
          x = b.to_data_position(y, x);
        b.cur.x = x;
      }
      break;
    case do_cux_succ_char:
      {
        curpos_t x = b.cur.x;
        if (isPresentation)
          x = b.to_presentation_position(y, x);
        x = std::min(x + (curpos_t) param, (check_stbm ? term.rmargin() : term.board().m_width) - 1);
        if (isPresentation)
          x = b.to_data_position(y, x);
        b.cur.x = x;
      }
      break;
    case do_cux_prec_line:
      b.cur.y = std::max(y - (curpos_t) param, check_stbm ? term.tmargin() : (curpos_t) 0);
      break;
    case do_cux_succ_line:
      b.cur.y = std::min(y + (curpos_t) param, (check_stbm ? term.bmargin() : term.board().m_height) - 1);
      break;
    }
  }
  static bool do_cux(term_t& term, csi_parameters& params, do_cux_direction direction, bool isPresentation, bool check_stbm) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }
    do_cux(term, param, direction, isPresentation, check_stbm);
    return true;
  }

  static constexpr std::uint32_t do_cux_vec_u
    = do_cux_vec_construct(presentation_direction_tblr, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_tbrl, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_btlr, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_btrl, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_lrtb, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_rltb, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_lrbt, do_cux_succ_line)
    | do_cux_vec_construct(presentation_direction_rlbt, do_cux_succ_line);
  static constexpr std::uint32_t do_cux_vec_d
    = do_cux_vec_construct(presentation_direction_btlr, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_btrl, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_tblr, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_tbrl, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_lrbt, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_rlbt, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_lrtb, do_cux_succ_line)
    | do_cux_vec_construct(presentation_direction_rltb, do_cux_succ_line);
  static constexpr std::uint32_t do_cux_vec_r
    = do_cux_vec_construct(presentation_direction_rltb, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_rlbt, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_lrtb, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_lrbt, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_tbrl, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_btrl, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_tblr, do_cux_succ_line)
    | do_cux_vec_construct(presentation_direction_btlr, do_cux_succ_line);
  static constexpr std::uint32_t do_cux_vec_l
    = do_cux_vec_construct(presentation_direction_lrtb, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_lrbt, do_cux_prec_char)
    | do_cux_vec_construct(presentation_direction_rltb, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_rlbt, do_cux_succ_char)
    | do_cux_vec_construct(presentation_direction_tblr, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_btlr, do_cux_prec_line)
    | do_cux_vec_construct(presentation_direction_tbrl, do_cux_succ_line)
    | do_cux_vec_construct(presentation_direction_btrl, do_cux_succ_line);

  bool do_cuu(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_u, term.board().m_presentation_direction);
    return do_cux(term, params, dir, true, true);
  }
  bool do_cud(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_d, term.board().m_presentation_direction);
    return do_cux(term, params, dir, true, true);
  }
  bool do_cuf(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_r, term.board().m_presentation_direction);
    return do_cux(term, params, dir, true, true);
  }
  bool do_cub(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_l, term.board().m_presentation_direction);
    return do_cux(term, params, dir, true, true);
  }

  bool do_hpb(term_t& term, csi_parameters& params) {
    return do_cux(term, params, do_cux_prec_char, false, false);
  }
  bool do_hpr(term_t& term, csi_parameters& params) {
    return do_cux(term, params, do_cux_succ_char, false, false);
  }
  bool do_vpb(term_t& term, csi_parameters& params) {
    return do_cux(term, params, do_cux_prec_line, false, false);
  }
  bool do_vpr(term_t& term, csi_parameters& params) {
    return do_cux(term, params, do_cux_succ_line, false, false);
  }

  // RLogin によると ADM-3 由来の動作として上下左右に動く。
  void do_adm3_fs(term_t& term) { do_cux(term, 1, do_cux_succ_char, false, false); }
  void do_adm3_gs(term_t& term) { do_cux(term, 1, do_cux_prec_char, false, false); }
  void do_adm3_rs(term_t& term) { do_cux(term, 1, do_cux_prec_char, false, false); }
  void do_adm3_us(term_t& term) { do_cux(term, 1, do_cux_succ_char, false, false); }

  static bool do_cup(board_t& b, curpos_t x, curpos_t y) {
    b.cur.x = b.to_data_position(y, x);
    b.cur.y = y;
    return true;
  }

  bool do_cnl(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0 && s.get_mode(mode_zdm)) param = 1;

    board_t& b = term.board();
    curpos_t const y = std::min(b.cur.y + (curpos_t) param, term.bmargin() - 1);
    return do_cup(b, 0, y);
  }

  bool do_cpl(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0 && s.get_mode(mode_zdm)) param = 1;

    board_t& b = term.board();
    curpos_t const y = std::max(b.cur.y - (curpos_t) param, term.tmargin());
    return do_cup(b, 0, y);
  }

  bool do_cup(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param1, param2;
    params.read_param(param1, 1);
    params.read_param(param2, 1);
    if (s.get_mode(mode_zdm)) {
      if (param1 == 0) param1 = 1;
      if (param2 == 0) param2 = 1;
    }

    if (param1 == 0 || param2 == 0) return false;

    return do_cup(term.board(), (curpos_t) param2 - 1, (curpos_t) param1 - 1);
  }

  bool do_hvp(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param1, param2;
    params.read_param(param1, 1);
    params.read_param(param2, 1);
    if (s.get_mode(mode_zdm)) {
      if (param1 == 0) param1 = 1;
      if (param2 == 0) param2 = 1;
    }

    if (param1 == 0 || param2 == 0) return false;
    board_t& b = term.board();
    b.cur.y = (curpos_t) param1 - 1;
    b.cur.x = (curpos_t) param2 - 1;
    return true;
  }

  bool do_cha(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    board_t& b = term.board();
    return do_cup(b, param - 1, b.cur.y);
  }

  bool do_hpa(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    term.board().cur.x = param - 1;
    return true;
  }

  bool do_vpa(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return false;
    }

    term.board().cur.y = param - 1;
    return true;
  }

  static void do_vertical_scroll(term_t& term, curpos_t shift, curpos_t tmargin, curpos_t bmargin, curpos_t lmargin, curpos_t rmargin, bool dcsm) {
    board_t& b = term.board();
    if (lmargin == 0 && rmargin == b.m_width) {
      b.shift_lines(tmargin, bmargin, shift);
    } else if (lmargin < rmargin) {
      // DECSLRM が設定されている時のスクロール。行内容を切り貼りする。
      line_segment_t segs[3];
      int iseg = 0, iseg_transfer;
      if (0 < lmargin)
        segs[iseg++] = line_segment_t({0, lmargin, line_segment_slice});
      segs[iseg_transfer = iseg++] = line_segment_t({lmargin, rmargin, line_segment_transfer});
      if (rmargin < b.m_width)
        segs[iseg++] = line_segment_t({rmargin, b.m_width, line_segment_slice});

      attribute_t const fill_attr = b.cur.fill_attr();
      if (shift > 0) {
        curpos_t ydst = bmargin - 1;
        curpos_t ysrc = ydst - shift;
        for (; tmargin <= ysrc; ydst--, ysrc--) {
          segs[iseg_transfer].source = &b.line(ysrc);
          segs[iseg_transfer].source_r2l = b.line_r2l(ysrc);
          b.line(ydst).compose_segments(segs, iseg, b.m_width, fill_attr, b.line_r2l(ydst), dcsm);
        }
        segs[iseg_transfer].type = line_segment_erase;
        for (; tmargin <= ydst; ydst--)
          b.line(ydst).compose_segments(segs, iseg, b.m_width, fill_attr, b.line_r2l(ydst), dcsm);
      } else if (shift < 0) {
        shift = -shift;
        curpos_t ydst = tmargin;
        curpos_t ysrc = tmargin + shift;
        for (; ysrc < bmargin; ydst++, ysrc++) {
          segs[iseg_transfer].source = &b.line(ysrc);
          segs[iseg_transfer].source_r2l = b.line_r2l(ysrc);
          b.line(ydst).compose_segments(segs, iseg, b.m_width, fill_attr, b.line_r2l(ydst), dcsm);
        }
        segs[iseg_transfer].type = line_segment_erase;
        for (; ydst < bmargin; ydst++)
          b.line(ydst).compose_segments(segs, iseg, b.m_width, fill_attr, b.line_r2l(ydst), dcsm);
      }
    }
  }
  void do_vertical_scroll(term_t& term, curpos_t shift, bool dcsm) {
    curpos_t const tmargin = term.tmargin();
    curpos_t const bmargin = term.bmargin();
    curpos_t const lmargin = term.lmargin();
    curpos_t const rmargin = term.rmargin();
    do_vertical_scroll(term, shift, tmargin, bmargin, lmargin, rmargin, dcsm);
  }

  static bool do_scroll(term_t& term, csi_parameters& params, do_cux_direction direction, bool dcsm = false) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }

    board_t& b = term.board();

    curpos_t const y = b.cur.y;
    curpos_t shift = 0;
    switch (direction) {
    case do_cux_prec_char:
      shift = -(curpos_t) param;
      goto shift_cells;
    case do_cux_succ_char:
      shift = (curpos_t) param;
      goto shift_cells;
    shift_cells:
      {
        curpos_t const p = b.to_presentation_position(y, b.cur.x);
        line_shift_flags flags = b.line_r2l() ? line_shift_flags::r2l : line_shift_flags::none;
        if (dcsm) flags |= line_shift_flags::dcsm;
        curpos_t const tmargin = term.tmargin();
        curpos_t const bmargin = term.bmargin();
        curpos_t const lmargin = term.lmargin();
        curpos_t const rmargin = term.rmargin();
        attribute_t const fill_attr = b.cur.fill_attr();
        for (curpos_t y = tmargin; y < bmargin; y++)
          b.line(y).shift_cells(lmargin, rmargin, shift, flags, b.m_width, fill_attr);
        b.cur.x = b.to_data_position(y, p);
      }
      break;

    case do_cux_prec_line:
      shift = -(curpos_t) param;
      goto shift_lines;
    case do_cux_succ_line:
      shift = (curpos_t) param;
      goto shift_lines;
    shift_lines:
      {
        curpos_t const p = b.to_presentation_position(y, b.cur.x);
        do_vertical_scroll(term, shift, dcsm);
        b.cur.x = b.to_data_position(y, p);
      }
      break;
    }

    return true;
  }

  bool do_su(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_u, term.board().m_presentation_direction);
    return do_scroll(term, params, dir);
  }
  bool do_sd(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_d, term.board().m_presentation_direction);
    return do_scroll(term, params, dir);
  }
  bool do_sr(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_r, term.board().m_presentation_direction);
    return do_scroll(term, params, dir);
  }
  bool do_sl(term_t& term, csi_parameters& params) {
    do_cux_direction const dir = do_cux_vec_select(do_cux_vec_l, term.board().m_presentation_direction);
    return do_scroll(term, params, dir);
  }

  //---------------------------------------------------------------------------
  // Cursor save

  bool do_scosc(term_t& term, csi_parameters& params) {
    (void) params;
    auto& s = term.state();
    auto& b = term.board();
    s.m_scosc_x = b.cur.x;
    s.m_scosc_y = b.cur.y;
    return true;
  }

  bool do_scorc(term_t& term, csi_parameters& params) {
    (void) params;
    auto& s = term.state();
    auto& b = term.board();
    if (s.m_scosc_x >= 0) {
      b.cur.x = s.m_scosc_x;
      b.cur.y = s.m_scosc_y;
    }
    return true;
  }

  void do_decsc(term_t& term) {
    auto& s = term.state();
    auto& b = term.board();
    s.m_decsc_cur = b.cur;
    s.m_decsc_decawm = s.get_mode(mode_decawm);
    s.m_decsc_decom = s.get_mode(mode_decom);
  }

  void do_decrc(term_t& term) {
    auto& s = term.state();
    auto& b = term.board();
    if (s.m_decsc_cur.x >= 0) {
      b.cur = s.m_decsc_cur;
      s.set_mode(mode_decawm, s.m_decsc_decawm);
      s.set_mode(mode_decom, s.m_decsc_decom);
    }
  }

  bool do_decslrm_or_scosc(term_t& term, csi_parameters& params) {
    if (params.size() == 0)
      return do_scosc(term, params);
    else
      return do_decslrm(term, params);
  }

  //---------------------------------------------------------------------------
  // SGR and other graphics

  static void do_sgr_iso8613_colors(board_t& b, csi_parameters& params, bool isfg) {
    csi_single_param_t colorSpace;
    params.read_arg(colorSpace, true, 0);
    color_t color;
    switch (colorSpace) {
    case attribute_t::color_space_default:
    default:
      if (isfg)
        b.cur.attribute.reset_fg();
      else
        b.cur.attribute.reset_bg();
      break;

    case attribute_t::color_space_transparent:
      color = 0;
      goto set_color;

    case attribute_t::color_space_rgb:
    case attribute_t::color_space_cmy:
    case attribute_t::color_space_cmyk:
      {
        color = 0;

        int const ncomp = colorSpace == attribute_t::color_space_cmyk ? 4: 3;
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

    case attribute_t::color_space_indexed:
      if (params.read_arg(color, true, 0))
        goto set_color;
      else
        std::fprintf(stderr, "missing argument for SGR 38:5\n");
      break;

    set_color:
      if (isfg)
        b.cur.attribute.set_fg(color, colorSpace);
      else
        b.cur.attribute.set_bg(color, colorSpace);
      break;
    }
  }

  static void do_sgr_parameter(term_t& term, csi_single_param_t param, csi_parameters& rest) {
    board_t& b = term.board();
    if (30 <= param && param < 40) {
      if (param < 38) {
        b.cur.attribute.set_fg(param - 30);
      } else if (param == 38) {
        do_sgr_iso8613_colors(b, rest, true);
      } else {
        b.cur.attribute.reset_fg();
      }
      return;
    } else if (40 <= param && param < 50) {
      if (param < 48) {
        b.cur.attribute.set_bg(param - 40);
      } else if (param == 48) {
        do_sgr_iso8613_colors(b, rest, false);
      } else {
        b.cur.attribute.reset_bg();
      }
      return;
    } else if (90 <= param && param <= 97) {
      b.cur.attribute.set_fg(8 + (param - 90));
      return;
    } else if (100 <= param && param <= 107) {
      b.cur.attribute.set_bg(8 + (param - 100));
      return;
    }

    aflags_t& aflags = b.cur.attribute.aflags;
    xflags_t& xflags = b.cur.attribute.xflags;

    typedef attribute_t _at;

    if (10 <= param && param <= 19) {
      xflags = (xflags & ~(xflags_t) _at::ansi_font_mask)
        | ((param - 10) << _at::ansi_font_shift & _at::ansi_font_mask);
      return;
    }

    switch (param) {
    case 0: b.cur.attribute.clear_sgr(); break;

    case 1 : aflags = (aflags & ~(aflags_t) _at::is_faint_set) | _at::is_bold_set; break;
    case 2 : aflags = (aflags & ~(aflags_t) _at::is_bold_set) | _at::is_faint_set; break;
    case 22: aflags = aflags & ~(aflags_t) (_at::is_bold_set | _at::is_faint_set); break;

    case 3 : aflags = (aflags & ~(aflags_t) _at::is_fraktur_set) | _at::is_italic_set; break;
    case 20: aflags = (aflags & ~(aflags_t) _at::is_italic_set) | _at::is_fraktur_set; break;
    case 23: aflags = aflags & ~(aflags_t) (_at::is_italic_set | _at::is_fraktur_set); break;

    case 4 : aflags = (aflags & ~(aflags_t) _at::is_double_underline_set) | _at::is_underline_set; break;
    case 21: aflags = (aflags & ~(aflags_t) _at::is_underline_set) | _at::is_double_underline_set; break;
    case 24: aflags = aflags & ~(aflags_t) (_at::is_underline_set | _at::is_double_underline_set); break;

    case 5 : aflags = (aflags & ~(aflags_t) _at::is_rapid_blink_set) | _at::is_blink_set; break;
    case 6 : aflags = (aflags & ~(aflags_t) _at::is_blink_set) | _at::is_rapid_blink_set; break;
    case 25: aflags = aflags & ~(aflags_t) (_at::is_blink_set | _at::is_rapid_blink_set); break;

    case 7 : aflags = aflags | _at::is_inverse_set; break;
    case 27: aflags = aflags & ~(aflags_t) _at::is_inverse_set; break;

    case 8 : aflags = aflags | _at::is_invisible_set; break;
    case 28: aflags = aflags & ~(aflags_t) _at::is_invisible_set; break;

    case 9 : aflags = aflags | _at::is_strike_set; break;
    case 29: aflags = aflags & ~(aflags_t) _at::is_strike_set; break;

    case 26: xflags = xflags | _at::is_proportional_set; break;
    case 50: xflags = xflags & ~(xflags_t) _at::is_proportional_set; break;

    case 51: xflags = (xflags & ~(xflags_t) _at::is_circle_set) | _at::is_frame_set; break;
    case 52: xflags = (xflags & ~(xflags_t) _at::is_frame_set) | _at::is_circle_set; break;
    case 54: xflags = xflags & ~(xflags_t) (_at::is_frame_set | _at::is_circle_set); break;

    case 53: xflags = xflags | _at::is_overline_set; break;
    case 55: xflags = xflags & ~(xflags_t) _at::is_overline_set; break;

    case 60: xflags = (xflags & ~(xflags_t) _at::is_ideogram_decoration_mask) | _at::is_ideogram_single_rb_set; break;
    case 61: xflags = (xflags & ~(xflags_t) _at::is_ideogram_decoration_mask) | _at::is_ideogram_double_rb_set; break;
    case 62: xflags = (xflags & ~(xflags_t) _at::is_ideogram_decoration_mask) | _at::is_ideogram_single_lt_set; break;
    case 63: xflags = (xflags & ~(xflags_t) _at::is_ideogram_decoration_mask) | _at::is_ideogram_double_lt_set; break;
    case 66: xflags = (xflags & ~(xflags_t) _at::is_ideogram_decoration_mask) | _at::is_ideogram_single_lb_set; break;
    case 67: xflags = (xflags & ~(xflags_t) _at::is_ideogram_decoration_mask) | _at::is_ideogram_double_lb_set; break;
    case 68: xflags = (xflags & ~(xflags_t) _at::is_ideogram_decoration_mask) | _at::is_ideogram_single_rt_set; break;
    case 69: xflags = (xflags & ~(xflags_t) _at::is_ideogram_decoration_mask) | _at::is_ideogram_double_rt_set; break;
    case 64: xflags = (xflags & ~(xflags_t) _at::is_ideogram_decoration_mask) | _at::is_ideogram_stress_set   ; break;
    case 65: xflags = xflags & ~(xflags_t) _at::is_ideogram_decoration_mask; break;

    default:
      std::fprintf(stderr, "unrecognized SGR value %d\n", param);
      break;
    }
  }

  bool do_sgr(term_t& term, csi_parameters& params) {
    if (params.size() == 0 || !term.state().get_mode(mode_grcm))
      do_sgr_parameter(term, 0, params);

    csi_single_param_t value;
    while (params.read_param(value, 0))
      do_sgr_parameter(term, value, params);

    return true;
  }

  bool do_sco(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 7) return false;

    xflags_t& xflags = term.board().cur.attribute.xflags;
    xflags = (xflags & ~(xflags_t) attribute_t::sco_mask) | param << attribute_t::sco_shift;
    return true;
  }

  void do_plu(term_t& term) {
    xflags_t& xflags = term.board().cur.attribute.xflags;
    if (xflags & attribute_t::is_sub_set)
      xflags &= ~(xflags_t) attribute_t::is_sub_set;
    else
      xflags |= attribute_t::is_sup_set;
  }

  void do_pld(term_t& term) {
    xflags_t& xflags = term.board().cur.attribute.xflags;
    if (xflags & attribute_t::is_sup_set)
      xflags &= ~(xflags_t) attribute_t::is_sup_set;
    else
      xflags |= attribute_t::is_sub_set;
  }

  bool do_decsca(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param > 2) return false;

    xflags_t& xflags = term.board().cur.attribute.xflags;
    if (param == 1)
      xflags |= (xflags_t) attribute_t::decsca_protected;
    else
      xflags &= ~(xflags_t) attribute_t::decsca_protected;
    return true;
  }

  void do_spa(term_t& term) {
    term.board().cur.attribute.xflags |= (xflags_t) attribute_t::spa_protected;
  }
  void do_epa(term_t& term) {
    term.board().cur.attribute.xflags &= ~(xflags_t) attribute_t::spa_protected;
  }
  void do_ssa(term_t& term) {
    term.board().cur.attribute.xflags |= (xflags_t) attribute_t::ssa_selected;
  }
  void do_esa(term_t& term) {
    term.board().cur.attribute.xflags &= ~(xflags_t) attribute_t::ssa_selected;
  }

  void do_sm_decscnm(term_t& term, bool value) {
    tty_state& s = term.state();
    if (s.get_mode(mode_decscnm_) == value) return;
    std::swap(s.m_default_fg, s.m_default_bg);
    std::swap(s.m_default_fg_space, s.m_default_bg_space);
    s.set_mode(mode_decscnm_, value);
  }
  int do_rqm_decscnm(term_t& term) {
    tty_state& s = term.state();
    return s.get_mode(mode_decscnm_) ? 1 : 2;
  }

  //---------------------------------------------------------------------------
  // ECH, DCH, ICH

  bool do_ech(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }

    board_t& b = term.board();
    line_shift_flags flags = b.line_r2l() ? line_shift_flags::r2l : line_shift_flags::none;
    if (s.get_mode(mode_erm)) flags |= line_shift_flags::erm;

    curpos_t x1;
    if (s.dcsm()) {
      // DCSM(DATA)
      flags |= line_shift_flags::dcsm;
      x1 = b.cur.x < b.m_width ? b.cur.x : s.get_mode(mode_xenl_ech) ? b.m_width - 1 : b.m_width;
    } else {
      // DCSM(PRESENTATION)
      x1 = b.to_presentation_position(b.cur.y, b.cur.x);
      if (x1 >= b.m_width) x1 = s.get_mode(mode_xenl_ech) ? b.m_width - 1 : b.m_width;
      b.cur.x = x1;
    }

    curpos_t const x2 = std::min(x1 + (curpos_t) param, b.m_width);
    b.line().shift_cells(x1, x2, x2 - x1, flags, b.m_width, b.cur.fill_attr());

    // カーソル位置
    if (!s.dcsm())
      x1 = b.to_data_position(b.cur.y, x1);
    b.cur.x = x1;

    return true;
  }

  static void do_ich_impl(term_t& term, curpos_t shift) {
    board_t& b = term.board();
    tty_state const& s = term.state();
    line_shift_flags flags = b.line_r2l() ? line_shift_flags::r2l : line_shift_flags::none;

    curpos_t x1;
    if (s.dcsm()) {
      // DCSM(DATA)
      flags |= line_shift_flags::dcsm;
      x1 = b.cur.x < b.m_width ? b.cur.x : s.get_mode(mode_xenl_ech) ? b.m_width - 1 : b.m_width;
    } else {
      // DCSM(PRESENTATION)
      x1 = b.to_presentation_position(b.cur.y, b.cur.x);
      if (x1 >= b.m_width) x1 = s.get_mode(mode_xenl_ech) ? b.m_width - 1 : b.m_width;
    }

    line_t& line = b.line();
    curpos_t const slh = term.implicit_slh(line);
    curpos_t const sll = term.implicit_sll(line);

    if (!s.get_mode(mode_hem))
      line.shift_cells(x1, sll + 1, shift, flags, b.m_width, b.cur.fill_attr());
    else
      line.shift_cells(slh, x1 + 1, -shift, flags, b.m_width, b.cur.fill_attr());

    // カーソル位置
    if (s.dcsm()) {
      if (s.get_mode(mode_home_il)) x1 = term.implicit_slh(line);
    } else {
      x1 = s.get_mode(mode_home_il) ? term.implicit_slh(line) : b.to_data_position(b.cur.y, x1);
    }
    b.cur.x = x1;
  }

  bool do_ich(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }

    do_ich_impl(term, (curpos_t) param);
    return true;
  }

  bool do_dch(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }

    do_ich_impl(term, -(curpos_t) param);
    return true;
  }

  //---------------------------------------------------------------------------
  // EL, IL, DL

  static void do_el(board_t& b, tty_state& s, line_t& line, csi_single_param_t param) {
    if (param != 0 && param != 1) {
      if (s.get_mode(mode_erm) && line.has_protected_cells()) {
        line_shift_flags flags = line_shift_flags::erm;
        if (line.is_r2l(b.m_presentation_direction)) flags |= line_shift_flags::r2l;
        if (s.dcsm()) flags |= line_shift_flags::dcsm;
        line.shift_cells(0, b.m_width, b.m_width, flags, b.m_width, b.cur.fill_attr());
      } else
        line.clear_content(b.m_width, b.cur.fill_attr());
      return;
    }

    line_shift_flags flags = 0;
    if (s.get_mode(mode_erm)) flags |= line_shift_flags::erm;
    if (line.is_r2l(b.m_presentation_direction)) flags |= line_shift_flags::r2l;
    if (s.dcsm()) flags |= line_shift_flags::dcsm;

    curpos_t x1 = b.cur.x;
    if (!s.dcsm()) x1 = b.to_presentation_position(b.cur.y, x1);
    if (x1 >= b.m_width) x1 = s.get_mode(mode_xenl_ech) ? b.m_width - 1 : b.m_width;

    if (param == 0)
      line.shift_cells(x1, b.m_width, b.m_width - x1, flags, b.m_width, b.cur.fill_attr());
    else
      line.shift_cells(0, x1 + 1, x1 + 1, flags, b.m_width, b.cur.fill_attr());

    if (!s.dcsm()) x1 = b.to_data_position(b.cur.y, x1);
    b.cur.x = x1;
  }

  bool do_el(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    board_t& b = term.board();
    do_el(b, term.state(), b.line(), param);
    return true;
  }

  void do_ed(term_t& term, csi_single_param_t param) {
    tty_state& s = term.state();
    board_t& b = term.board();
    curpos_t y1 = 0, y2 = 0;
    if (param != 0 && param != 1) {
      y1 = 0;
      y2 = b.m_height;
    } else {
      do_el(b, s, b.line(), param);
      if (param == 0) {
        y1 = b.cur.y + 1;
        y2 = b.m_height;
      } else {
        y1 = 0;
        y2 = b.cur.y;
      }
    }

    if (s.get_mode(mode_erm)) {
      for (curpos_t y = y1; y < y2; y++)
        do_el(b, s, b.m_lines[y], 2);
    } else {
      for (curpos_t y = y1; y < y2; y++)
        b.m_lines[y].clear_content(b.m_width, b.cur.fill_attr());
    }
  }
  bool do_ed(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    do_ed(term, param);
    return true;
  }

  static void do_il_impl(term_t& term, curpos_t shift) {
    board_t& b = term.board();
    tty_state& s = term.state();

    // カーソル表示位置
    curpos_t p = 0;
    if (!s.get_mode(mode_home_il) && !s.dcsm())
      p = b.to_presentation_position(b.cur.y, b.cur.x);

    // 挿入
    curpos_t const beg = term.tmargin();
    curpos_t const end = term.bmargin();
    curpos_t const lmargin = term.lmargin();
    curpos_t const rmargin = term.rmargin();
    if (beg <= b.cur.y && b.cur.y < end) {
      if (!s.get_mode(mode_vem)) {
        do_vertical_scroll(term, shift, b.cur.y, end, lmargin, rmargin, s.dcsm());
      } else {
        do_vertical_scroll(term, -shift, beg, b.cur.y + 1, lmargin, rmargin, s.dcsm());
      }
    }

    // カーソル位置設定
    if (s.get_mode(mode_home_il)) {
      term.initialize_line(b.line());
      b.cur.x = term.implicit_slh(b.line());
    } else if (!s.dcsm())
      b.cur.x = b.to_data_position(b.cur.y, p);
  }

  bool do_il(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }
    do_il_impl(term, (curpos_t) param);
    return true;
  }

  bool do_dl(term_t& term, csi_parameters& params) {
    tty_state& s = term.state();
    csi_single_param_t param;
    params.read_param(param, 1);
    if (param == 0) {
      if (s.get_mode(mode_zdm))
        param = 1;
      else
        return true;
    }
    do_il_impl(term, -(curpos_t) param);
    return true;
  }

  //---------------------------------------------------------------------------
  // device attributes

  void do_s7c1t(term_t& term) {
    term.state().set_mode(mode_s7c1t, true);
  }
  void do_s8c1t(term_t& term) {
    term.state().set_mode(mode_s7c1t, false);
  }
  bool do_decscl(term_t& term, csi_parameters& params) {
    csi_single_param_t param, s7c1t;
    params.read_param(param, 0);
    params.read_param(s7c1t, 0);
    if (param == 0) param = 65;

    term.state().m_decscl = param;
    term.state().set_mode(mode_s7c1t, param == 1);
    return true;
  }
  bool do_da1(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param != 0) return false;

    term.response_put(ascii_csi);
    term.response_put(ascii_question);
    term.response_number(64);
    term.response_put(ascii_semicolon);
    term.response_number(21);
    term.response_put(ascii_c);
    term.respond();
    return true;
  }
  bool do_da2(term_t& term, csi_parameters& params) {
    csi_single_param_t param;
    params.read_param(param, 0);
    if (param != 0) return false;

    term.response_put(ascii_csi);
    term.response_put(ascii_greater);
    term.response_number(67);
    term.response_put(ascii_semicolon);
    term.response_number(0);
    term.response_put(ascii_c);
    term.respond();
    return true;
  }
  bool do_decrqss(term_t& term, char32_t const* param, std::size_t len) {
    if (len == 2) {
      auto _start = [&term] {
        term.response_put(ascii_dcs);
        term.response_put(ascii_0);
        term.response_put(ascii_dollar);
        term.response_put(ascii_r);
      };
      auto _end = [&term] {
        term.response_put(ascii_st);
      };

      if (param[0] == ascii_double_quote) {
        switch (param[1]) {
        case ascii_p:
          // DECSCL
          _start();
          term.response_number(term.state().m_decscl);
          term.response_put(term.state().m_decscl);
          if (term.state().get_mode(mode_s7c1t)) {
            term.response_put(ascii_semicolon);
            term.response_put(ascii_1);
          }
          term.response_put(ascii_p);
          _end();
          term.respond();
          return true;
        case ascii_q:
          // DECSCA
          _start();
          if (term.board().cur.attribute.is_decsca_protected())
            term.response_put(ascii_1);
          term.response_put(ascii_double_quote);
          term.response_put(ascii_q);
          _end();
          term.respond();
          return true;
        }
      }
    }

    return false;
  }

  //---------------------------------------------------------------------------
  // dispatch

  constexpr std::uint32_t compose_bytes(byte major, byte minor) {
    return minor << 8 | major;
  }

  struct control_function_dictionary {
    control_function_t* data1[63];
    std::unordered_map<std::uint16_t, control_function_t*> data2;

    void register_cfunc(control_function_t* fp, byte F) {
      int const index = (int) F - ascii_at;
      mwg_assert(0 <= index && index < (int) size(data1), "final byte out of range");
      mwg_assert(data1[index] == nullptr, "another function is already registered");
      data1[index] = fp;
    }

    void register_cfunc(control_function_t* fp, byte I, byte F) {
      data2[compose_bytes(I, F)] = fp;
    }

    control_function_dictionary() {
      std::fill(std::begin(data1), std::end(data1), nullptr);

      register_cfunc(&do_simd, ascii_circumflex);

      register_cfunc(&do_ich , ascii_at);
      register_cfunc(&do_cuu , ascii_A);
      register_cfunc(&do_cud , ascii_B);
      register_cfunc(&do_cuf , ascii_C);
      register_cfunc(&do_cub , ascii_D);
      register_cfunc(&do_cnl , ascii_E);
      register_cfunc(&do_cpl , ascii_F);
      register_cfunc(&do_cha , ascii_G);
      register_cfunc(&do_cup , ascii_H);
      register_cfunc(&do_ed  , ascii_J);
      register_cfunc(&do_el  , ascii_K);
      register_cfunc(&do_il  , ascii_L);
      register_cfunc(&do_dl  , ascii_M);
      register_cfunc(&do_dch , ascii_P);
      register_cfunc(&do_su  , ascii_S);
      register_cfunc(&do_sd  , ascii_T);
      register_cfunc(&do_ech , ascii_X);
      register_cfunc(&do_srs , ascii_left_bracket);
      register_cfunc(&do_sds , ascii_right_bracket);

      register_cfunc(&do_hpa , ascii_back_quote);
      register_cfunc(&do_hpr , ascii_a);
      register_cfunc(&do_da1 , ascii_c);
      register_cfunc(&do_vpa , ascii_d);
      register_cfunc(&do_vpr , ascii_e);
      register_cfunc(&do_hvp , ascii_f);
      register_cfunc(&do_sm  , ascii_h);
      register_cfunc(&do_hpb , ascii_j);
      register_cfunc(&do_vpb , ascii_k);
      register_cfunc(&do_rm  , ascii_l);
      register_cfunc(&do_sgr , ascii_m);

      register_cfunc(&do_sl  , ascii_sp, ascii_at);
      register_cfunc(&do_sr  , ascii_sp, ascii_A);
      register_cfunc(&do_spd , ascii_sp, ascii_S);
      register_cfunc(&do_slh , ascii_sp, ascii_U);
      register_cfunc(&do_sll , ascii_sp, ascii_V);
      register_cfunc(&do_sco , ascii_sp, ascii_e);
      register_cfunc(&do_sph , ascii_sp, ascii_i);
      register_cfunc(&do_spl , ascii_sp, ascii_j);
      register_cfunc(&do_scp , ascii_sp, ascii_k);

      register_cfunc(&do_decstbm         , ascii_r);
      register_cfunc(&do_decslrm_or_scosc, ascii_s);
      register_cfunc(&do_scorc   , ascii_u);
      register_cfunc(&do_decscusr, ascii_sp, ascii_q);
      register_cfunc(&do_decscl  , ascii_double_quote, ascii_p);
      register_cfunc(&do_decsca  , ascii_double_quote, ascii_q);
      register_cfunc(&do_decscpp , ascii_dollar, ascii_vertical_bar);
    }

    control_function_t* get(byte F) const {
      int const index = (int) F - ascii_at;
      return 0 <= index && index < (int) size(data1) ? data1[index] : nullptr;
    }

    control_function_t* get(byte I, byte F) const {
      typedef std::unordered_map<std::uint16_t, control_function_t*>::const_iterator const_iterator;
      const_iterator const it = data2.find(compose_bytes(I, F));
      return it != data2.end() ? it->second : nullptr;
    }
  };

  static control_function_dictionary cfunc_dict;

  static bool process_private_control_sequence(term_t& term, sequence const& seq) {
    char32_t const* param = seq.parameter();
    std::size_t const len = seq.parameter_size();
    if (len > 0) {
      if (param[0] == '?') {
        // CSI ? ... Ft の形式
        csi_parameters params(param + 1, len - 1);
        if (!params) return false;

        switch (seq.final()) {
        case 'h': return do_decset(term, params);
        case 'l': return do_decrst(term, params);
        }
      } else if (param[0] == '>') {
        csi_parameters params(param + 1, len - 1);
        if (!params) return false;
        switch (seq.final()) {
        case 'c': return do_da2(term, params);
        }
      }
    }

    return false;
  }

  void term_t::process_control_sequence(sequence const& seq) {
    if (seq.is_private_csi()) {
      if (process_private_control_sequence(*this, seq)) return;
      print_unrecognized_sequence(seq);
      return;
    }

    csi_parameters params(seq);
    if (!params) {
      print_unrecognized_sequence(seq);
      return;
    }

    bool result = false;
    std::int32_t const intermediate_size = seq.intermediate_size();
    if (intermediate_size == 0) {
      if (seq.final() == ascii_m)
        result = do_sgr(*this, params);
      else if (control_function_t* const f = cfunc_dict.get(seq.final()))
        result = f(*this, params);
    } else if (intermediate_size == 1) {
      mwg_assert(seq.intermediate()[0] <= 0xFF);
      if (control_function_t* const f = cfunc_dict.get((byte) seq.intermediate()[0], seq.final()))
        result = f(*this, params);
    }

    if (!result)
      print_unrecognized_sequence(seq);
  }

}
}