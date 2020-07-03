#include <cstdio>
#include <cstring>
#include "sequence.hpp"

using namespace contra;

template<typename Processor>
class sequence_decoder2 {
  typedef Processor processor_type;

  enum decode_state {
                     decode_default,
                     decode_iso2022,
                     decode_esc,
                     decode_escape_sequence,
                     decode_control_sequence,
                     decode_command_string,
                     decode_character_string,
  };

  processor_type* m_proc;
  sequence_decoder_config* m_config;
  decode_state m_dstate = decode_default;
  decode_state m_dstate_plain = decode_default;

  sequence m_seq;

  // command_string 及び character_string で使う状態
  bool m_hasPendingESC = false;

public:
  sequence_decoder2(Processor* proc, sequence_decoder_config* config): m_proc(proc), m_config(config) {}

private:
  void clear() {
    this->m_seq.clear();
    this->m_dstate = m_dstate_plain;
  }

  void process_invalid_sequence() {
    m_proc->process_invalid_sequence(this->m_seq);
    clear();
  }

private:
  // 以下の charset_t のメンバの値は、全て charset_t に加えて、
  // その文字集合が 94 であるか 96 であるかを識別する為のフラグを付加した物である。
  static constexpr charset_t _iso2022_96_iso8859_1 = iso2022_96_iso8859_1 | iso2022_charset_is96;
  charset_t m_iso2022_Gn[4] = { iso2022_94_iso646_usa, _iso2022_96_iso8859_1, 0, 0, };
  charset_t m_iso2022_GL = iso2022_94_iso646_usa;
  charset_t m_iso2022_GR = _iso2022_96_iso8859_1;
  int m_iso2022_GL_lock = 0;
  int m_iso2022_GR_lock = 1;
  charset_t m_iso2022_SS = iso2022_unspecified;

  // 複数バイト対応 (実質2Bにしか対応しない)
  int m_iso2022_mb_count = 0;
  charset_t m_iso2022_mb_charset = iso2022_unspecified;
  std::uint32_t m_iso2022_mb_buff = 0;
  bool m_iso2022_mb_right = false;

  void iso2022_update() {
    if (m_iso2022_GL == iso2022_94_iso646_usa &&
      m_iso2022_GR == _iso2022_96_iso8859_1 &&
      m_iso2022_SS == iso2022_unspecified
    ) {
      m_dstate_plain = decode_default;
    } else {
      m_dstate_plain = decode_iso2022;
    }
  }
  void iso2022_LSn(int n) {
    m_iso2022_GL_lock = n;
    m_iso2022_GL = m_iso2022_Gn[n];
    iso2022_update();
  }
  void iso2022_LSnR(int n) {
    m_iso2022_GR_lock = n;
    m_iso2022_GR = m_iso2022_Gn[n];
    iso2022_update();
  }
  void iso2022_GnD(int n, int charset) {
    m_iso2022_Gn[n] = charset;
    if (m_iso2022_GL_lock == n)
      m_iso2022_GL = charset;
    if (m_iso2022_GR_lock == n)
      m_iso2022_GR = charset;
    iso2022_update();
  }

  bool iso2022_escape_sequence() {
    if (this->m_seq.content_size() == 0) {
      switch (this->m_seq.final()) {
      case ascii_n: // LS2
        iso2022_LSn(2);
        return true;
      case ascii_o: // LS3
        iso2022_LSn(3);
        return true;
      case ascii_vertical_bar: // LS3R
        iso2022_LSnR(3);
        return true;
      case ascii_right_brace: // LS2R
        iso2022_LSnR(2);
        return true;
      case ascii_tilde: // LS1R
        iso2022_LSnR(1);
        return true;
      }
    } else {
      int seq_skip = 1;
      int n;
      iso2022_charset_size m;
      switch (this->m_seq.content()[0]) {
      case ascii_left_paren: // GZD4
        n = 0; m = iso2022_size_sb94;
        goto process_iso2022_GnDm;
      case ascii_right_paren: // G1D4
        n = 1; m = iso2022_size_sb94;
        goto process_iso2022_GnDm;
      case ascii_asterisk: // G2D4
        n = 2; m = iso2022_size_sb94;
        goto process_iso2022_GnDm;
      case ascii_plus: // G3D4
        n = 3; m = iso2022_size_sb94;
        goto process_iso2022_GnDm;
      case ascii_minus: // G1D6
        n = 1; m = iso2022_size_sb96;
        goto process_iso2022_GnDm;
      case ascii_dot: // G2D6
        n = 2; m = iso2022_size_sb96;
        goto process_iso2022_GnDm;
      case ascii_slash: // G3D6
        n = 3; m = iso2022_size_sb96;
        goto process_iso2022_GnDm;
      case ascii_dollar:
        if (this->m_seq.content_size() == 1) {
          // ESC $ @, ESC $ A, ESC $ B は特別扱い
          if (ascii_at <= m_seq.final() && m_seq.final() <= ascii_B) {
            n = 0; m = iso2022_size_mb94;
            goto process_iso2022_GnDm;
          }
        } else {
          seq_skip = 2;
          switch (m_seq.content()[1]) {
          case ascii_left_paren: // GZDM4
            n = 0; m = iso2022_size_mb94;
            goto process_iso2022_GnDm;
          case ascii_right_paren: // G1DM4
            n = 1; m = iso2022_size_mb94;
            goto process_iso2022_GnDm;
          case ascii_asterisk: // G2DM4
            n = 2; m = iso2022_size_mb94;
            goto process_iso2022_GnDm;
          case ascii_plus: // G3DM4
            n = 3; m = iso2022_size_mb94;
            goto process_iso2022_GnDm;
          case ascii_minus: // G1DM6
            n = 1; m = iso2022_size_mb96;
            goto process_iso2022_GnDm;
          case ascii_dot: // G2DM6
            n = 2; m = iso2022_size_mb96;
            goto process_iso2022_GnDm;
          case ascii_slash: // G3DM6
            n = 3; m = iso2022_size_mb96;
            goto process_iso2022_GnDm;
          case ascii_sp: // [T.61/E.3.1.2] DRCS の時は ( を省略しても良い?
            seq_skip = 1;
            n = 0; m = iso2022_size_mb94;
            goto process_iso2022_GnDm;
          }
        }
        break;
      process_iso2022_GnDm:
        {
          // ESC(B は terminfo に登録されて頻繁に呼び出される。
          // 初期化を遅延する為に特別に処理する事にする。
          if (seq_skip == m_seq.content_size()) {
            if (m == iso2022_size_sb94 && m_seq.final() == ascii_B) {
              iso2022_GnD(n, iso2022_94_iso646_usa);
              return true;
            } else if (m == iso2022_size_sb96 && m_seq.final() == ascii_A) {
              iso2022_GnD(n, _iso2022_96_iso8859_1);
              return true;
            }
          }

          auto& iso2022 = iso2022_charset_registry::instance();
          char32_t const* intermediate = m_seq.content() + seq_skip;
          std::size_t const intermediate_size = m_seq.content_size() - seq_skip;
          charset_t const charset = iso2022.resolve_designator(m, intermediate, intermediate_size, m_seq.final());
          if (charset != iso2022_unspecified) {
            iso2022_GnD(n, charset);
            return true;
          } else {
            // unrecognized charset
            // ■ログ機能を実装したら詳細な情報をそちらに出力する
          }
        }
        break;
      }
    }
    return false;
  }

  void iso2022_send_char(charset_t charset, std::uint32_t char_index) {
    charset_t const cssize = charset & charflag_iso2022_db ? 96 * 96 : 96;
    std::uint32_t const charset_index = charset & charflag_iso2022_mask_code;
    std::uint32_t const flags = (charset & charflag_iso2022_mask_flag) | charflag_iso2022;
    m_proc->process_char((charset_index * cssize + char_index) | flags);
  }
  void iso2022_mb_start(charset_t charset, char32_t uchar) {
    m_iso2022_mb_count = 1; // 今は2バイト文字集合のみ対応
    m_iso2022_mb_charset = charset;
    m_iso2022_mb_buff = (uchar & 0x7F) - 0x20;
    m_iso2022_mb_right = uchar >= 0x80;
  }
  bool iso2022_mb_add(char32_t uchar) {
    // GL/GR が混ざった multibyte は駄目
    if (m_iso2022_mb_right != (uchar >= 0x80)) return false;

    // 94/94n文字集合の時 2/0 は駄目。7/15 は無視する。
    if (!(m_iso2022_mb_charset & iso2022_charset_is96)) {
      if (uchar == ascii_sp) return false;
      if (uchar == ascii_del) return true;
    }

    m_iso2022_mb_buff = m_iso2022_mb_buff * 96 + ((uchar & 0x7F) - 0x20);
    if (--m_iso2022_mb_count == 0) {
      // ISO-2022 複数バイト文字確定
      m_iso2022_SS = iso2022_unspecified;
      iso2022_send_char(m_iso2022_mb_charset, m_iso2022_mb_buff);
    }
    return true;
  }

  // GL/GR のコードポイントの文字を処理
  void process_char_iso2022_graphic(char32_t uchar) {
    //assert(uchar < 0x100 && 0x20 <= (uchar & 0x7F));

    // 複数バイト文字集合の読み取り途中
    if (m_iso2022_mb_count) {
      if (iso2022_mb_add(uchar)) return;

      // 復号エラーの時は 0xFFFD を出力して、
      // 状態をリセットして普通に処理する。
      m_proc->process_char(0xFFFD);
      m_iso2022_mb_count = 0;
      m_iso2022_SS = iso2022_unspecified;
    }

    charset_t charset;
    if (m_iso2022_SS != iso2022_unspecified) {
      charset = m_iso2022_SS;

      // 現在の実装では単一シフト領域を GL/GR のどちらにも規定しない。
      // 送信側がどちらを想定していても動く様にする。

      // 94/94n 文字集合を SS した直後の 2/0 及び 7/15 は駄目
      if (!(charset & iso2022_charset_is96) && (uchar == ascii_sp || uchar == ascii_del)) {
        // DEL は無視する (CC-data-element から除いてなかった事にする)
        if (uchar == ascii_del) return;

        m_proc->process_char(0xFFFD);
        m_iso2022_SS = iso2022_unspecified;
        process_char_iso2022_graphic(uchar); // retry
        return;
      }
    } else {
      charset = uchar < 0x80 ? m_iso2022_GL : m_iso2022_GR;
      if (!(charset & iso2022_charset_is96) && (uchar == ascii_sp || uchar == ascii_del)) {
        // DEL は無視する (CC-data-element から除いてなかった事にする)
        if (uchar == ascii_del) return;

        m_proc->process_char(uchar);
        return;
      }
    }

    if (charset & charflag_iso2022_db) {
      iso2022_mb_start(charset, uchar);
    } else {
      m_iso2022_SS = iso2022_unspecified;
      if (charset == iso2022_94_iso646_usa)
        m_proc->process_char(uchar & 0x7F);
      else if (charset == _iso2022_96_iso8859_1)
        m_proc->process_char((uchar & 0x7F) | 0x80);
      else
        iso2022_send_char(charset, (uchar & 0x7F) - 0x20);
    }
  }

private:
  bool is_command_string_terminator(char32_t uchar) const {
    if (uchar == ascii_st)
      return m_config->c1_8bit_representation_enabled;
    else if (uchar == ascii_bel) {
      if (m_config->command_string_terminated_by_bel) return true;
      if (m_seq.type() == ascii_osc) return m_config->osc_sequence_terminated_by_bel;
      return false;
    } else
      return false;
  }

  bool is_character_string_terminator(char32_t uchar) const {
    if (uchar == ascii_st)
      return m_config->c1_8bit_representation_enabled;
    else if (uchar == ascii_bel)
      return m_config->character_string_terminated_by_bel;
    else
      return false;
  }
public:
  void decode_char(char32_t uchar) {
    decode(&uchar, &uchar + 1);
  }

  static constexpr bool is_ctrl(std::uint32_t uchar) {
    std::uint32_t u = uchar & ~(std::uint32_t) 0x80;
    return u < 0x20 || uchar == ascii_del;
  }

  decode_state decode_impl(char32_t const* beg, char32_t const* end) {
#define contra_coroutine(name) case name: name: if (beg == end) return name;
    char32_t uchar;
    switch (m_dstate) {
    decode_plain:
      {
        if (m_dstate_plain == decode_iso2022)
          goto decode_iso2022;
        else
          goto decode_default;
      }

    process_invalid_sequence:
      {
        m_proc->process_invalid_sequence(this->m_seq);
        this->m_seq.clear();
        if (m_hasPendingESC) {
          this->m_hasPendingESC = false;
          goto decode_esc;
        } else
          goto decode_plain;
      }

    process_escape_sequence:
      {
        if (!iso2022_escape_sequence())
          m_proc->process_escape_sequence(this->m_seq);
        this->m_seq.clear();
        goto decode_plain;
      }

    process_control_sequence:
      {
        m_proc->process_control_sequence(this->m_seq);
        this->m_seq.clear();
        goto decode_plain;
      }

    process_command_string:
      {
        m_proc->process_command_string(this->m_seq);
        this->m_seq.clear();
        this->m_hasPendingESC = false;
        goto decode_plain;
      }

    process_character_string:
      {
        m_proc->process_character_string(this->m_seq);
        this->m_seq.clear();
        this->m_hasPendingESC = false;
        goto decode_plain;
      }

    process_c1:
      {
        switch (uchar) {
        case ascii_csi:
          m_seq.set_type((byte) uchar);
          goto decode_control_sequence;
          break;
        case ascii_sos:
          m_seq.set_type((byte) uchar);
          goto decode_character_string;
        case ascii_dcs:
        case ascii_osc:
        case ascii_pm:
        case ascii_apc:
          m_seq.set_type((byte) uchar);
          goto decode_command_string;
        default:
          m_proc->process_control_character(uchar);
          goto decode_plain;
        }
      }

      contra_coroutine(decode_default) {
        // 通常文字の連続はまとめて処理
        if (!is_ctrl(*beg)) {
          char32_t const* const batch_begin = beg;
          do beg++; while (beg != end && !is_ctrl(*beg));
          char32_t const* const batch_end = beg;
          m_proc->process_chars(batch_begin, batch_end);
          if (beg == end) return decode_default;
        }

        uchar = *beg++;
        if (uchar < 0x20) {
          // C0
          if (0xF7FF3FFEu & 1 << uchar) {
            // 速度のため頻出制御文字は先に分岐
            // Note: 0xF7FF3FFEu は sequence_decoder で特別に
            //   処理する必要のある NUL, SO, SI, ESC を除いた物。
            m_proc->process_control_character(uchar);
            goto decode_default;
          } else {
            switch (uchar) {
            case ascii_esc:
              goto decode_esc;
            case ascii_so: // SO/LS1
              iso2022_LSn(1);
              goto decode_plain;
            case ascii_si: // SI/LS0
              iso2022_LSn(0);
              goto decode_plain;
            }
          }
        } else if (0x80 <= uchar && uchar < 0xA0) {
          // C1
          if (m_config->c1_8bit_representation_enabled)
            goto process_c1;
        }
        goto decode_default;
      }

      contra_coroutine(decode_iso2022) {
        uchar = *beg++;
        if (uchar < 0x100 && 0x20 <= (uchar & 0x7F)) {
          process_char_iso2022_graphic(*beg);
          goto decode_iso2022;
        }

        if (m_iso2022_mb_count) {
          m_proc->process_char(0xFFFD);
          m_iso2022_mb_count = 0;
          m_iso2022_SS = iso2022_unspecified;
        } else if (m_iso2022_SS != iso2022_unspecified) {
          m_proc->process_char(0xFFFD);
          m_iso2022_SS = iso2022_unspecified;
        }

        if (uchar < 0x20) {
          // C0
          switch (uchar) {
          case ascii_esc:
            goto decode_esc;
          case ascii_so: // SO/LS1
            iso2022_LSn(1);
            goto decode_plain;
          case ascii_si: // SI/LS0
            iso2022_LSn(0);
            goto decode_plain;
          default:
            m_proc->process_control_character(uchar);
            goto decode_iso2022;
          }
        } else if (0x80 <= uchar && uchar < 0xA0) {
          if (m_config->c1_8bit_representation_enabled)
            goto process_c1;
          goto decode_iso2022;
        } else {
          m_proc->process_char(uchar);
          goto decode_iso2022;
        }
      }

      contra_coroutine(decode_esc) {
        uchar = *beg++;
        if (uchar == ascii_nul) goto decode_esc; // Note: DEL は96文字集合の時には処理する。
        if (0x20 <= uchar && uchar < 0x7E) {
          if (uchar < 0x30) {
            // <I> to start an escape sequence
            m_seq.set_type((byte) ascii_esc);
            m_seq.append((byte) uchar);
            goto decode_escape_sequence;
          } else if (0x40 <= uchar && uchar < 0x60) {
            // <F> to call C1
            uchar = (uchar & 0x1F) | 0x80;
            goto process_c1;
          } else if (uchar == ascii_k && m_config->title_definition_string_enabled) {
            m_seq.set_type((byte) ascii_k);
            goto decode_character_string;
          } else {
            // <F>/<P> to complete an escape sequence
            m_seq.set_type((byte) ascii_esc);
            m_seq.set_final((byte) uchar);
            goto process_escape_sequence;
          }
        } else if (uchar == ascii_nul || uchar == ascii_del) {
          goto decode_esc;
        } else {
          m_seq.set_type((byte) ascii_esc);
          m_seq.set_final((byte) uchar);
          goto process_invalid_sequence;
        }
      }

      contra_coroutine(decode_escape_sequence) {
        char32_t const uchar = *beg++;
        if (uchar == ascii_nul || uchar == ascii_del) goto decode_escape_sequence;

        if (0x30 <= uchar && uchar <= 0x7E) {
          m_seq.set_final((byte) uchar);
          goto process_escape_sequence;
        } else if (0x20 <= uchar && uchar <= 0x2F) {
          m_seq.append(uchar);
          goto decode_escape_sequence;
        } else {
          beg--;
          goto process_invalid_sequence;
        }
      }

      contra_coroutine(decode_control_sequence) {
        do {
          char32_t const uchar = *beg++;
          if (uchar == ascii_nul || uchar == ascii_del) continue;

          // final byte
          if (0x40 <= uchar && uchar <= 0x7E) {
            m_seq.set_final((byte) uchar);
            goto process_control_sequence;
          }

          // param bytes or intermediate bytes
          if (m_seq.has_intermediate()) {
            if (0x20 <= uchar && uchar <= 0x2F) {
              m_seq.append(uchar);
              continue;
            }
          } else {
            if (0x20 <= uchar && uchar <= 0x3F) {
              if (uchar <= 0x2F)
                m_seq.start_intermediate();
              m_seq.append(uchar);
              continue;
            }
          }

          // Note: 事情はよく分からないが vttest は HT, VT, CR をその場で処理する事を要求する。
          //   RLogin はエスケープシーケンス以外の C0 を全てその場で処理している気がする。
          //   SI, SO に関しても文字コード制御の効果を持っている。
          if (uchar < 0x20 && uchar != ascii_esc) {
            switch (uchar) {
            case ascii_so: // SO/LS1
              iso2022_LSn(1);
              break;
            case ascii_si: // SI/LS0
              iso2022_LSn(0);
              break;
            default:
              m_proc->process_control_character(uchar);
              break;
            }
            continue;
          }

          beg--;
          m_seq.set_final(uchar);
          goto process_invalid_sequence;
        } while (beg != end);
        return decode_control_sequence;
      }

      contra_coroutine(decode_command_string) {
        do {
          char32_t const uchar = *beg++;
          if (uchar == ascii_nul || uchar == ascii_del) continue;

          if (m_hasPendingESC) {
            if (uchar == ascii_backslash) {
              goto process_command_string;
            } else {
              beg--;
              goto process_invalid_sequence;
            }
          }

          if ((0x08 <= uchar && uchar <= 0x0D) || (0x20 <= uchar && uchar <= 0x7E)) {
            m_seq.append(uchar);
          } else if (uchar == ascii_esc) {
            m_hasPendingESC = true;
          } else if (is_command_string_terminator(uchar)) {
            goto process_command_string;
          } else {
            beg--;
            goto process_invalid_sequence;
          }
        } while (beg != end);
        return decode_command_string;
      }

      contra_coroutine(decode_character_string) {
        do {
          char32_t const uchar = *beg++;
          if (uchar == ascii_nul || uchar == ascii_del) continue;

          if (m_hasPendingESC) {
            if (uchar == ascii_backslash || is_character_string_terminator(uchar)) {
              // final ST
              if (uchar == ascii_st)
                m_seq.append(ascii_esc);
              goto process_character_string;
            } else if (m_seq.type() == ascii_sos &&
              (uchar == ascii_X || (uchar == ascii_sos && m_config->c1_8bit_representation_enabled))) {
              // invalid SOS
              if (uchar == ascii_sos) {
                m_seq.append(ascii_esc);
                m_hasPendingESC = false;
              }
              beg--;
              goto process_invalid_sequence;
            } else if (uchar == ascii_esc) {
              m_seq.append(ascii_esc);
              continue;
            }
          }

          if (uchar == ascii_esc) {
            m_hasPendingESC = true;
          } else if (is_character_string_terminator(uchar)) {
            goto process_character_string;
          } else if (uchar == ascii_sos && m_config->c1_8bit_representation_enabled) {
            beg--;
            goto process_invalid_sequence;
          } else
            m_seq.append(uchar);
        } while (beg != end);
        return decode_character_string;
      }
    default:
      mwg_check(0, "unexpected m_dstate = %d", (int) m_dstate);
      goto decode_plain;
    }

#undef contra_coroutine
  }

  void decode(char32_t const* beg, char32_t const* end) {
    m_dstate = decode_impl(beg, end);
  }

  void process_end() {
    switch (m_dstate) {
    case decode_escape_sequence:
    case decode_control_sequence:
    case decode_command_string:
    case decode_character_string:
      process_invalid_sequence();
      break;
    default: break;
    }
  }
};

int main() {
  sequence_print_processor processor(stdout);
  sequence_decoder_config config; // ToDo: should be simpler instance
  sequence_decoder2 decoder {&processor, &config};
  auto _write = [&](const char* str) {
    while (*str)
      decoder.decode_char((byte) *str++);
  };
  _write("hello world\n");
  _write("\x1bM");
  _write("\x1b[1;2:3m");
  _write("\x1b]12;hello\a");
  _write("\033Pqhello\x9c");

  return 0;
}
