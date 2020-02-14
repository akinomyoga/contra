// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_contradef_hpp
#define contra_contradef_hpp
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
namespace contra {
  typedef std::uint8_t byte;

  template<typename T>
  struct identity { typedef T type; };

  // // std::size (C++17 <iterator>)
  // template<typename T, std::size_t N>
  // constexpr std::size_t size(T const (&)[N]) {return N;}

  template<typename T>
  constexpr T clamp(T value, typename identity<T>::type min, typename identity<T>::type max) {
    return value < min ? min : value > max ? max : value;
  }

  // discarded expression
#define contra_unused(X) static_cast<void>(X)

  template<typename Integer>
  constexpr Integer ceil_div(Integer a, Integer b) { return (a + b - 1) / b; }

  //---------------------------------------------------------------------------
  // Output devices

  struct idevice {
    virtual void dev_write(char const* data, std::size_t size) = 0;
    virtual ~idevice() {}
  };

  class multicast_device: public idevice {
    std::vector<idevice*> m_list;
  public:
    void push(idevice* dev) {this->m_list.push_back(dev);}
    virtual void dev_write(char const* data, std::size_t size) override {
      for (idevice* const dev: m_list)
        dev->dev_write(data, size);
    }
  };

  //---------------------------------------------------------------------------
  // ASCII codes

  enum ascii_codes {

    // control characters

    ascii_nul = 0x00, ascii_dle = 0x10, ascii_pad = 0x80, ascii_dcs  = 0x90,
    ascii_soh = 0x01, ascii_dc1 = 0x11, ascii_hop = 0x81, ascii_pu1  = 0x91,
    ascii_stk = 0x02, ascii_dc2 = 0x12, ascii_bph = 0x82, ascii_pu2  = 0x92,
    ascii_etx = 0x03, ascii_dc3 = 0x13, ascii_nbh = 0x83, ascii_sts  = 0x93,
    ascii_eot = 0x04, ascii_dc4 = 0x14, ascii_ind = 0x84, ascii_cch  = 0x94,
    ascii_enq = 0x05, ascii_nak = 0x15, ascii_nel = 0x85, ascii_mw   = 0x95,
    ascii_ack = 0x06, ascii_syn = 0x16, ascii_ssa = 0x86, ascii_spa  = 0x96,
    ascii_bel = 0x07, ascii_etb = 0x17, ascii_esa = 0x87, ascii_epa  = 0x97,
    ascii_bs  = 0x08, ascii_can = 0x18, ascii_hts = 0x88, ascii_sos  = 0x98,
    ascii_ht  = 0x09, ascii_em  = 0x19, ascii_htj = 0x89, ascii_sgci = 0x99,
    ascii_lf  = 0x0A, ascii_sub = 0x1A, ascii_vts = 0x8A, ascii_sci  = 0x9A,
    ascii_vt  = 0x0B, ascii_esc = 0x1B, ascii_pld = 0x8B, ascii_csi  = 0x9B,
    ascii_ff  = 0x0C, ascii_fs  = 0x1C, ascii_plu = 0x8C, ascii_st   = 0x9C,
    ascii_cr  = 0x0D, ascii_gs  = 0x1D, ascii_ri  = 0x8D, ascii_osc  = 0x9D,
    ascii_so  = 0x0E, ascii_rs  = 0x1E, ascii_ss2 = 0x8E, ascii_pm   = 0x9E,
    ascii_si  = 0x0F, ascii_us  = 0x1F, ascii_ss3 = 0x8F, ascii_apc  = 0x9F,

    ascii_sp   = 0x20,
    ascii_del  = 0x7F,
    ascii_nbsp = 0xA0,

    // alnum

    ascii_0 = 0x30,
    ascii_1 = 0x31,
    ascii_2 = 0x32,
    ascii_3 = 0x33,
    ascii_4 = 0x34,
    ascii_5 = 0x35,
    ascii_6 = 0x36,
    ascii_7 = 0x37,
    ascii_8 = 0x38,
    ascii_9 = 0x39,

    ascii_A = 0x41, ascii_a = 0x61,
    ascii_B = 0x42, ascii_b = 0x62,
    ascii_C = 0x43, ascii_c = 0x63,
    ascii_D = 0x44, ascii_d = 0x64,
    ascii_E = 0x45, ascii_e = 0x65,
    ascii_F = 0x46, ascii_f = 0x66,
    ascii_G = 0x47, ascii_g = 0x67,
    ascii_H = 0x48, ascii_h = 0x68,
    ascii_I = 0x49, ascii_i = 0x69,
    ascii_J = 0x4A, ascii_j = 0x6A,
    ascii_K = 0x4B, ascii_k = 0x6B,
    ascii_L = 0x4C, ascii_l = 0x6C,
    ascii_M = 0x4D, ascii_m = 0x6D,
    ascii_N = 0x4E, ascii_n = 0x6E,
    ascii_O = 0x4F, ascii_o = 0x6F,
    ascii_P = 0x50, ascii_p = 0x70,
    ascii_Q = 0x51, ascii_q = 0x71,
    ascii_R = 0x52, ascii_r = 0x72,
    ascii_S = 0x53, ascii_s = 0x73,
    ascii_T = 0x54, ascii_t = 0x74,
    ascii_U = 0x55, ascii_u = 0x75,
    ascii_V = 0x56, ascii_v = 0x76,
    ascii_W = 0x57, ascii_w = 0x77,
    ascii_X = 0x58, ascii_x = 0x78,
    ascii_Y = 0x59, ascii_y = 0x79,
    ascii_Z = 0x5A, ascii_z = 0x7A,

    // punctuations

    ascii_exclamation   = 0x21,
    ascii_double_quote  = 0x22,
    ascii_number        = 0x23,
    ascii_dollar        = 0x24,
    ascii_percent       = 0x25,
    ascii_ampersand     = 0x26,
    ascii_single_quote  = 0x27,
    ascii_left_paren    = 0x28,
    ascii_right_paren   = 0x29,
    ascii_asterisk      = 0x2A, ascii_colon     = 0x3A,
    ascii_plus          = 0x2B, ascii_semicolon = 0x3B,
    ascii_comma         = 0x2C, ascii_less      = 0x3C,
    ascii_minus         = 0x2D, ascii_equals    = 0x3D,
    ascii_dot           = 0x2E, ascii_greater   = 0x3E,
    ascii_slash         = 0x2F, ascii_question  = 0x3F,

    ascii_at            = 0x40, ascii_back_quote   = 0x60,
    ascii_left_bracket  = 0x5B, ascii_left_brace   = 0x7B,
    ascii_backslash     = 0x5C, ascii_vertical_bar = 0x7C,
    ascii_right_bracket = 0x5D, ascii_right_brace  = 0x7D,
    ascii_circumflex    = 0x5E, ascii_tilde        = 0x7E,
    ascii_underscore    = 0x5F,
  };

  const char* get_ascii_name(char32_t value);

  enum character_flags {
    unicode_mask      = 0x001FFFFF,
    unicode_max       = 0x0010FFFF,
    charflag_iso2022  = 0x01000000,
    charflag_object   = 0x02000000, // not yet supported
    charflag_private1 = 0x04000000, // for private usage
    charflag_private2 = 0x08000000, // for private usage

    // 0x01XXXXXX は iso-2022 による文字に使う。
    //   0x010XXXXX ... 94/96 文字集合
    //     96文字を単位とする"区"で区切る。
    //     第0区は ASCII として予約
    //     第1区は ISO-8859-1 として予約
    //   0x011XXXXX ... 94^2 文字集合
    //     96*96文字を単位とする"面"で区切る。
    //     第0面は contra 描画図形として予約
    //   0x012XXXXX ... DRCS 94/96
    //     初めの160区はDRCSMM領域として予約
    //   0x013XXXXX ... DRCS 94^2/96^2
    charflag_iso2022_db    = 0x00100000,
    charflag_iso2022_drcs  = 0x00200000,
    charflag_iso2022_mask_flag  = 0x00300000,
    charflag_iso2022_mask_code  = 0x000FFFFF,
    charflag_iso2022_mosaic_beg = 0x01100000,
    charflag_iso2022_mosaic_end = 0x01100000 + 96 * 96,

    charflag_wide_extension    = 0x10000000,
    charflag_cluster_extension = 0x20000000,
    charflag_marker            = 0x40000000,

    marker_base    = charflag_marker | 0x00200000,
    marker_sds_l2r = marker_base + 1,
    marker_sds_r2l = marker_base + 2,
    marker_sds_end = marker_base + 3,
    marker_srs_beg = marker_base + 4,
    marker_srs_end = marker_base + 5,

    // Note: Unicode 表示のある marker に関しては
    // charflag_marker を外せばその文字になる様にする。

    // Unicode bidi markers
    marker_lre = charflag_marker | 0x202A,
    marker_rle = charflag_marker | 0x202B,
    marker_pdf = charflag_marker | 0x202C,
    marker_lro = charflag_marker | 0x202D,
    marker_rlo = charflag_marker | 0x202E,
    marker_lri = charflag_marker | 0x2066,
    marker_rli = charflag_marker | 0x2067,
    marker_fsi = charflag_marker | 0x2068,
    marker_pdi = charflag_marker | 0x2069,

    invalid_character = 0xFFFFFFFF,
  };

  //---------------------------------------------------------------------------
  // key_t

  typedef std::uint32_t key_t;

  enum key_flags {
    _character_mask = 0x001FFFFF,
    _unicode_max    = 0x0010FFFF,
    _key_base       = 0x00110000,

    _modifier_mask  = 0xFF000000,
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
    _key_fn_first = key_f1,
    _key_fn_last = key_f24,

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
    _key_cursor_first = key_insert,
    _key_cursor_last = key_down,

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
    key_kpf1      = _key_base | 60,
    key_kpf2      = _key_base | 61,
    key_kpf3      = _key_base | 62,
    key_kpf4      = _key_base | 63,
    key_kpent     = _key_base | 64,
    key_kpeq      = _key_base | 65,
    _key_kp_first = key_kp0,
    _key_kp_last = key_kpeq,

    modifier_shift       = 0x01000000,
    modifier_meta        = 0x02000000,
    modifier_control     = 0x04000000,
    modifier_super       = 0x08000000,
    modifier_hyper       = 0x10000000,
    modifier_alter       = 0x20000000,
    modifier_application = 0x40000000,
    modifier_autorepeat  = 0x80000000,

    // focus in/out
    key_focus     = _key_base | 58,
    key_blur      = _key_base | 59,

    // mouse events
    _key_mouse_button_mask = 0x0700,
    _key_mouse_button_shft = 8,
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
  bool parse_modifier(key_t& value, const char* text);

  //---------------------------------------------------------------------------
  // prototype/forward declaration

  namespace ansi {
    struct cell_t;
    class line_t;
    struct board_t;
    class term_t;

    typedef std::int32_t coord_t;
    typedef std::int32_t curpos_t;
  }

  //---------------------------------------------------------------------------
  // Implementation limitations

  namespace limit {
    template<typename T>
    class range {
      T m_min, m_max;
    public:
      constexpr range(T min, T max): m_min(min), m_max(max) {}
      constexpr T min() const { return m_min; }
      constexpr T max() const { return m_max; }
      constexpr T clamp(T value) const {
        return contra::clamp(value, m_min, m_max);
      }
    };

    constexpr range<ansi::curpos_t> term_col {1, 2048};
    constexpr range<ansi::curpos_t> term_row {1, 2048};
    constexpr range<ansi::coord_t> term_xpixel {4, 512}; // SGR装飾の類で仮定?
    constexpr range<ansi::coord_t> term_ypixel {4, 512}; // SGR装飾の類で仮定?

    constexpr std::size_t maximal_cells_per_line = term_col.max() * 5;
  }
}
#endif
