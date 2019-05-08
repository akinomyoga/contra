// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_TERM_HPP
#define CONTRA_ANSI_TERM_HPP
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstdarg>
#include <climits>
#include <iterator>
#include <algorithm>
#include <vector>
#include <sstream>
#include "../sequence.hpp"
#include "line.hpp"
#include "../enc.c2w.hpp"
#include "../enc.utf8.hpp"

namespace contra {
namespace ansi {

  typedef std::uint32_t key_t;

  enum key_flags {
    _character_mask = 0x001FFFFF,
    _unicode_max    = 0x0010FFFF,
    _key_base       = 0x00110000,

    _modifier_mask  = 0x3F000000,
    _modifier_shft  = 24,

    key_f1        = _key_base | 1,
    key_f2        = _key_base | 2,
    key_f3        = _key_base | 3,
    key_f4        = _key_base | 4,
    key_f5        = _key_base | 5,
    key_f6        = _key_base | 6,
    key_f7        = _key_base | 7,
    key_f8        = _key_base | 8,
    key_f9        = _key_base | 9,
    key_f10       = _key_base | 10,
    key_f11       = _key_base | 11,
    key_f12       = _key_base | 12,
    key_f13       = _key_base | 13,
    key_f14       = _key_base | 14,
    key_f15       = _key_base | 15,
    key_f16       = _key_base | 16,
    key_f17       = _key_base | 17,
    key_f18       = _key_base | 18,
    key_f19       = _key_base | 19,
    key_f20       = _key_base | 20,
    key_f21       = _key_base | 21,
    key_f22       = _key_base | 22,
    key_f23       = _key_base | 23,
    key_f24       = _key_base | 24,
    key_insert    = _key_base | 31,
    key_delete    = _key_base | 32,
    key_home      = _key_base | 33,
    key_end       = _key_base | 34,
    key_prior     = _key_base | 35,
    key_next      = _key_base | 36,
    key_begin     = _key_base | 37,
    key_left      = _key_base | 38,
    key_right     = _key_base | 39,
    key_up        = _key_base | 40,
    key_down      = _key_base | 41,
    key_kp0       = _key_base | 42,
    key_kp1       = _key_base | 43,
    key_kp2       = _key_base | 44,
    key_kp3       = _key_base | 45,
    key_kp4       = _key_base | 46,
    key_kp5       = _key_base | 47,
    key_kp6       = _key_base | 48,
    key_kp7       = _key_base | 49,
    key_kp8       = _key_base | 50,
    key_kp9       = _key_base | 51,
    key_kpdec     = _key_base | 52,
    key_kpsep     = _key_base | 53,
    key_kpmul     = _key_base | 54,
    key_kpadd     = _key_base | 55,
    key_kpsub     = _key_base | 56,
    key_kpdiv     = _key_base | 57,

    modifier_shift       = 0x01000000,
    modifier_meta        = 0x02000000,
    modifier_control     = 0x04000000,
    modifier_super       = 0x08000000,
    modifier_hyper       = 0x10000000,
    modifier_alter       = 0x20000000,

    // focus in/out
    key_focus     = _key_base | 58,
    key_blur      = _key_base | 59,

    // mouse events
    _key_mouse_button_mask = 0x0700,
    _key_mouse1     = 0x0100,
    _key_mouse2     = 0x0200,
    _key_mouse3     = 0x0300,
    _key_mouse4     = 0x0400,
    _key_mouse5     = 0x0500,
    _key_wheel_down = 0x0600,
    _key_wheel_up   = 0x0700,

    _key_mouse_event_mask = 0x1800,
    _key_mouse_event_down = 0x0000,
    _key_mouse_event_up   = 0x0800,
    _key_mouse_event_move = 0x1000,

    key_mouse1_down = _key_base | _key_mouse1 | _key_mouse_event_down,
    key_mouse2_down = _key_base | _key_mouse2 | _key_mouse_event_down,
    key_mouse3_down = _key_base | _key_mouse3 | _key_mouse_event_down,
    key_mouse4_down = _key_base | _key_mouse4 | _key_mouse_event_down,
    key_mouse5_down = _key_base | _key_mouse5 | _key_mouse_event_down,
    key_mouse1_up   = _key_base | _key_mouse1 | _key_mouse_event_up,
    key_mouse2_up   = _key_base | _key_mouse2 | _key_mouse_event_up,
    key_mouse3_up   = _key_base | _key_mouse3 | _key_mouse_event_up,
    key_mouse4_up   = _key_base | _key_mouse4 | _key_mouse_event_up,
    key_mouse5_up   = _key_base | _key_mouse5 | _key_mouse_event_up,
    key_mouse1_drag = _key_base | _key_mouse1 | _key_mouse_event_move,
    key_mouse2_drag = _key_base | _key_mouse2 | _key_mouse_event_move,
    key_mouse3_drag = _key_base | _key_mouse3 | _key_mouse_event_move,
    key_mouse4_drag = _key_base | _key_mouse4 | _key_mouse_event_move,
    key_mouse5_drag = _key_base | _key_mouse5 | _key_mouse_event_move,
    key_mouse_move  = _key_base | _key_mouse_event_move,
    key_wheel_down  = _key_base | _key_wheel_down,
    key_wheel_up    = _key_base | _key_wheel_up,
  };

  void print_key(key_t key, std::FILE* file);

  typedef std::uint32_t mode_t;

  enum mode_spec {
    ansi_mode   = 0, // CSI Ps h
    dec_mode    = 1, // CSI ? Ps h
    contra_mode = 2, // private mode

    accessor_flag = 0x10000,

#include "../../out/gen/term.mode_def.hpp"
  };

  struct tstate_t;
  class term_t;
  typedef std::uint32_t csi_single_param_t;

  void do_insert_graph(term_t& term, char32_t u);
  void do_insert_graphs(term_t& term, char32_t const* beg, char32_t const* end);

  enum mouse_mode_flags {
    mouse_report_mask   = 0xFF,
    mouse_report_down   = 0x01,
    mouse_report_up     = 0x02,
    mouse_report_select = 0x04,
    mouse_report_drag   = 0x08,
    mouse_report_move   = 0x10,

    mouse_report_xtMouseX10    = mouse_report_down,
    mouse_report_xtMouseVt200  = mouse_report_down | mouse_report_up,
    mouse_report_xtMouseHilite = mouse_report_down | mouse_report_select,
    mouse_report_xtMouseButton = mouse_report_xtMouseVt200 | mouse_report_drag,
    mouse_report_xtMouseAll    = mouse_report_xtMouseVt200 | mouse_report_drag | mouse_report_move,

    mouse_sequence_mask  = 0xFF00,
    mouse_sequence_byte  = 0x0100,
    mouse_sequence_utf8  = 0x0200,
    mouse_sequence_sgr   = 0x0400,
    mouse_sequence_urxvt = 0x0800,
  };

  struct tstate_t {
    term_t* m_term;

    curpos_t page_home  {-1};
    curpos_t page_limit {-1};
    curpos_t line_home  {-1};
    curpos_t line_limit {-1};

    line_attr_t lflags {0};

    color_t m_rgba256[256];

    // OSC(0), scrTDS
    std::u32string title; // xterm title
    std::u32string screen_title; // GNU screen title

    // DECSC, SCOSC
    cursor_t m_decsc_cur;
    bool m_decsc_decawm;
    bool m_decsc_decom;
    curpos_t m_scosc_x;
    curpos_t m_scosc_y;
    bool m_scosc_xenl;

    // Alternate Screen Buffer
    board_t altscreen;

    // DECSTBM, DECSLRM
    curpos_t dec_tmargin = -1;
    curpos_t dec_bmargin = -1;
    curpos_t dec_lmargin = -1;
    curpos_t dec_rmargin = -1;

    // conformance level (61-65)
    int m_decscl = 65;

    // 画面サイズ
    curpos_t cfg_decscpp_enabled = true;
    curpos_t cfg_decscpp_min = 20;
    curpos_t cfg_decscpp_max = 400;

    // 既定の前景色・背景色
    byte m_default_fg_space = 0;
    byte m_default_bg_space = 0;
    color_t m_default_fg_color = 0;
    color_t m_default_bg_color = 0;

    std::uint32_t mouse_mode = 0;

    tstate_t(term_t* term): m_term(term) {
      this->clear();
    }

    void clear() {
      this->initialize_mode();
      this->initialize_palette();

      this->title = U"";
      this->screen_title = U"";
      this->m_decsc_cur.set_x(-1);
      this->m_scosc_x = -1;

      this->page_home = -1;
      this->page_limit = -1;
      this->line_home = -1;
      this->line_limit = -1;
      this->clear_margin();

      this->m_decscl = 65;
    }
    void clear_margin() {
      this->dec_tmargin = -1;
      this->dec_bmargin = -1;
      this->dec_lmargin = -1;
      this->dec_rmargin = -1;
    }
    void initialize_palette() {
      color_t A = 0xFF000000;

      // rosaterm palette (rgba format)
      int index = 0;
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x00, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x00, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x80, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x80, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x00, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x00, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x80, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0xC0, 0xC0, 0xC0);
      m_rgba256[index++] = contra::dict::rgba(0x80, 0x80, 0x80);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0x00, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x32, 0xCD, 0x32);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0xD7, 0x00);
      m_rgba256[index++] = contra::dict::rgba(0x00, 0x00, 0xFF);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0x00, 0xFF);
      m_rgba256[index++] = contra::dict::rgba(0x40, 0xE0, 0xD0);
      m_rgba256[index++] = contra::dict::rgba(0xFF, 0xFF, 0xFF);

      // 6x6x6 cube
      //   xterm は i == 0 ? 0 : i * 40 + 55 という式を使っている。
      //   RLogin は恐らく i * 51 という式を使った時と同じに見える。
      auto _intensity = [] (int i) { return i == 0 ? 0 : i * 40 + 55; };
      //auto _intensity = [] (int i) { return i * 51; };
      for (int r = 0; r < 6; r++) {
        color_t const R = A | _intensity(r);
        for (int g = 0; g < 6; g++) {
          color_t const RG = R | _intensity(g) << 8;
          for (int b = 0; b < 6; b++) {
            color_t const RGB = RG | _intensity(b) << 16;
            m_rgba256[index++] = RGB;
          }
        }
      }

      // 24 grayscale
      for (int k = 0; k < 24; k++) {
        color_t const K = k * 10 + 8;
        m_rgba256[index++] = A | K << 16 | K << 8 | K;
      }
    }

    sequence_decoder_config m_sequence_decoder_config;

    bool vt_affected_by_lnm    { true  };
    bool vt_appending_newline  { true  };
    bool vt_using_line_tabstop { false }; // ToDo: not yet supported

    bool ff_affected_by_lnm { true  };
    bool ff_clearing_screen { false };
    bool ff_using_page_home { false };

    /// @var cursor_shape
    ///   0 の時ブロックカーソル、1 の時下線、-1 の時縦線。
    ///   2 以上の時カーソルの下線の高さを百分率で表現。
    int m_cursor_shape;

  private:
    std::uint32_t m_mode_flags[2];

    void initialize_mode();

  private:
    int rqm_mode_with_accessor(mode_t modeSpec) const;
  public:
    bool get_mode(mode_t modeSpec) const {
      std::uint32_t const index = modeSpec;
      if (!(index & accessor_flag)) {
        std::uint32_t const bit = 1 << (index & 0x1F);
        mwg_assert(index < CHAR_BIT * sizeof m_mode_flags, "invalid modeSpec");;

        std::uint32_t const& flags = m_mode_flags[index >> 5];
        return (flags & bit) != 0;
      } else {
        return rqm_mode_with_accessor(modeSpec) & 1;
      }
    }
    int rqm_mode(mode_t modeSpec) const {
      std::uint32_t const index = modeSpec;
      if (!(index & accessor_flag)) {
        std::uint32_t const bit = 1 << (index & 0x1F);
        mwg_assert(index < CHAR_BIT * sizeof m_mode_flags, "invalid modeSpec");;

        std::uint32_t const& flags = m_mode_flags[index >> 5];
        return (flags & bit) != 0 ? 1 : 2;
      } else {
        return rqm_mode_with_accessor(modeSpec);
      }
    }

  private:
    void set_mode_with_accessor(mode_t modeSpec, bool value);
  public:
    void set_mode(mode_t modeSpec, bool value = true) {
      std::uint32_t const index = modeSpec;
      if (!(index & accessor_flag)) {
        std::uint32_t const bit = 1 << (index & 0x1F);
        mwg_assert(index < CHAR_BIT * sizeof m_mode_flags, "invalid modeSpec");;

        std::uint32_t& flags = m_mode_flags[index >> 5];
        if (value)
          flags |= bit;
        else
          flags &= ~bit;
      } else {
        set_mode_with_accessor(modeSpec, value);
      }
    }

    bool dcsm() const { return get_mode(mode_bdsm) || get_mode(mode_dcsm); }

    bool is_cursor_visible() const { return get_mode(mode_dectcem); }
    bool is_cursor_blinking() const {
      bool const st = get_mode(mode_attCursorBlink);
      bool const mn = get_mode(resource_cursorBlink);
      return get_mode(resource_cursorBlinkXOR) ? st != mn : st || mn;
    }
    int get_cursor_shape() const { return m_cursor_shape; }

  public:
    void do_bel() {
      //std::fputc('\a', stdout);
    }

  public:
    std::size_t c2w(char32_t u) const {
      // ToDo: 文字幅設定
      return contra::encoding::c2w(u, contra::encoding::c2w_width_emacs);
    }
  };

  class term_t: public contra::idevice {
  private:
    board_t m_board;
    tstate_t m_state {this};

    contra::idevice* m_send_target = nullptr;

    void report_error(const char* message) {
      std::fprintf(stderr, "%s", message);
    }
  public:
    void initialize_line(line_t& line) const {
      if (line.lflags() & line_attr_t::is_line_used) return;
      line.lflags() = line_attr_t::is_line_used | m_state.lflags;
      line.home() = m_state.line_home;
      line.limit() = m_state.line_limit;
    }

  public:
    // do_insert_graphs() で使う為だけの変数
    std::vector<cell_t> m_buffer;

  public:
    term_t(curpos_t width, curpos_t height): m_board(width, height) {}

  public:
    board_t& board() { return this->m_board; }
    board_t const& board() const { return this->m_board; }
    tstate_t& state() {return this->m_state;}
    tstate_t const& state() const {return this->m_state;}

    curpos_t tmargin() const {
      curpos_t const b = m_state.dec_tmargin;
      return 0 <= b && b < m_board.m_height ? b : 0;
    }
    curpos_t bmargin() const {
      curpos_t const e = m_state.dec_bmargin;
      return 0 < e && e <= m_board.m_height ? e : m_board.m_height;
    }
    curpos_t lmargin() const {
      if (!m_state.get_mode(mode_declrmm)) return 0;
      curpos_t const l = m_state.dec_lmargin;
      return 0 <= l && l < m_board.m_width ? l : 0;
    }
    curpos_t rmargin() const {
      if (!m_state.get_mode(mode_declrmm)) return m_board.m_width;
      curpos_t const r = m_state.dec_rmargin;
      return 0 < r && r <= m_board.m_width ? r : m_board.m_width;
    }

    curpos_t implicit_sph() const {
      curpos_t sph = 0;
      if (m_state.page_home > 0) sph = m_state.page_home;
      if (m_state.dec_tmargin > sph) sph = m_state.dec_tmargin;
      return sph;
    }
    curpos_t implicit_spl() const {
      curpos_t spl = m_board.m_height - 1;
      if (m_state.page_limit >= 0 && m_state.page_limit < spl)
        spl = m_state.page_limit;
      if (m_state.dec_bmargin > 0 && m_state.dec_bmargin - 1 < spl)
        spl = m_state.dec_bmargin - 1;
      return spl;
    }
    curpos_t implicit_slh(line_t const& line) const {
      curpos_t const width = m_board.m_width;
      curpos_t home = line.home();
      home = home < 0 ? 0 : std::min(home, width - 1);
      if (m_state.get_mode(mode_declrmm))
        home = std::max(home, std::min(m_state.dec_lmargin, width - 1));
      return home;
    }
    curpos_t implicit_sll(line_t const& line) const {
      curpos_t const width = m_board.m_width;
      curpos_t limit = line.limit();
      limit = limit < 0 ? width - 1 : std::min(limit, width - 1);
      if (m_state.get_mode(mode_declrmm) && m_state.dec_rmargin > 0)
        limit = std::min(limit, m_state.dec_rmargin - 1);
      return limit;
    }
    curpos_t implicit_sll() const {
      return implicit_sll(m_board.line(m_board.cur.y()));
    }
    curpos_t implicit_slh() const {
      return implicit_slh(m_board.line(m_board.cur.y()));
    }

    attribute_t fill_attr() const {
      if (m_state.get_mode(mode_bce)) {
        attribute_t const& src = m_board.cur.attribute;
        attribute_t ret;
        ret.set_bg(src.bg_space(), src.bg_color());
        return ret;
      } else {
        return {};
      }
    }

  public:
    void insert_marker(std::uint32_t marker) {
      if (m_board.line().cells().size() >= contra::limit::maximal_cells_per_line) {
        this->report_error("insert_marker: Exceeded the maximal number of cells per line.\n");
        return;
      }

      bool const simd = m_state.get_mode(mode_simd);
      curpos_t const dir = simd ? -1 : 1;

      cell_t cell;
      cell.character = marker | character_t::flag_marker;
      cell.attribute = m_board.cur.attribute;
      cell.width = 0;
      initialize_line(m_board.line());
      m_board.line().write_cells(m_board.cur.x(), &cell, 1, 1, dir);
    }

    static constexpr bool is_marker(char32_t u) {
      // Unicode bidi formatting characters
      return (U'\u202A' <= u && u <= U'\u202E') ||
        (U'\u2066' <= u && u <= U'\u2069');
    }

    void insert_char(char32_t u) {
      u &= character_t::unicode_mask;

      if (is_marker(u))
        return insert_marker(u);

      do_insert_graph(*this, u);

#ifndef NDEBUG
      board_t& b = board();
      mwg_assert(b.cur.is_sane(b.m_width),
        "cur: {x=%d, xenl=%d, width=%d} after Insert U+%04X",
        b.cur.x(), b.cur.xenl(), b.m_width, u);
#endif
    }
    void insert_chars(char32_t const* beg, char32_t const* end) {
      while (beg < end) {
        if (!is_marker(*beg)) {
          char32_t const* graph_beg = beg;
          do beg++; while (beg != end && !is_marker(*beg));
          char32_t const* graph_end = beg;
          do_insert_graphs(*this, graph_beg, graph_end);
        } else
          insert_marker(*beg++);

#ifndef NDEBUG
        board_t& b = board();
        mwg_assert(b.cur.is_sane(b.m_width),
          "cur: {x=%d, xenl=%d, width=%d} after InsertLength=#%zu",
          b.cur.x(), b.cur.xenl(), b.m_width, end - beg);
#endif
      }
    }

  public:
    void do_bel() {
      m_state.do_bel();
    }

  private:
    typedef sequence_decoder<term_t> decoder_type;
    decoder_type m_seqdecoder {this, &this->m_state.m_sequence_decoder_config};
    friend decoder_type;

    void print_unrecognized_sequence(sequence const& seq) {
      return; //取り敢えず off

      const char* name = "sequence";
      switch (seq.type()) {
      case ascii_csi:
        name = "control sequence";
        break;
      case ascii_esc:
        name = "escape sequence";
        break;
      case ascii_sos:
        name = "character string";
        break;
      case ascii_pm:
      case ascii_apc:
      case ascii_dcs:
      case ascii_osc:
        name = "command string";
        break;
      }

      std::fprintf(stderr, "unrecognized %s: ", name);
      seq.print(stderr);
      std::fputc('\n', stderr);
    }
    void process_invalid_sequence(sequence const& seq) {
      // ToDo: 何処かにログ出力
      print_unrecognized_sequence(seq);
    }

    void process_escape_sequence(sequence const& seq);
    void process_control_sequence(sequence const& seq);
    void process_command_string(sequence const& seq);

    void process_character_string(sequence const& seq) {
      if (seq.type() == ascii_k) {
        m_state.screen_title = std::u32string(seq.parameter(), (std::size_t) seq.parameter_size());
        return;
      } else
        print_unrecognized_sequence(seq);
    }

    void process_control_character(char32_t uchar);

  public:
    std::uint64_t printt_state = 0;
    std::vector<char32_t> printt_buff;
    void write(const char* data, std::size_t const size) {
      printt_buff.resize(size);
      char32_t* const q0 = &printt_buff[0];
      char32_t* q1 = q0;
      contra::encoding::utf8_decode(data, data + size, q1, q0 + size, printt_state);
      m_seqdecoder.process(q0, q1);
    }
    void printt(const char* text) {
      write(text, std::strlen(text));
    }
    void putc(char32_t uchar) {
      m_seqdecoder.process_char(uchar);
    }

  private:
    virtual void dev_write(char const* data, std::size_t size) override {
      this->write(data, size);
    }

  public:
    void set_input_device(contra::idevice& dev) { this->m_send_target = &dev; }
    contra::idevice& input_device() const { return *this->m_send_target; }

  private:
    std::vector<byte> input_buffer;
  public:
    void input_flush() {
      if (m_send_target)
        m_send_target->dev_write(reinterpret_cast<char const*>(&input_buffer[0]), input_buffer.size());
      input_buffer.clear();
    }
    void input_byte(byte value) {
      input_buffer.push_back(value);
    }
    void input_c1(std::uint32_t code) {
      if (m_state.get_mode(ansi::mode_s7c1t)) {
        input_buffer.push_back(ascii_esc);
        input_buffer.push_back(code - 0x40);
      } else {
        contra::encoding::put_u8(code, input_buffer);
      }
    }
    void input_uchar(char32_t u) {
      contra::encoding::put_u8(u, input_buffer);
    }
    void input_unsigned(std::uint32_t value) {
      if (value >= 10) input_unsigned(value / 10);
      input_buffer.push_back(ascii_0 + value % 10);
    }
    void input_modifier(key_t mod) {
      input_unsigned(1 + ((mod & _modifier_mask) >> _modifier_shft));
    }

  private:
    curpos_t m_mouse_prev_x = -1, m_mouse_prev_y = -1;
  public:
    bool input_key(key_t key);
    bool input_mouse(key_t key, coord_t px, coord_t py, curpos_t x, curpos_t y);
    bool input_paste(std::u32string const& data);
  };

}
}
#endif
