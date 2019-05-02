// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_TERM_HPP
#define CONTRA_ANSI_TERM_HPP
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstdarg>
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
    mask_unicode   = 0x0001FFFF,
    mask_keycode   = 0x0003FFFF,

    _modifier_mask = 0x3F000000,
    _modifier_shft = 24,

    key_base      = 0x00020000,
    key_f1        = key_base | 1,
    key_f2        = key_base | 2,
    key_f3        = key_base | 3,
    key_f4        = key_base | 4,
    key_f5        = key_base | 5,
    key_f6        = key_base | 6,
    key_f7        = key_base | 7,
    key_f8        = key_base | 8,
    key_f9        = key_base | 9,
    key_f10       = key_base | 10,
    key_f11       = key_base | 11,
    key_f12       = key_base | 12,
    key_f13       = key_base | 13,
    key_f14       = key_base | 14,
    key_f15       = key_base | 15,
    key_f16       = key_base | 16,
    key_f17       = key_base | 17,
    key_f18       = key_base | 18,
    key_f19       = key_base | 19,
    key_f20       = key_base | 20,
    key_f21       = key_base | 21,
    key_f22       = key_base | 22,
    key_f23       = key_base | 23,
    key_f24       = key_base | 24,
    key_insert    = key_base | 31,
    key_delete    = key_base | 32,
    key_home      = key_base | 33,
    key_end       = key_base | 34,
    key_prior     = key_base | 35,
    key_next      = key_base | 36,
    key_begin     = key_base | 37,
    key_left      = key_base | 38,
    key_right     = key_base | 39,
    key_up        = key_base | 40,
    key_down      = key_base | 41,
    key_kp0       = key_base | 42,
    key_kp1       = key_base | 43,
    key_kp2       = key_base | 44,
    key_kp3       = key_base | 45,
    key_kp4       = key_base | 46,
    key_kp5       = key_base | 47,
    key_kp6       = key_base | 48,
    key_kp7       = key_base | 49,
    key_kp8       = key_base | 50,
    key_kp9       = key_base | 51,
    key_kpdec     = key_base | 52,
    key_kpsep     = key_base | 53,
    key_kpmul     = key_base | 54,
    key_kpadd     = key_base | 55,
    key_kpsub     = key_base | 56,
    key_kpdiv     = key_base | 57,

    modifier_shift       = 0x01000000,
    modifier_meta        = 0x02000000,
    modifier_control     = 0x04000000,
    modifier_super       = 0x08000000,
    modifier_hyper       = 0x10000000,
    modifier_alter       = 0x20000000,
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
      for (int r = 0; r < 6; r++) {
        color_t const R = A | r * 51;
        for (int g = 0; g < 6; g++) {
          color_t const RG = R | g * 51 << 8;
          for (int b = 0; b < 6; b++) {
            color_t const RGB = RG | b * 51 << 16;
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
    bool get_mode_with_accessor(mode_t modeSpec) const;
  public:
    bool get_mode(mode_t modeSpec) const {
      std::uint32_t const index = modeSpec;
      if (!(index & accessor_flag)) {
        unsigned const field = index >> 5;
        std::uint32_t const bit = 1 << (index & 0x1F);
        mwg_assert(field < sizeof(m_mode_flags) / sizeof(m_mode_flags[0]), "invalid modeSpec");;

        std::uint32_t const& flags = m_mode_flags[index >> 5];
        return (flags & bit) != 0;
      } else {
        return get_mode_with_accessor(modeSpec);
      }
    }

  private:
    void set_mode_with_accessor(mode_t modeSpec, bool value);
  public:
    void set_mode(mode_t modeSpec, bool value = true) {
      std::uint32_t const index = modeSpec;
      if (!(index & accessor_flag)) {
        unsigned const field = index >> 5;
        std::uint32_t const bit = 1 << (index & 0x1F);
        mwg_assert(field < sizeof(m_mode_flags) / sizeof(m_mode_flags[0]), "invalid modeSpec");;

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
    board_t* m_board;
    tstate_t m_state {this};

    contra::idevice* m_send_target = nullptr;
    std::vector<byte> m_send_buff;

  public:
    void initialize_line(line_t& line) const {
      if (line.lflags() & line_attr_t::is_line_used) return;
      line.lflags() = line_attr_t::is_line_used | m_state.lflags;
      line.home() = m_state.line_home;
      line.limit() = m_state.line_limit;
    }

  public:
    term_t(contra::ansi::board_t& board): m_board(&board) {
      this->m_send_buff.reserve(32);
    }

  public:
    board_t& board() { return *m_board; }
    board_t const& board() const { return *m_board; }
    tstate_t& state() {return this->m_state;}
    tstate_t const& state() const {return this->m_state;}

    curpos_t tmargin() const {
      curpos_t const b = m_state.dec_tmargin;
      return 0 <= b && b < m_board->m_height ? b : 0;
    }
    curpos_t bmargin() const {
      curpos_t const e = m_state.dec_bmargin;
      return 0 < e && e <= m_board->m_height ? e : m_board->m_height;
    }
    curpos_t lmargin() const {
      if (!m_state.get_mode(mode_declrmm)) return 0;
      curpos_t const l = m_state.dec_lmargin;
      return 0 <= l && l < m_board->m_width ? l : 0;
    }
    curpos_t rmargin() const {
      if (!m_state.get_mode(mode_declrmm)) return m_board->m_width;
      curpos_t const r = m_state.dec_rmargin;
      return 0 < r && r <= m_board->m_width ? r : m_board->m_width;
    }

    curpos_t implicit_sph() const {
      curpos_t sph = 0;
      if (m_state.page_home > 0) sph = m_state.page_home;
      if (m_state.dec_tmargin > sph) sph = m_state.dec_tmargin;
      return sph;
    }
    curpos_t implicit_spl() const {
      curpos_t spl = m_board->m_height - 1;
      if (m_state.page_limit >= 0 && m_state.page_limit < spl)
        spl = m_state.page_limit;
      if (m_state.dec_bmargin > 0 && m_state.dec_bmargin - 1 < spl)
        spl = m_state.dec_bmargin - 1;
      return spl;
    }
    curpos_t implicit_slh(line_t const& line) const {
      curpos_t const width = m_board->m_width;
      curpos_t home = line.home();
      home = home < 0 ? 0 : std::min(home, width - 1);
      if (m_state.get_mode(mode_declrmm))
        home = std::max(home, std::min(m_state.dec_lmargin, width - 1));
      return home;
    }
    curpos_t implicit_sll(line_t const& line) const {
      curpos_t const width = m_board->m_width;
      curpos_t limit = line.limit();
      limit = limit < 0 ? width - 1 : std::min(limit, width - 1);
      if (m_state.get_mode(mode_declrmm) && m_state.dec_rmargin > 0)
        limit = std::min(limit, m_state.dec_rmargin - 1);
      return limit;
    }
    curpos_t implicit_sll() const {
      return implicit_sll(m_board->line(m_board->cur.y()));
    }
    curpos_t implicit_slh() const {
      return implicit_slh(m_board->line(m_board->cur.y()));
    }

    attribute_t fill_attr() const {
      if (m_state.get_mode(mode_bce)) {
        attribute_t const& src = m_board->cur.attribute;
        attribute_t ret;
        ret.set_bg(src.bg_space(), src.bg_color());
        return ret;
      } else {
        return {};
      }
    }

  public:
    void insert_marker(std::uint32_t marker) {
      bool const simd = m_state.get_mode(mode_simd);
      curpos_t const dir = simd ? -1 : 1;

      cell_t cell;
      cell.character = marker | character_t::flag_marker;
      cell.attribute = m_board->cur.attribute;
      cell.width = 0;
      initialize_line(m_board->line());
      m_board->line().write_cells(m_board->cur.x(), &cell, 1, 1, dir);
    }

    void insert_char(char32_t u) {
      u &= character_t::unicode_mask;

      // Unicode bidi formatting characters
      if ((U'\u202A' <= u && u <= U'\u202E') ||
        (U'\u2066' <= u && u <= U'\u2069'))
        return insert_marker(u);

      do_insert_graph(*this, u);

      board_t& b = board();
      mwg_assert(b.cur.is_sane(b.m_width),
        "cur: {x=%d, xenl=%d, width=%d} after Insert U+%04X",
        b.cur.x(), b.cur.xenl(), b.m_width, u);
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
      for (char32_t const* q = q0; q < q1; q++)
        m_seqdecoder.process_char(*q);
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
    void set_input_target(contra::idevice& dev) { this->m_send_target = &dev; }

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
    void input_unsigned(std::uint32_t value) {
      if (value >= 10) input_unsigned(value / 10);
      input_buffer.push_back(ascii_0 + value % 10);
    }
    void input_modifier(key_t mod) {
      input_unsigned(1 + ((mod & _modifier_mask) >> _modifier_shft));
    }

  public:
    void input_key(key_t key);
  };

}
}
#endif
