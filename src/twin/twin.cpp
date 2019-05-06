#include <string.h>
#include <tchar.h>
#include <wchar.h>
#include <windows.h>
#include <windowsx.h>
#include <imm.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <sstream>
#include <iomanip>
#include <memory>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <tuple>
#include <limits>
#include <mwg/except.h>
#include "../contradef.hpp"
#include "../enc.utf8.hpp"
#include "../util.hpp"
#include "../pty.hpp"
#include "../manager.hpp"
#include "win_messages.hpp"

#define _tcslen wcslen
#define _tcscpy_s wcscpy_s
#define _tcscpy wcscpy

//#define contra_dbgout
#ifdef contra_dbgout
std::FILE* dbgout = NULL;
# define dbgprintf(...) do { std::fprintf(dbgout, __VA_ARGS__); std::fflush(dbgout); } while(0)
#else
# define dbgprintf(...) do {} while (0)
#endif

namespace contra {
namespace twin {

  template<typename XCH>
  int xcscpy_s(XCH* dst, std::size_t sz, const XCH* src) {
    if (!sz--) return 1;
    char c;
    while (sz-- && (c = *src++)) *dst++ = c;
    *dst = '\0';
    return 0;
  }

  using namespace contra::term;
  using namespace contra::ansi;

  struct twin_settings {
    curpos_t m_col = 80;//@size
    curpos_t m_row = 30;
    coord_t m_xpixel = 7;
    coord_t m_ypixel = 13;
    coord_t window_size_xadjust = 0;
    coord_t window_size_yadjust = 0;
    coord_t m_xframe = 1;
    coord_t m_yframe = 1;

    coord_t m_caret_underline_min_height = 2;
    coord_t m_caret_vertical_min_width = 2;
    bool m_caret_hide_on_ime = true;
    UINT m_caret_interval = 400;

    UINT m_blinking_interval = 200;

    const char* m_env_term = "xterm-256color";

    bool m_debug_print_window_messages = false;
    bool m_debug_print_unknown_key = false;

  public:
    coord_t calculate_client_width() const {
      return m_xpixel * m_col + 2 * m_xframe;
    }
    coord_t calculate_client_height() const {
      return m_ypixel * m_row + 2 * m_yframe;
    }
    coord_t calculate_window_width() const {
      return calculate_client_width() + window_size_xadjust;
    }
    coord_t calculate_window_height() const {
      return calculate_client_height() + window_size_yadjust;
    }

  };

  typedef std::uint32_t font_t;

  enum font_flags {
    // bit 0-3: fontface
    font_face_mask     = 0x000F,
    font_face_shft     = 0,
    font_face_fraktur  = 0x000A,

    // bit 4: italic
    font_flag_italic   = 0x0010,

    // bit 5-6: weight
    font_weight_mask   = 0x0060,
    font_weight_shft   = 4,
    font_weight_bold   = 0x0020,
    font_weight_faint  = 0x0040,
    font_weight_heavy  = 0x0060,

    // bit 7: small
    font_flag_small    = 0x0080,

    // bit 8-10: rotation
    font_rotation_mask = 0x0700,
    font_rotation_shft = 8,

    // bit 12-13: DECDWL, DECDHL
    font_decdhl_mask   = 0x3000,
    font_decdhl_shft   = 12,
    font_decdhl        = 0x1000,
    font_decdwl        = 0x2000,

    // 11x2x2 種類のフォントは記録する (下位6bit)。
    font_basic_mask = font_face_mask | font_weight_bold | font_flag_italic,
    font_basic_bits = 6,

    font_cache_count = 32,

    font_layout_mask         = 0xFF0000,
    font_layout_shft         = 16,
    font_layout_sup          = 0x010000,
    font_layout_sub          = 0x020000,
    font_layout_framed       = 0x040000,
    font_layout_upper_half   = 0x080000,
    font_layout_lower_half   = 0x100000,
    font_layout_proportional = 0x200000,
  };

  class font_store_t {
    LOGFONT m_logfont_normal;

    HFONT m_fonts[1u << font_basic_bits];
    std::pair<font_t, HFONT> m_cache[font_cache_count];

    LONG m_width;
    LONG m_height;

    LPCTSTR m_fontnames[16];

    void release() {
      for (auto& hfont : m_fonts) {
        if (hfont) ::DeleteObject(hfont);
        hfont = NULL;
      }
      auto const init = std::pair<font_t, HFONT>(0u, NULL);
      for (auto& pair : m_cache) {
        if (pair.second) ::DeleteObject(pair.second);
        pair = init;
      }
    }
  public:
    font_store_t(twin_settings const& settings) {
      std::fill(std::begin(m_fonts), std::end(m_fonts), (HFONT) NULL);
      std::fill(std::begin(m_cache), std::end(m_cache), std::pair<font_t, HFONT>(0u, NULL));
      std::fill(std::begin(m_fontnames), std::end(m_fontnames), (LPCTSTR) NULL);

      // My configuration@font
      m_fontnames[0]  = TEXT("MeiryoKe_Console");
      m_fontnames[1]  = TEXT("MS Gothic");
      m_fontnames[2]  = TEXT("MS Mincho");
      m_fontnames[3]  = TEXT("HGMaruGothicMPRO"); // HG丸ｺﾞｼｯｸM-PRO
      m_fontnames[4]  = TEXT("HGKyokashotai"); // HG教科書体
      m_fontnames[5]  = TEXT("HGGyoshotai"); // HG行書体
      m_fontnames[6]  = TEXT("HGSeikaishotaiPRO"); // HG正楷書体-PRO
      m_fontnames[7]  = TEXT("HGSoeiKakupoptai"); // HG創英角ﾎﾟｯﾌﾟ体
      m_fontnames[8]  = TEXT("HGGothicM"); // HGｺﾞｼｯｸM
      // m_fontnames[9]  = TEXT("HGMinchoB"); // HG明朝B
      m_fontnames[9]  = TEXT("Times New Roman"); // HG明朝B
      m_fontnames[10] = TEXT("aghtex_mathfrak");

      // ゴシック系のフォント
      //   HGｺﾞｼｯｸE
      //   HGｺﾞｼｯｸM
      //   HG創英角ｺﾞｼｯｸUB
      // 明朝系のフォント
      //   HG明朝B
      //   HG明朝E
      //   HG創英ﾌﾟﾚｾﾞﾝｽEB

      // default settings
      m_width = settings.m_xpixel;
      m_height = settings.m_ypixel;
      m_logfont_normal.lfHeight = m_height;
      m_logfont_normal.lfWidth  = m_width;
      m_logfont_normal.lfEscapement = 0;
      m_logfont_normal.lfOrientation = 0;
      m_logfont_normal.lfWeight = FW_NORMAL;
      m_logfont_normal.lfItalic = FALSE;
      m_logfont_normal.lfUnderline = FALSE;
      m_logfont_normal.lfStrikeOut = FALSE;
      m_logfont_normal.lfCharSet = DEFAULT_CHARSET;
      m_logfont_normal.lfOutPrecision = OUT_DEFAULT_PRECIS;
      m_logfont_normal.lfClipPrecision = CLIP_LH_ANGLES;
      //m_logfont_normal.lfClipPrecision = CLIP_DEFAULT_PRECIS;
      m_logfont_normal.lfQuality = CLEARTYPE_QUALITY;
      m_logfont_normal.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
      xcscpy_s(m_logfont_normal.lfFaceName, LF_FACESIZE, m_fontnames[0]);
    }

    void set_size(LONG width, LONG height) {
      if (m_width == width && m_height == height) return;
      release();
      this->m_width = width;
      this->m_height = height;
      m_logfont_normal.lfWidth = width;
      m_logfont_normal.lfHeight = height;
    }
    LONG width() const { return m_width; }
    LONG height() const { return m_height; }

    LOGFONT const& logfont_normal() { return m_logfont_normal; }

    HFONT normal() {
      return get_font(0);
    }

  public:
    coord_t small_height() const {
      return std::max<coord_t>(8u, std::min(m_height - 4, (m_height * 7 + 9) / 10));
    }
    coord_t small_width() const {
      return std::max<coord_t>(4u, std::min(m_width - 4, (m_width * 8 + 9) / 10));
    }

    coord_t get_font_height(font_t font) const {
      coord_t height = font & font_flag_small ? small_height() : m_height;
      if (font & font_decdhl) height *= 2;
      return height;
    }
    std::pair<coord_t, coord_t> get_font_size(font_t font) const {
      coord_t width = m_width, height = m_height;
      if (font & font_flag_small) {
        height = small_height();
        width = small_width();
      }
      if (font & font_decdhl) height *= 2;
      if (font & font_decdwl) width *= 2;
      return {width, height};
    }

  public:
    LOGFONT create_logfont(font_t font) const {
      LOGFONT logfont = m_logfont_normal;
      if (font_t const face = (font & font_face_mask) >> font_face_shft)
        if (LPCTSTR const fontname = m_fontnames[face])
          xcscpy_s(logfont.lfFaceName, LF_FACESIZE, fontname);
      if (font & font_flag_italic)
        logfont.lfItalic = TRUE;

      switch (font & font_weight_mask) {
      case font_weight_bold: logfont.lfWeight = FW_BOLD; break;
      case font_weight_faint: logfont.lfWeight = FW_THIN; break;
      case font_weight_heavy: logfont.lfWeight = FW_HEAVY; break;
      }

      if (font & font_flag_small) {
        logfont.lfHeight = small_height();
        logfont.lfWidth = small_width();
      }
      if (font & font_decdhl) logfont.lfHeight *= 2;
      if (font & font_decdwl) logfont.lfWidth *= 2;
      if (font_t const rotation = (font & font_rotation_mask) >> font_rotation_shft) {
        logfont.lfOrientation = 450 * rotation;
        logfont.lfEscapement  = 450 * rotation;
      }
      return logfont; /* NRVO */
    }

  private:
    HFONT create_font(font_t font) const {
      LOGFONT const logfont = create_logfont(font);
      return ::CreateFontIndirect(&logfont);
    }

  public:
    HFONT get_font(font_t font) {
      font &= ~font_layout_mask;

      if (!(font & ~font_basic_mask)) {
        // basic fonts
        if (!m_fonts[font]) m_fonts[font] = create_font(font);
        return m_fonts[font];
      } else {
        // cached fonts (先頭にある物の方が新しい)
        for (std::size_t i = 0; i < std::size(m_cache); i++) {
          if (m_cache[i].first == font) {
            // 見つかった時は、見つかった物を先頭に移動して、返す。
            HFONT const ret = m_cache[i].second;
            std::move_backward(&m_cache[0], &m_cache[i], &m_cache[i + 1]);
            m_cache[0] = std::make_pair(font, ret);
            return ret;
          }
        }

        // 見つからない時は末尾にあった物を削除して、新しく作る。
        if (HFONT last = std::end(m_cache)[-1].second) ::DeleteObject(last);
        std::move_backward(std::begin(m_cache), std::end(m_cache) - 1, std::end(m_cache));

        HFONT const ret = create_font(font);
        m_cache[0] = std::make_pair(font, ret);
        return ret;
      }
    }

    /*?lwiki
     * @fn std::tuple<coord_t, coord_t, double> get_displacement(font_t font);
     * @return dx は描画の際の横のずれ量(文字幅1の文字に対して)
     * @return dy は描画の際の縦のずれ量
     * @return dxW は (文字の幅-1) に比例して増える各文字の横のずれ量
     */
    std::tuple<coord_t, coord_t, double> get_displacement(font_t font) {
      double dx = 0, dy = 0, dxW = 0;
      if (font & (font_layout_mask | font_decdwl | font_flag_italic)) {
        // Note: 横のずれ量は dxI + dxW * cell.width である。
        // これを実際には幅1の時の値 dx = dxI + dxW を分離して、
        // ずれ量 = dx + dxW * (cell.width - 1) と計算する。

        double dxI = 0;
        if (font & font_layout_sup)
          dy -= 0.2 * m_height;
        else if (font & font_layout_sub) {
          // Note: 下端は baseline より下なので下端に合わせれば十分。
          dy += m_height - this->small_height();
        }
        if (font & (font_layout_framed | font_layout_sup | font_layout_sub)) {
          dxW = 0.5 * (m_width - this->small_width());
          dy += 0.5 * (m_height - this->small_height());
        }
        if (font & font_flag_italic) {
          coord_t width = this->width();
          if (font & (font_layout_framed | font_layout_sup | font_layout_sub))
            width = this->small_width();
          dx -= 0.4 * width;
        }
        if (font & (font_layout_upper_half | font_layout_lower_half)) dy *= 2;
        if (font & font_decdwl) dxI *= 2;
        if (font & font_layout_lower_half) dy -= m_height;
        dx = std::round(dxI + dxW);
        dy = std::round(dy);
      }
      return {dx, dy, dxW};
    }

    ~font_store_t() {
      release();
    }
  };

  LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

  class twin_window_t {
    twin_settings settings;
    font_store_t fstore { settings };

    terminal_manager manager;

    HWND hWnd = NULL;

    static constexpr LPCTSTR szClassName = TEXT("Contra.Twin.Main");

  private:
    bool is_session_ready() { return hWnd && manager.is_active(); }
  public:
    HWND create_window(HINSTANCE hInstance) {
      fstore.set_size(settings.m_xpixel, settings.m_ypixel);

      static bool is_window_class_registered = false;
      if (!is_window_class_registered) {
        is_window_class_registered = true;

        WNDCLASS myProg;
        myProg.style         = CS_HREDRAW | CS_VREDRAW;
        myProg.lpfnWndProc   = contra::twin::WndProc;
        myProg.cbClsExtra    = 0;
        myProg.cbWndExtra    = 0;
        myProg.hInstance     = hInstance;
        myProg.hIcon         = NULL;
        myProg.hCursor       = LoadCursor(NULL, IDC_ARROW);
        myProg.hbrBackground = (HBRUSH) ::GetStockObject(WHITE_BRUSH);
        myProg.lpszMenuName  = NULL;
        myProg.lpszClassName = szClassName;
        if (!RegisterClass(&myProg)) return NULL;
      }

      settings.window_size_xadjust = 2 * ::GetSystemMetrics(SM_CXSIZEFRAME);
      settings.window_size_yadjust = 2 * ::GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CYCAPTION);
      this->hWnd = CreateWindowEx(
        0, //WS_EX_COMPOSITED,
        szClassName,
        TEXT("Contra/Cygwin"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        settings.calculate_window_width(),
        settings.calculate_window_height(),
        NULL,
        NULL,
        hInstance,
        NULL);

      this->start_blinking_timer();
      return hWnd;
    }
    bool m_window_size_adjusted = false;
    void adjust_window_size_using_current_client_size() {
      if (m_window_size_adjusted) return;
      m_window_size_adjusted = true;

      RECT rcClient;
      ::GetClientRect(hWnd, &rcClient);
      LONG const width = rcClient.right - rcClient.left;
      LONG const height = rcClient.bottom - rcClient.top;
      coord_t const xadjust = settings.calculate_client_width() - (coord_t) width;
      coord_t const yadjust = settings.calculate_client_height() - (coord_t) height;
      if (xadjust || yadjust) {
        settings.window_size_xadjust += xadjust;
        settings.window_size_yadjust += yadjust;
        ::SetWindowPos(
          hWnd, NULL, 0, 0,
          settings.calculate_window_width(), settings.calculate_window_height(),
          SWP_NOZORDER | SWP_NOMOVE);
      }
    }
    void adjust_terminal_size_using_current_client_size() {
      if (!m_window_size_adjusted) return;
      if (!is_session_ready()) return;
      RECT rcClient;
      ::GetClientRect(hWnd, &rcClient);
      coord_t const width = rcClient.right - rcClient.left;
      coord_t const height = rcClient.bottom - rcClient.top;
      curpos_t const new_col = std::max(1, (width - 2 * settings.m_xframe) / settings.m_xpixel);
      curpos_t const new_row = std::max(1, (height - 2 * settings.m_yframe) / settings.m_ypixel);

      if (new_col != settings.m_col || new_row != settings.m_row) {
        settings.m_col = new_col;
        settings.m_row = new_row;
        manager.reset_size(new_col, new_row);
      }
    }

  private:
    class compatible_bitmap_t {
      LONG m_width = -1, m_height = -1;
      struct layer_t {
        HDC hdc = NULL;
        HBITMAP hbmp = NULL;
      };
      std::vector<layer_t> m_layers;

      void release() {
        for (layer_t const& layer : m_layers) {
          if (layer.hdc) ::DeleteDC(layer.hdc);
          if (layer.hbmp) ::DeleteObject(layer.hbmp);
        }
        m_layers.clear();
      }

    private:
      void check_window_size(HWND hWnd, bool* out_resized = NULL) {
        RECT rcClient;
        ::GetClientRect(hWnd, &rcClient);
        LONG const width = rcClient.right - rcClient.left;
        LONG const height = rcClient.bottom - rcClient.top;
        bool const is_resized = width != m_width || height != m_height;
        if (is_resized) {
          release();
          m_width = width;
          m_height = height;
        }
        if (out_resized) *out_resized = is_resized;
      }
    public:
      HDC hdc(HWND hWnd, HDC hdc, std::size_t index = 0, bool* out_resized = NULL) {
        if (out_resized)
          check_window_size(hWnd, out_resized);
        if (index < m_layers.size() && m_layers[index].hdc)
          return m_layers[index].hdc;

        if (index >= m_layers.size()) m_layers.resize(index + 1);
        layer_t& layer = m_layers[index];
        layer.hbmp = ::CreateCompatibleBitmap(hdc, m_width, m_height);
        layer.hdc = ::CreateCompatibleDC(hdc);
        ::SelectObject(layer.hdc, layer.hbmp);
        return layer.hdc;
      }

      HBITMAP bmp(std::size_t index = 0) const {
        return index < m_layers.size() ? m_layers[index].hbmp : (HBITMAP) NULL;
      }
      LONG width() { return m_width; }
      LONG height() { return m_height; }
      ~compatible_bitmap_t() {
        release();
      }
    };

    compatible_bitmap_t m_background;

  private:
    class status_tracer_t {
    public:
      status_tracer_t() {}

      void store(twin_window_t const& win, terminal_application const& app) {
        store_metric(win);
        store_content(win, app);
        store_cursor(win, app);
        store_blinking_state(win);
      }

    private:
      coord_t m_xframe = 0;
      coord_t m_yframe = 0;
      curpos_t m_col = 0;
      curpos_t m_row = 0;
      coord_t m_xpixel = 0;
      coord_t m_ypixel = 0;
    public:
      bool is_metric_changed(twin_window_t const& win) const {
        twin_settings const& st = win.settings;
        if (m_xframe != st.m_xframe) return true;
        if (m_yframe != st.m_yframe) return true;
        if (m_col != st.m_col) return true;
        if (m_row != st.m_row) return true;
        if (m_xpixel != st.m_xpixel) return true;
        if (m_ypixel != st.m_ypixel) return true;
        return false;
      }
      void store_metric(twin_window_t const& win) {
        twin_settings const& st = win.settings;
        m_xframe = st.m_xframe;
        m_yframe = st.m_yframe;
        m_col = st.m_col;
        m_row = st.m_row;
        m_xpixel = st.m_xpixel;
        m_ypixel = st.m_ypixel;
      }

    private:
      struct line_trace_t {
        std::uint32_t id = (std::uint32_t) -1;
        std::uint32_t version = 0;
        bool has_blinking = false;
      };
      std::vector<line_trace_t> m_lines;
    public:
      bool is_content_changed([[maybe_unused]] twin_window_t const& win, terminal_application const& app) const {
        board_t const& b = app.board();
        std::size_t const height = b.m_height;
        if (height != m_lines.size()) return true;
        for (std::size_t iline = 0; iline < height; iline++) {
          line_t const& line = b.line(iline);
          if (line.id() != m_lines[iline].id) return true;
          if (line.version() != m_lines[iline].version) return true;
        }
        return false;
      }
      void store_content([[maybe_unused]] twin_window_t const& win, terminal_application const& app) {
        board_t const& b = app.board();
        m_lines.resize(b.m_lines.size());
        for (std::size_t i = 0; i < m_lines.size(); i++) {
          m_lines[i].id = b.m_lines[i].id();
          m_lines[i].version = b.m_lines[i].version();
          m_lines[i].has_blinking = b.m_lines[i].has_blinking_cells();
        }
      }

    private:
      std::uint32_t m_blinking_count = 0;
    public:
      bool has_blinking_cells() const {
        for (line_trace_t const& line : m_lines)
          if (line.has_blinking) return true;
        return false;
      }
      bool is_blinking_changed(twin_window_t const& win) const {
        if (!this->has_blinking_cells()) return false;
        return this->m_blinking_count != win.m_blinking_count;
      }
      void store_blinking_state(twin_window_t const& win) {
        this->m_blinking_count = win.m_blinking_count;
      }

    private:
      // todo: カーソル状態の記録
      //   実はカーソルは毎回描画でも良いのかもしれない
      bool m_cur_visible = false;
      bool m_cur_blinking = true;
      curpos_t m_cur_x = 0;
      curpos_t m_cur_y = 0;
      bool m_cur_xenl = false;
      int m_cur_shape = 0;
    public:
      curpos_t cur_x() const { return m_cur_x; }
      curpos_t cur_y() const { return m_cur_y; }
      bool is_cursor_changed(twin_window_t const& win, terminal_application const& app) const {
        if (m_cur_visible != win.is_cursor_visible(app)) return true;
        board_t const& b = app.board();
        tstate_t const& s = app.state();
        if (m_cur_x != b.cur.x() || m_cur_y != b.cur.y() || m_cur_xenl != b.cur.xenl()) return true;
        if (m_cur_shape != s.get_cursor_shape()) return true;
        if (m_cur_blinking != s.is_cursor_blinking()) return true;
        return false;
      }
      void store_cursor(twin_window_t const& win, terminal_application const& app) {
        m_cur_visible = win.is_cursor_visible(app);
        board_t const& b = app.board();
        tstate_t const& s = app.state();
        m_cur_x = b.cur.x();
        m_cur_y = b.cur.y();
        m_cur_xenl = b.cur.xenl();
        m_cur_shape = s.m_cursor_shape;
      }
    };

    status_tracer_t m_tracer;

  private:
    static constexpr UINT blinking_timer_id = 11; // 適当
    UINT m_blinking_timer_id = 0;
    std::uint32_t m_blinking_count = 0;
    void start_blinking_timer() {
      if (m_blinking_timer_id)
        ::KillTimer(hWnd, m_blinking_timer_id);
      m_blinking_timer_id = ::SetTimer(hWnd, blinking_timer_id, settings.m_blinking_interval, NULL);
    }
    void reset_blinking_timer() {
      if (!is_session_ready()) return;
      start_blinking_timer();
    }
    void process_blinking_timer() {
      m_blinking_count++;
      if (m_tracer.has_blinking_cells())
        render_window();
    }

  private:
    static constexpr UINT cursor_timer_id = 10; // 適当
    UINT m_cursor_timer_id = 0;
    std::uint32_t m_cursor_timer_count = 0;

    void unset_cursor_timer() {
      if (m_cursor_timer_id)
        ::KillTimer(hWnd, m_cursor_timer_id);
    }
    void reset_cursor_timer() {
      if (!is_session_ready()) return;
      this->unset_cursor_timer();
      m_cursor_timer_id = ::SetTimer(hWnd, cursor_timer_id, settings.m_caret_interval, NULL);
      m_cursor_timer_count = 0;
    }
    void process_cursor_timer() {
      m_cursor_timer_count++;
      render_window();
    }
    bool is_cursor_visible(terminal_application const& app) const {
      if (m_ime_composition_active &&
        settings.m_caret_hide_on_ime) return false;
      return app.state().is_cursor_visible();
    }
    void draw_cursor(HDC hdc, terminal_application const& app) {
      using namespace contra::ansi;
      term_t const& term = app.term();
      board_t const& b = term.board();
      curpos_t const x = b.cur.x(), y = b.cur.y();
      bool const xenl = b.cur.xenl();
      int const cursor_shape = term.state().m_cursor_shape;

      coord_t const xorigin = settings.m_xframe;
      coord_t const yorigin = settings.m_yframe;
      coord_t const ypixel = settings.m_ypixel;
      coord_t const xpixel = settings.m_xpixel;

      coord_t x0 = xorigin + xpixel * x;
      coord_t const y0 = yorigin + ypixel * y;

      coord_t size;
      bool underline = false;
      if (xenl) {
        // 行末にいる時は設定に関係なく縦棒にする。
        x0 -= 2;
        size = 2;
      } else if (cursor_shape == 0) {
        underline = true;
        size = 3;
      } else if (cursor_shape < 0) {
        size = (xpixel * std::min(100, -cursor_shape) + 99) / 100;
      } else if (cursor_shape) {
        underline = true;
        size = (ypixel * std::min(100, cursor_shape) + 99) / 100;
      }

      if (underline) {
        coord_t const height = std::min(ypixel, std::max(size, settings.m_caret_underline_min_height));
        coord_t const x1 = x0, y1 = y0 + ypixel - height;
        ::PatBlt(hdc, x1, y1, xpixel, height, DSTINVERT);
      } else {
        coord_t const width = std::min(xpixel, std::max(size, settings.m_caret_vertical_min_width));
        ::PatBlt(hdc, x0, y0, width, ypixel, DSTINVERT);
      }
    }

    class color_resolver_t {
      using color_t = contra::dict::color_t;
      using attribute_t = contra::dict::attribute_t;
      using tstate_t = contra::ansi::tstate_t;

      tstate_t const* s;
      byte m_space = (byte) attribute_t::color_space_default;
      color_t m_color = color_t(-1);
      color_t m_rgba = 0;

    public:
      color_resolver_t(tstate_t const& s): s(&s) {}

    public:
      color_t resolve(byte space, color_t color) {
        color_t rgba = 0;
        switch (space) {
        case attribute_t::color_space_default:
        case attribute_t::color_space_transparent:
        default:
          // transparent なので色はない。
          rgba = 0x00000000; // black
          break;
        case attribute_t::color_space_indexed:
          rgba = s->m_rgba256[color & 0xFF];
          break;
        case attribute_t::color_space_rgb:
          rgba = contra::dict::rgb2rgba(color);
          break;
        case attribute_t::color_space_cmy:
          rgba = contra::dict::cmy2rgba(color);
          break;
        case attribute_t::color_space_cmyk:
          rgba = contra::dict::cmyk2rgba(color);
          break;
        }

        m_space = space;
        m_color = color;
        m_rgba = rgba;
        return rgba;
      }

    private:
      std::pair<byte, color_t> get_fg(attribute_t const& attr) {
        byte space = attr.fg_space();
        color_t color = attr.fg_color();
        if (space == 0) {
          space = s->m_default_fg_space;
          color = s->m_default_fg_color;
        }
        return {space, color};
      }
      std::pair<byte, color_t> get_bg(attribute_t const& attr) {
        byte space = attr.bg_space();
        color_t color = attr.bg_color();
        if (space == 0) {
          space = s->m_default_bg_space;
          color = s->m_default_bg_color;
        }
        return {space, color};
      }
    public:
      color_t resolve_fg(attribute_t const& attr) {
        bool const inverse = attr.aflags & attribute_t::is_inverse_set;
        bool const selected = attr.xflags & attribute_t::ssa_selected;
        auto [space, color] = inverse != selected ? get_bg(attr) : get_fg(attr);
        if (space == m_space && color == m_color) return m_rgba;
        return resolve(space, color);
      }

      color_t resolve_bg(attribute_t const& attr) {
        bool const inverse = attr.aflags & attribute_t::is_inverse_set;
        bool const selected = attr.xflags & attribute_t::ssa_selected;
        auto [space, color] = inverse != selected ? get_fg(attr) : get_bg(attr);
        if (space == m_space && color == m_color) return m_rgba;
        return resolve(space, color);
      }
    };

    class font_resolver_t {
      using attribute_t = contra::dict::attribute_t;
      using xflags_t = contra::dict::xflags_t;
      using aflags_t = contra::dict::aflags_t;

      aflags_t m_aflags = 0;
      xflags_t m_xflags = 0;
      font_t m_font = 0;

    public:
      font_t resolve_font(attribute_t const& attr) {
        if (attr.aflags == m_aflags && attr.xflags == m_xflags) return m_font;

        font_t ret = 0;
        if (xflags_t const face = (attr.xflags & attribute_t::ansi_font_mask) >> attribute_t::ansi_font_shift)
          ret |= font_face_mask & face << font_face_shft;

        switch (attr.aflags & attribute_t::is_heavy_set) {
        case attribute_t::is_bold_set: ret |= font_weight_bold; break;
        case attribute_t::is_faint_set: ret |= font_weight_faint; break;
        case attribute_t::is_heavy_set: ret |= font_weight_heavy; break;
        }

        if (attr.aflags & attribute_t::is_italic_set)
          ret |= font_flag_italic;
        else if (attr.aflags & attribute_t::is_fraktur_set)
          ret = (ret & ~font_face_mask) | font_face_fraktur;

        if (attr.aflags & attribute_t::is_bold_set)
          ret |= font_weight_bold;
        else if (attr.aflags & attribute_t::is_faint_set)
          ret |= font_weight_faint;

        if (attr.xflags & attribute_t::is_sup_set)
          ret |= font_flag_small | font_layout_sup;
        else if (attr.xflags & attribute_t::is_sub_set)
          ret |= font_flag_small | font_layout_sub;

        if (attr.xflags & (attribute_t::is_frame_set | attribute_t::is_circle_set))
          ret |= font_flag_small | font_layout_framed;

        if (attr.xflags & attribute_t::is_proportional_set)
          ret |= font_layout_proportional;

        if (xflags_t const sco = (attr.xflags & attribute_t::sco_mask) >> attribute_t::sco_shift)
          ret |= font_rotation_mask & sco << font_rotation_shft;

        switch (attr.xflags & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_double_width:
          ret |= font_decdwl;
          break;
        case attribute_t::decdhl_upper_half:
          ret |= font_decdwl | font_decdhl | font_layout_upper_half;
          break;
        case attribute_t::decdhl_lower_half:
          ret |= font_decdwl | font_decdhl | font_layout_lower_half;
          break;
        }

        m_aflags = attr.aflags;
        m_xflags = attr.xflags;
        m_font = ret;
        return ret;
      }
    };

    void draw_background(HDC hdc, terminal_application const& app, std::vector<std::vector<contra::ansi::cell_t>>& content) {
      using namespace contra::ansi;
      coord_t const xorigin = settings.m_xframe;
      coord_t const yorigin = settings.m_yframe;
      coord_t const ypixel = settings.m_ypixel;
      coord_t const xpixel = settings.m_xpixel;
      board_t const& b = app.board();
      tstate_t const& s = app.state();
      color_resolver_t _color(s);

      {
        RECT rc;
        ::SetRect(&rc, 0, 0, m_background.width(), m_background.height());
        color_t const bg = _color.resolve(s.m_default_bg_space, s.m_default_bg_color);
        HBRUSH brush = ::CreateSolidBrush(contra::dict::rgba2rgb(bg));
        ::FillRect(hdc, &rc, brush);
        ::DeleteObject(brush);
      }

      coord_t x = xorigin, y = yorigin;
      coord_t x0 = x;
      color_t bg0 = 0;
      auto _fill = [=, &x, &y, &x0, &bg0] () {
        if (x0 >= x || !bg0) return;
        RECT rc;
        ::SetRect(&rc, x0, y, x, y + ypixel);
        HBRUSH brush = ::CreateSolidBrush(contra::dict::rgba2rgb(bg0));
        ::FillRect(hdc, &rc, brush);
        ::DeleteObject(brush);
      };

      for (curpos_t iline = 0; iline < b.m_height; iline++, y += ypixel) {
        std::vector<cell_t>& cells = content[iline];
        x = xorigin;
        x0 = x;
        bg0 = 0;

        for (std::size_t i = 0; i < cells.size(); ) {
          auto const& cell = cells[i++];
          color_t const bg = _color.resolve_bg(cell.attribute);
          if (bg != bg0) {
            _fill();
            bg0 = bg;
            x0 = x;
          }
          x += cell.width * xpixel;
        }
        _fill();
      }
    }

    void draw_rotated_text_ext(
      HDC hdc, coord_t x0, coord_t y0, coord_t dx, coord_t dy, coord_t width,
      std::vector<TCHAR> const& characters, std::vector<INT> const& progress, font_t font) {
      font_t const sco = (font & font_rotation_mask) >> font_rotation_shft;
      double const angle = -(M_PI / 4.0) * sco;
      coord_t const h = fstore.get_font_height(font);

      // Note: 何故か知らないが Windows の TextOut の仕様か Font の仕様で
      //   90 度に比例しないフォントだと y 方向に文字がずれている。
      //   以下の式は色々測って調べた結果分かったフォントのずれの量である。
      double const gdi_xshift = sco & 1 ? 1.0 : 0;
      double const gdi_yshift = sco & 1 ? h * 0.3 : 0.0;

      // (xc1, yc1) はGDIの回転中心(文字の左上)から見た、希望の回転中心(文字の中央)の位置
      // (xc2, yc2) は希望の回転中心が回転後に移動する位置
      double const xc1 = 0.5 * width + gdi_xshift - dx;
      double const yc1 = 0.5 * h + gdi_yshift;
      double const xc2 = xc1 * std::cos(angle) - yc1 * std::sin(angle);
      double const yc2 = xc1 * std::sin(angle) + yc1 * std::cos(angle);
      int const rot_dx = (int) std::round(xc2 - xc1 + gdi_xshift);
      int const rot_dy = (int) std::round(yc2 - yc1 + gdi_yshift);
      ::ExtTextOut(hdc, x0 + dx - rot_dx, y0 + dy - rot_dy, 0, NULL, &characters[0], characters.size(), &progress[0]);
    }

    class decdhl_region_holder_t {
      LONG width, height;
      HRGN rgn1 = NULL, rgn2 = NULL;
      coord_t y = 0, ypixel = 0;
      void release() {
        if (rgn1) {
          ::DeleteObject(rgn1);
          rgn1 = NULL;
        }
        if (rgn2) {
          ::DeleteObject(rgn2);
          rgn2 = NULL;
        }
      }
    public:
      decdhl_region_holder_t(LONG width, LONG height): width(width), height(height) {}
      ~decdhl_region_holder_t() {
        release();
      }
      void next_line(coord_t y, coord_t ypixel) {
        release();
        this->y = y;
        this->ypixel = ypixel;
      }
      HRGN get_upper_region() {
        if (!rgn1) rgn1 = ::CreateRectRgn(0, 0, width, y + ypixel);
        return rgn1;
      }
      HRGN get_lower_region() {
        if (!rgn2) rgn2 = ::CreateRectRgn(0, y, width, height);
        return rgn2;
      }
    };

    void draw_characters(HDC hdc, terminal_application const& app, std::vector<std::vector<contra::ansi::cell_t>>& content) {
      using namespace contra::ansi;
      coord_t const xorigin = settings.m_xframe;
      coord_t const yorigin = settings.m_yframe;
      coord_t const ypixel = settings.m_ypixel;
      coord_t const xpixel = settings.m_xpixel;
      board_t const& b = app.board();
      tstate_t const& s = app.state();

      constexpr std::uint32_t flag_processed = character_t::flag_private1;

      aflags_t invisible_flags = attribute_t::is_invisible_set;
      if (m_blinking_count & 1) invisible_flags |= attribute_t::is_rapid_blink_set;
      if (m_blinking_count & 2) invisible_flags |= attribute_t::is_blink_set;
      auto _visible = [invisible_flags] (std::uint32_t code, aflags_t aflags, bool sp_visible = false) {
        return code != ascii_nul && (sp_visible || code != ascii_sp)
          && character_t::is_char(code & flag_processed)
          && !(aflags & invisible_flags);
      };

      color_resolver_t _color(s);
      font_resolver_t _font;

      coord_t x = xorigin, y = yorigin, xL = xorigin, xR = xorigin;
      coord_t dx = 0, dy = 0;
      double dxW = 0.0;
      std::vector<TCHAR> characters;
      std::vector<INT> progress;
      auto _push_char = [&characters, &progress, &xR, &dx, &dxW] (std::uint32_t code, INT prog, curpos_t width) {
        xR += prog;

        // 文字位置の補正
        if (dxW && width > 1) {
          int const shift = dxW * (width - 1);
          if (progress.size())
            progress.back() += shift;
          else
            dx += shift;
          prog -= shift;
        }

        if (code < 0x10000) {
          characters.push_back(code);
          progress.push_back(prog);
        } else {
          // surrogate pair
          code -= 0x10000;
          characters.push_back(0xD800 | (code >> 10 & 0x3FF));
          progress.push_back(prog);
          characters.push_back(0xDC00 | (code & 0x3FF));
          progress.push_back(0);
        }
      };

      decdhl_region_holder_t region(m_background.width(), m_background.height());

      for (curpos_t iline = 0; iline < b.m_height; iline++, y += ypixel) {
        std::vector<cell_t>& cells = content[iline];

        x = xorigin;

        region.next_line(y, ypixel);

        for (std::size_t i = 0; i < cells.size(); ) {
          auto const& cell = cells[i];
          auto const& attr = cell.attribute;
          xL = xR = x;
          coord_t const cell_progress = cell.width * xpixel;
          i++;
          x += cell_progress;
          std::uint32_t code = cell.character.value;
          code &= ~character_t::flag_cluster_extension;
          if (!_visible(code, cell.attribute.aflags)) continue;

          // 色の決定
          color_t const fg = _color.resolve_fg(attr);
          font_t const font = _font.resolve_font(attr);
          std::tie(dx, dy, dxW) = fstore.get_displacement(font);

          characters.clear();
          progress.clear();
          _push_char(code, cell_progress, cell.width);

          // 同じ色を持つ文字は同時に描画してしまう。
          for (std::size_t j = i; j < cells.size(); j++) {
            auto const& cell2 = cells[j];
            std::uint32_t code2 = cell2.character.value;
            coord_t const cell2_progress = cell2.width * xpixel;

            // 回転文字の場合は一つずつ書かなければならない。
            // 零幅の cluster などだけ一緒に描画する。
            if ((font & font_rotation_mask) && cell2_progress) break;

            bool const is_cluster = code2 & character_t::flag_cluster_extension;
            color_t const fg2 = _color.resolve_fg(cell2.attribute);
            font_t const font2 = _font.resolve_font(cell2.attribute);
            code2 &= ~character_t::flag_cluster_extension;
            if (!_visible(code2, cell2.attribute.aflags, font & font_layout_proportional)) {
              if (font & font_layout_proportional) break;
              progress.back() += cell2_progress;
            } else if (is_cluster || (fg == fg2 && font == font2)) {
              _push_char(code2, cell2_progress, cell2.width);
            } else {
              if (font & font_layout_proportional) break;
              progress.back() += cell2_progress;
              continue;
            }

            if (i == j) {
              i++;
              x += cell2_progress;
            } else
              cells[j].character.value |= flag_processed;
          }

          ::SetTextColor(hdc, contra::dict::rgba2rgb(fg));
          ::SelectObject(hdc, fstore.get_font(font));

          // DECDHL用の制限
          if (font & font_layout_upper_half)
            ::SelectClipRgn(hdc, region.get_upper_region());
          else if (font & font_layout_lower_half)
            ::SelectClipRgn(hdc, region.get_lower_region());

          if (font & font_rotation_mask) {
            this->draw_rotated_text_ext(hdc, xL, y, dx, dy, xR - xL, characters, progress, font);
          } else if (font & font_layout_proportional) {
            // 配置を GDI に任せる
            ::TextOut(hdc, xL + dx, y + dy, &characters[0], characters.size());
          } else {
            ::ExtTextOut(hdc, xL + dx, y + dy, 0, NULL, &characters[0], characters.size(), &progress[0]);
          }

          // DECDHL用の制限の解除
          if (font & (font_layout_upper_half | font_layout_lower_half))
            ::SelectClipRgn(hdc, NULL);
        }

        // clear private flags
        for (cell_t& cell : cells) cell.character.value &= ~flag_processed;
      }
    }

    template<typename F>
    struct decoration_horizontal_t {
      coord_t x0 = 0;
      std::uint32_t style = 0;
      F _draw;

    public:
      decoration_horizontal_t(F draw): _draw(draw) {}

    public:
      void update(curpos_t x, std::uint32_t new_style, xflags_t xflags) {
        if (new_style) {
          switch (xflags & attribute_t::decdhl_mask) {
          case attribute_t::decdhl_upper_half:
            new_style |= attribute_t::decdhl_upper_half;
            break;
          case attribute_t::decdhl_lower_half:
            new_style |= attribute_t::decdhl_lower_half;
            break;
          }
        }
        if (style != new_style) {
          if (style) _draw(x0, x, style);
          x0 = x;
          style = new_style;
        }
      }
    };

    class brush_holder_t {
      color_t m_color = 0;
      HBRUSH m_brush = NULL;
      HPEN m_pen = NULL;
      int m_pen_width = 0;
      void release() {
        if (m_brush) {
          ::DeleteObject(m_brush);
          m_brush = NULL;
        }
        if (m_pen) {
          ::DeleteObject(m_pen);
          m_pen = NULL;
        }
      }
    public:
      HBRUSH get_brush(color_t color) {
        if (m_color != color) release();
        m_color = color;
        if (!m_brush)
          m_brush = ::CreateSolidBrush(contra::dict::rgba2rgb(color));
        return m_brush;
      }
      HPEN get_pen(color_t color, int width) {
        if (m_color != color) release();
        m_color = color;
        if (!m_pen || m_pen_width != width) {
          if (m_pen) ::DeleteObject(m_pen);
          m_pen_width = width;
          m_pen = ::CreatePen(PS_SOLID, width, contra::dict::rgba2rgb(color));
        }
        return m_pen;
      }
      ~brush_holder_t() {
        release();
      }
    };

    void draw_decoration(HDC hdc, terminal_application const& app, std::vector<std::vector<contra::ansi::cell_t>>& content) {
      using namespace contra::ansi;
      coord_t const xorigin = settings.m_xframe;
      coord_t const yorigin = settings.m_yframe;
      coord_t const ypixel = settings.m_ypixel;
      coord_t const xpixel = settings.m_xpixel;
      board_t const& b = app.board();
      tstate_t const& s = app.state();
      color_resolver_t _color(s);

      coord_t x = xorigin, y = yorigin;

      color_t color0 = 0;
      brush_holder_t brush;
      decdhl_region_holder_t region(m_background.width(), m_background.height());

      decoration_horizontal_t dec_ul([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        coord_t h = (coord_t) std::ceil(ypixel * 0.05);
        switch (style & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_upper_half: return;
        case attribute_t::decdhl_lower_half:
          h *= 2;
          break;
        }
        RECT rc;
        HBRUSH const hbr = brush.get_brush(color0);
        ::SetRect(&rc, x1, y + ypixel - h, x2, y + ypixel);
        ::FillRect(hdc, &rc, hbr);
        if ((style & ~attribute_t::decdhl_mask) == 2) {
          ::SetRect(&rc, x1, y + ypixel - 3 * h, x2, y + ypixel - 2 * h);
          ::FillRect(hdc, &rc, hbr);
        }
      });

      decoration_horizontal_t dec_sl([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        HBRUSH const hbr = brush.get_brush(color0);
        RECT rc;
        coord_t h = (coord_t) std::ceil(ypixel * 0.05), y2 = y + ypixel / 2;
        if ((style & ~attribute_t::decdhl_mask) != 2) {
          // 一重打ち消し線
          switch (style & attribute_t::decdhl_mask) {
          case attribute_t::decdhl_lower_half: return;
          case attribute_t::decdhl_upper_half:
            h *= 2; y2 = y + ypixel;
            break;
          }
        } else {
          // 二重打ち消し線
          switch (style & attribute_t::decdhl_mask) {
          case attribute_t::decdhl_lower_half:
            h *= 2; y2 = y + h; break;
          case attribute_t::decdhl_upper_half:
            h *= 2; y2 = y + ypixel - h; break;
          default:
            ::SetRect(&rc, x1, y2 - 2 * h, x2, y2 - h);
            ::FillRect(hdc, &rc, hbr);
            y2 += h;
            break;
          }
        }
        ::SetRect(&rc, x1, y2 - h, x2, y2);
        ::FillRect(hdc, &rc, hbr);
      });

      decoration_horizontal_t dec_ol([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        coord_t h = (coord_t) std::ceil(ypixel * 0.05);
        switch (style & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_lower_half: return;
        case attribute_t::decdhl_upper_half:
          h *= 2;
          return;
        }
        HBRUSH const hbr = brush.get_brush(color0);
        RECT rc;
        ::SetRect(&rc, x1, y, x2, y + h);
        ::FillRect(hdc, &rc, hbr);
        if ((style & ~attribute_t::decdhl_mask) == 2) {
          ::SetRect(&rc, x1, y + 2 * h, x2, y + 3 * h);
          ::FillRect(hdc, &rc, hbr);
        }
      });

      auto _draw_frame = [&] (coord_t x1, coord_t x2, xflags_t xflags, int lline, int rline) {
        // 上の線と下の線は dec_ul, dec_ol に任せる。
        coord_t w = (coord_t) std::ceil(ypixel * 0.05);
        if (xflags & attribute_t::decdhl_mask) w *= 2;
        HBRUSH const hbr = brush.get_brush(color0);
        RECT rc;
        if (lline) {
          ::SetRect(&rc, x1, y, x1 + w, y + ypixel);
          ::FillRect(hdc, &rc, hbr);
          if (lline > 1) {
            ::SetRect(&rc, x1 + 2 * w, y, x1 + 3 * w, y + ypixel);
            ::FillRect(hdc, &rc, hbr);
          }
        }
        if (rline) {
          ::SetRect(&rc, x2 - w, y, x2, y + ypixel);
          ::FillRect(hdc, &rc, hbr);
          if (rline > 1) {
            ::SetRect(&rc, x2 - 3 * w, y, x2 - 2 * w, y + ypixel);
            ::FillRect(hdc, &rc, hbr);
          }
        }
      };

      auto _draw_circle = [&] (coord_t x1, coord_t x2, xflags_t xflags) {
        coord_t w = (coord_t) std::ceil(ypixel * 0.04);
        coord_t y1 = y, y2 = y + ypixel;
        bool is_clipped = false;
        switch (xflags & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_upper_half:
          is_clipped = true;
          ::SelectClipRgn(hdc, region.get_upper_region());
          w *= 2;
          y2 = y + 2 * ypixel;
          break;
        case attribute_t::decdhl_lower_half:
          is_clipped = true;
          ::SelectClipRgn(hdc, region.get_lower_region());
          w *= 2;
          y1 = y - ypixel;
          y2 = y + ypixel;
          break;
        }
        ::SelectObject(hdc, brush.get_pen(color0, w));
        ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
        ::Ellipse(hdc, x1, y1, x2, y2);
        ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
        if (is_clipped)
          ::SelectClipRgn(hdc, NULL);
      };

      auto _draw_stress = [&] (coord_t x1, coord_t x2, xflags_t xflags) {
        double w1 = ypixel * 0.15, h1 = ypixel * 0.15;
        switch (xflags & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_lower_half: return;
        case attribute_t::decdhl_upper_half:
          w1 *= 2.0;
          h1 *= 2.0;
          break;
        case attribute_t::decdhl_double_width:
          w1 *= 2.0;
          break;
        }
        coord_t const w = std::max<coord_t>(4, std::ceil(w1));
        coord_t const h = std::max<coord_t>(4, std::ceil(h1));
        coord_t const xL = (x1 + x2 - w - 1) / 2;
        coord_t const xR = xL + w;
        coord_t const yT = y - h;
        coord_t const yB = yT + h;
        ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
        ::SelectObject(hdc, brush.get_brush(color0));
        ::Ellipse(hdc, xL, yT, xR + 1, yB + 1);
        ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
      };

      for (curpos_t iline = 0; iline < b.m_height; iline++, y += ypixel) {
        std::vector<cell_t>& cells = content[iline];
        x = xorigin;
        color0 = 0;
        region.next_line(y, ypixel);

        for (std::size_t i = 0; i < cells.size(); ) {
          auto const& cell = cells[i++];
          auto const& code = cell.character.value;
          auto const& aflags = cell.attribute.aflags;
          auto const& xflags = cell.attribute.xflags;
          if (cell.width == 0) continue;
          coord_t const cell_width = cell.width * xpixel;
          color_t color = 0;
          if (code != ascii_nul && !(aflags & attribute_t::is_invisible_set))
            color = _color.resolve_fg(cell.attribute);
          if (color != color0) {
            dec_ul.update(x, 0, (xflags_t) 0);
            dec_sl.update(x, 0, (xflags_t) 0);
            dec_ol.update(x, 0, (xflags_t) 0);
          }

          if (color) {
            xflags_t const ideo = xflags & attribute_t::is_ideogram_mask & ~attribute_t::is_ideogram_line_left;

            // 下線
            bool const ul2 = aflags & attribute_t::is_double_underline_set ||
              ideo == attribute_t::is_ideogram_line_double_rb;
            bool const ul1 = aflags & attribute_t::is_underline_set ||
              xflags & attribute_t::is_frame_set ||
              ideo == attribute_t::is_ideogram_line_single_rb;
            dec_ul.update(x, ul2 ? 2 : ul1 ? 1 : 0, xflags);

            // 上線
            bool const ol2 = ideo == attribute_t::is_ideogram_line_double_rt;
            bool const ol1 = xflags & (attribute_t::is_overline_set | attribute_t::is_frame_set) ||
              ideo == attribute_t::is_ideogram_line_single_rt;
            dec_ol.update(x, ol2 ? 2 : ol1 ? 1 : 0, xflags);

            // 打ち消し線
            bool const sl2 = xflags & attribute_t::rlogin_double_strike;
            bool const sl1 = aflags & attribute_t::is_strike_set;
            dec_sl.update(x, sl2 ? 2 : sl1 ? 1 : 0, xflags);

            // 左線・右線
            bool const ll2 = xflags & attribute_t::rlogin_double_lline;
            bool const ll1 = xflags & (attribute_t::is_frame_set | attribute_t::rlogin_single_lline);
            int const ll = ll2 ? 2 : ll1 ? 1 : 0;
            bool const rl2 = xflags & attribute_t::rlogin_double_rline;
            bool const rl1 = xflags & (attribute_t::is_frame_set | attribute_t::rlogin_single_rline);
            int const rl = rl2 ? 2 : rl1 ? 1 : 0;
            _draw_frame(x, x + cell_width, xflags, ll, rl);

            // 丸
            if (xflags & attribute_t::is_circle_set)
              _draw_circle(x, x + cell_width, xflags);

            // 圏点
            if (ideo == attribute_t::is_ideogram_stress)
              _draw_stress(x, x + cell_width, xflags);
          }

          color0 = color;
          x += cell_width;
        }
        dec_ul.update(x, 0, (xflags_t) 0);
        dec_sl.update(x, 0, (xflags_t) 0);
        dec_ol.update(x, 0, (xflags_t) 0);
      }
    }

    void draw_characters_mono(HDC hdc, terminal_application const& app, std::vector<std::vector<contra::ansi::cell_t>> const& content) {
      using namespace contra::ansi;
      coord_t const xorigin = settings.m_xframe;
      coord_t const yorigin = settings.m_yframe;
      coord_t const ypixel = settings.m_ypixel;
      coord_t const xpixel = settings.m_xpixel;
      board_t const& b = app.board();

      std::vector<cell_t> cells;
      std::vector<TCHAR> characters;
      std::vector<INT> progress;
      for (curpos_t y = 0; y < b.m_height; y++) {
        std::vector<cell_t> const& cells = content[y];

        coord_t xoffset = xorigin;
        coord_t yoffset = yorigin + y * ypixel;
        characters.clear();
        progress.clear();
        characters.reserve(cells.size());
        progress.reserve(cells.size());
        for (auto const& cell : cells) {
          std::uint32_t code = cell.character.value;
          if (code & character_t::flag_cluster_extension)
            code &= ~character_t::flag_cluster_extension;
          if (code == ascii_nul || code == ascii_sp) {
            if (progress.size())
              progress.back() += cell.width * xpixel;
            else
              xoffset += cell.width * xpixel;
          } else if (code == (code & character_t::unicode_mask)) {
            characters.push_back(code);
            progress.push_back(cell.width * xpixel);
          }
          // ToDo: その他の文字・マーカーに応じた処理。
        }
        if (characters.size())
          ExtTextOut(hdc, xoffset, yoffset, 0, NULL, &characters[0], characters.size(), &progress[0]);
      }
    }

    // Note: background buffer として 3 枚使う。
    //   hdc0 ... 背景+内容+カーソル
    //   hdc1 ... 背景+内容
    //   hdc2 ... 背景 (未実装)
    void paint_terminal_content(HDC hdc0, terminal_application const& app, bool full_update) {
      bool const content_changed = m_tracer.is_content_changed(*this, app);
      bool const cursor_changed = m_tracer.is_cursor_changed(*this, app);

      if (m_tracer.is_metric_changed(*this)) full_update = true;
      HDC const hdc1 = m_background.hdc(hWnd, hdc0, 1);
      bool content_redraw = full_update || content_changed;
      if (m_tracer.is_blinking_changed(*this)) content_redraw = true;
      if (content_redraw) {
        std::vector<std::vector<contra::ansi::cell_t>> content;
        auto const& b = app.board();
        content.resize(b.m_height);

        for (contra::ansi::curpos_t y = 0; y < b.m_height; y++) {
          auto const& line = b.m_lines[y];
          line.get_cells_in_presentation(content[y], b.line_r2l(line));
        }

        ::SetBkMode(hdc1, TRANSPARENT);
        //this->draw_characters_mono(hdc1, app, content);
        this->draw_background(hdc1, app, content);
        this->draw_characters(hdc1, app, content);
        this->draw_decoration(hdc1, app, content);

        // ToDo: 更新のあった部分だけ転送する?
        // 更新のあった部分 = 内用の変更・点滅の変更・選択範囲の変更など
        ::BitBlt(hdc0, 0, 0, m_background.width(), m_background.height(), hdc1, 0, 0, SRCCOPY);
      }

      tstate_t const& s = app.state();
      bool const cursor_blinking = s.is_cursor_blinking();
      bool const cursor_visible = this->is_cursor_visible(app);
      if (!cursor_visible || !cursor_blinking)
        this->unset_cursor_timer();
      else if (content_changed || cursor_changed)
        this->reset_cursor_timer();

      if (!full_update) {
        // 前回のカーソル位置のセルをカーソルなしに戻す。
        coord_t const old_x1 = settings.m_xframe + settings.m_xpixel * m_tracer.cur_x();
        coord_t const old_y1 = settings.m_yframe + settings.m_ypixel * m_tracer.cur_y();
        ::BitBlt(hdc0, old_x1, old_y1, settings.m_xpixel, settings.m_ypixel, hdc1, old_x1, old_y1, SRCCOPY);
      }
      if (cursor_visible && (!cursor_blinking || !(m_cursor_timer_count & 1)))
        this->draw_cursor(hdc0, app);

      // ToDo: 二重チェックになっている気がする。もっと効率的な実装?
      m_tracer.store(*this, app);
    }

    void render_window() {
      if (!is_session_ready()) return;
      HDC hdc = GetDC(hWnd);
      {
        bool resized = false;
        HDC const hdc0 = m_background.hdc(hWnd, hdc, 0, &resized);
        paint_terminal_content(hdc0, manager.app(), resized);
        BitBlt(hdc, 0, 0, m_background.width(), m_background.height(), hdc0, 0, 0, SRCCOPY);
      }
      ReleaseDC(hWnd, hdc);
      ::ValidateRect(hWnd, NULL);
    }
    // WM_PAINT による再描画要求
    void redraw_window(HDC hdc, RECT const& rc) {
      bool resized = false;
      HDC const hdc0 = m_background.hdc(hWnd, hdc, 0, &resized);
      if (resized) {
        // 全体を再描画して全体を転送
        paint_terminal_content(hdc0, manager.app(), resized);
        ::SelectClipRgn(hdc, NULL);
        BitBlt(hdc, 0, 0, m_background.width(), m_background.height(), hdc0, 0, 0, SRCCOPY);
      } else {
        // 必要な領域だけ転送
        BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hdc0, rc.left, rc.top, SRCCOPY);
      }
    }


    enum extended_key_flags {
      modifier_application = 0x04000000,
      toggle_capslock   = 0x100,
      toggle_numlock    = 0x200,
      toggle_scrolllock = 0x400,
    };

    void process_input(std::uint32_t key) {
      //contra::term::print_key(key, stderr);
      manager.input_key(key);
    }

    std::uint32_t get_modifiers() {
      std::uint32_t ret = 0;
      if (GetKeyState(VK_LSHIFT) & 0x8000) ret |= modifier_shift;
      if (GetKeyState(VK_LCONTROL) & 0x8000) ret |= modifier_control;
      if (GetKeyState(VK_LMENU) & 0x8000) ret |= modifier_meta;
      if (GetKeyState(VK_RMENU) & 0x8000) ret |= modifier_alter;
      if (GetKeyState(VK_RCONTROL) & 0x8000) ret |= modifier_super;
      if (GetKeyState(VK_RSHIFT) & 0x8000) ret |= modifier_hyper;
      if (GetKeyState(VK_APPS) & 0x8000) ret |= modifier_application;

      if (GetKeyState(VK_CAPITAL) & 1) ret |= toggle_capslock;
      if (GetKeyState(VK_NUMLOCK) & 1) ret |= toggle_numlock;
      if (GetKeyState(VK_SCROLL) & 1) ret |= toggle_scrolllock;
      return ret;
    }

    static bool is_keypad_shifted(std::uint32_t modifiers) {
      if (modifiers & modifier_shift) {
        for (UINT k = VK_NUMPAD0; k <= VK_NUMPAD9; k++)
          if (GetKeyState(k) & 0x8000) return true;
        if (GetKeyState(VK_DECIMAL) & 0x8000) return true;
      }
      return false;
    }

    bool process_key(WPARAM wParam, std::uint32_t modifiers) {
      static BYTE kbstate[256];

      if (ascii_A <= wParam && wParam <= ascii_Z) {
        if (modifiers & ~modifier_shift & _modifier_mask) {
          wParam += ascii_a - ascii_A;
        } else {
          bool const capslock = modifiers & toggle_capslock;
          bool const shifted = modifiers & modifier_shift;
          if (capslock == shifted) wParam += ascii_a - ascii_A;
          modifiers &= ~modifier_shift;
        }
        process_input(wParam | (modifiers & _modifier_mask));
      } else if (VK_F1 <= wParam && wParam < VK_F24) {
        process_input((wParam -VK_F1 + key_f1) | (modifiers & _modifier_mask));
      } else {
        std::uint32_t code = 0;
        switch (wParam) {
        case VK_BACK:      code = ascii_bs;   goto function_key;
        case VK_TAB:       code = ascii_ht;   goto function_key;
        case VK_RETURN:    code = ascii_cr;   goto function_key;
        case VK_ESCAPE:    code = ascii_esc;  goto function_key;
        case VK_DELETE:    code = key_delete; goto keypad_function_key;
        case VK_INSERT:    code = key_insert; goto keypad_function_key;
        case VK_PRIOR:     code = key_prior;  goto keypad_function_key;
        case VK_NEXT:      code = key_next;   goto keypad_function_key;
        case VK_END:       code = key_end;    goto keypad_function_key;
        case VK_HOME:      code = key_home;   goto keypad_function_key;
        case VK_LEFT:      code = key_left;   goto keypad_function_key;
        case VK_UP:        code = key_up;     goto keypad_function_key;
        case VK_RIGHT:     code = key_right;  goto keypad_function_key;
        case VK_DOWN:      code = key_down;   goto keypad_function_key;
        case VK_CLEAR:     code = key_begin;  goto keypad_function_key;
        keypad_function_key:
          if ((modifiers & toggle_numlock) && is_keypad_shifted(modifiers))
            modifiers &= ~modifier_shift;
          goto function_key;
        case VK_NUMPAD0:   code = key_kp0;   goto keypad_number_key;
        case VK_NUMPAD1:   code = key_kp1;   goto keypad_number_key;
        case VK_NUMPAD2:   code = key_kp2;   goto keypad_number_key;
        case VK_NUMPAD3:   code = key_kp3;   goto keypad_number_key;
        case VK_NUMPAD4:   code = key_kp4;   goto keypad_number_key;
        case VK_NUMPAD5:   code = key_kp5;   goto keypad_number_key;
        case VK_NUMPAD6:   code = key_kp6;   goto keypad_number_key;
        case VK_NUMPAD7:   code = key_kp7;   goto keypad_number_key;
        case VK_NUMPAD8:   code = key_kp8;   goto keypad_number_key;
        case VK_NUMPAD9:   code = key_kp9;   goto keypad_number_key;
        case VK_DECIMAL:   code = key_kpdec; goto keypad_number_key;
        keypad_number_key:
          modifiers &= ~modifier_shift;
          goto function_key;
        case VK_MULTIPLY:  code = key_kpmul; goto function_key;
        case VK_ADD:       code = key_kpadd; goto function_key;
        case VK_SEPARATOR: code = key_kpsep; goto function_key;
        case VK_SUBTRACT:  code = key_kpsub; goto function_key;
        case VK_DIVIDE:    code = key_kpdiv; goto function_key;
        function_key:
          process_input(code | (modifiers & _modifier_mask));
          break;
        case VK_PROCESSKEY: // IME 処理中 (無視)
          return false;
        default:
          {
            kbstate[VK_SHIFT] = modifiers & modifier_shift ? 0x80 : 0x00;
            WCHAR buff = 0;
            int const count = ::ToUnicode(wParam, ::MapVirtualKey(wParam, MAPVK_VK_TO_VSC), kbstate, &buff, 1, 0);
            if (count == 1 || count == -1) {
              // Note: dead-char (count == 2) の場合には今の所は対応しない事にする。

              // shift によって文字が変化していたら shift 修飾を消す。
              UINT const unshifted = ::MapVirtualKey(wParam, MAPVK_VK_TO_CHAR);
              int const unshifted_count = (INT) unshifted < 0 ? 2 : 1;
              if (count != unshifted_count || buff != (unshifted & _character_mask))
                modifiers &= ~modifier_shift;

              process_input((buff & _character_mask) | (modifiers & _modifier_mask));
            } else if (UINT const code = ::MapVirtualKey(wParam, MAPVK_VK_TO_CHAR); (INT) code > 0) {
              process_input((code & _character_mask) | (modifiers & _modifier_mask));
            } else {
              if (settings.m_debug_print_unknown_key)
                std::fprintf(stderr, "key (unknown): wparam=%08x flags=%x\n", wParam, modifiers);
              return false;
            }
          }
          break;
        }
      }
      return true;
    }

    void process_char(WPARAM wParam, std::uint32_t modifiers) {
      // ToDo: Process surrogate pair
      process_input((wParam & _character_mask) | (modifiers & _modifier_mask));
    }

    void process_mouse(key_t key, std::uint32_t modifiers, WORD x, WORD y) {
      curpos_t const x1 = (x - settings.m_xframe) / settings.m_xpixel;
      curpos_t const y1 = (y - settings.m_yframe) / settings.m_ypixel;
      manager.input_mouse(key | (modifiers & _modifier_mask), x, y, x1, y1);
      if (manager.m_dirty) render_window();
    }

  private:
    bool m_ime_composition_active = false;
    void start_ime_composition() {
      this->m_ime_composition_active = true;
      this->render_window();
      this->adjust_ime_composition();
    }
    void adjust_ime_composition() {
      if (!is_session_ready() || !m_ime_composition_active) return;
      util::raii hIMC(::ImmGetContext(hWnd), [this] (auto hIMC) { ::ImmReleaseContext(hWnd, hIMC); });

      font_t const current_font = font_resolver_t().resolve_font(manager.app().board().cur.attribute) & ~font_rotation_mask;
      auto const [dx, dy, dxW] = fstore.get_displacement(current_font);
      contra_unused(dx);
      contra_unused(dxW);
      LOGFONT logfont = fstore.create_logfont(current_font);
      logfont.lfWidth = fstore.width() * (current_font & font_decdwl ? 2 : 1);
      ::ImmSetCompositionFont(hIMC, const_cast<LOGFONT*>(&logfont));


      RECT rcClient;
      if (::GetClientRect(hWnd, &rcClient)) {
        auto const& b = manager.app().board();
        COMPOSITIONFORM form;
        coord_t const x0 = rcClient.left + settings.m_xframe + settings.m_xpixel * b.cur.x();
        coord_t const y0 = rcClient.top + settings.m_yframe + settings.m_ypixel * b.cur.y();
        form.dwStyle = CFS_POINT;
        form.ptCurrentPos.x = x0;
        form.ptCurrentPos.y = y0 + dy;
        ::ImmSetCompositionWindow(hIMC, &form);
      }
    }
    void cancel_ime_composition() {
      if (!is_session_ready() || !m_ime_composition_active) return;
      util::raii hIMC(::ImmGetContext(hWnd), [this] (auto hIMC) { ::ImmReleaseContext(hWnd, hIMC); });
      ::ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
      this->end_ime_composition();
    }
    void end_ime_composition() {
      if (!m_ime_composition_active) return;
      m_ime_composition_active = false;
      this->render_window();
    }

  public:
    LRESULT process_message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
      key_t key = 0;
      switch (msg) {
      case WM_CREATE:
        this->adjust_window_size_using_current_client_size();
        goto defproc;
      case WM_SIZE:
        if (hWnd != this->hWnd) goto defproc;
        if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
          this->adjust_terminal_size_using_current_client_size();
        goto defproc;
      case WM_DESTROY:
        ::PostQuitMessage(0);
        this->hWnd = NULL;
        return 0L;
      case WM_PAINT:
        if (is_session_ready()) {
          if (hWnd != this->hWnd) goto defproc;
          PAINTSTRUCT paint;
          HDC hdc = BeginPaint(hWnd, &paint);
          redraw_window(hdc, paint.rcPaint);
          EndPaint(hWnd, &paint);
        }
        return 0L;

      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
        if (hWnd != this->hWnd) goto defproc;
        if (process_key(wParam, get_modifiers())) return 0L;
        goto defproc;
      case WM_IME_CHAR:
        if (hWnd != this->hWnd) goto defproc;
        process_char(wParam, get_modifiers());
        return 0L;
      case WM_CHAR:
      case WM_DEADCHAR:
      case WM_KEYUP:
      case WM_SYSCHAR:
      case WM_SYSDEADCHAR:
      case WM_SYSKEYUP:
        // 余分な操作をしない様に無視
        // DEADCHAR は accent や diacritic などの入力の時に発生する。
        return 0L;

      case WM_LBUTTONDOWN: key = key_mouse1_down; goto mouse_event;
      case WM_MBUTTONDOWN: key = key_mouse2_down; goto mouse_event;
      case WM_RBUTTONDOWN: key = key_mouse3_down; goto mouse_event;
      case WM_XBUTTONDOWN:
        key = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? key_mouse4_down : key_mouse5_down;
        goto mouse_event;
      case WM_LBUTTONUP: key = key_mouse1_up; goto mouse_event;
      case WM_MBUTTONUP: key = key_mouse2_up; goto mouse_event;
      case WM_RBUTTONUP: key = key_mouse3_up; goto mouse_event;
      case WM_XBUTTONUP:
        key = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? key_mouse4_up : key_mouse5_up;
        goto mouse_event;
      case WM_MOUSEMOVE:
        ::SetCursor(::LoadCursor(NULL, IDC_IBEAM));
        {
          constexpr WORD mouse_buttons = MK_LBUTTON | MK_MBUTTON | MK_RBUTTON | MK_XBUTTON1 | MK_XBUTTON2;
          key = key_mouse_move;
          WORD const kstate = GET_KEYSTATE_WPARAM(wParam);
          if (kstate & mouse_buttons) {
            if (kstate & MK_LBUTTON)
              key = key_mouse1_drag;
            else if (kstate & MK_RBUTTON)
              key = key_mouse3_drag;
            else if (kstate & MK_MBUTTON)
              key = key_mouse2_drag;
            else if (kstate & MK_XBUTTON1)
              key = key_mouse4_drag;
            else if (kstate & MK_XBUTTON2)
              key = key_mouse5_drag;
          }
          goto mouse_event;
        }
      case WM_MOUSEWHEEL:
        key = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? key_wheel_up : key_wheel_down;
        goto mouse_event;
      mouse_event:
        process_mouse(key, get_modifiers(), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0L;

      case WM_IME_STARTCOMPOSITION:
        {
          LRESULT const result = ::DefWindowProc(hWnd, msg, wParam, lParam);
          this->start_ime_composition();
          return result;
        }
      case WM_IME_ENDCOMPOSITION:
        this->end_ime_composition();
        goto defproc;
      case WM_SETFOCUS:
        if (is_session_ready())
          this->process_input(key_focus);
        goto defproc;
      case WM_KILLFOCUS:
        this->cancel_ime_composition();
        this->process_input(key_blur);
        goto defproc;

      case WM_TIMER:
        if (m_cursor_timer_id && wParam == m_cursor_timer_id)
          process_cursor_timer();
        else if (m_blinking_timer_id && wParam == m_blinking_timer_id)
          process_blinking_timer();
        goto defproc;

      default:
        if (settings.m_debug_print_window_messages) {
          const char* name = get_window_message_name(msg);
          if (name)
            std::fprintf(stderr, "message: %s\n", name);
          else
            std::fprintf(stderr, "message: %08x\n", msg);
        }
      defproc:
        return ::DefWindowProc(hWnd, msg, wParam, lParam);
      }
    }

  private:
    static void exec_error_handler(int errno1, std::uintptr_t) {
      char const* msg = std::strerror(errno1);
      std::ostringstream buff;
      buff << "exec: " << msg << " (errno=" << errno1 << ")";
      ::MessageBoxA(NULL, buff.str().c_str(), "Contra/Cygwin - exec failed", MB_OK);
    }
    bool setup_session() {
      terminal_session_parameters params;
      params.col = settings.m_col;
      params.row = settings.m_row;
      params.xpixel = settings.m_xpixel;
      params.ypixel = settings.m_ypixel;
      params.exec_error_handler = &exec_error_handler;
      params.env_term = settings.m_env_term;
      std::unique_ptr<terminal_application> sess = contra::term::create_terminal_session(params);
      if (!sess) return false;

      contra::ansi::tstate_t& s = sess->state();
      s.m_default_fg_space = contra::dict::attribute_t::color_space_rgb;
      s.m_default_bg_space = contra::dict::attribute_t::color_space_rgb;
      s.m_default_fg_color = contra::dict::rgb(0x00, 0x00, 0x00);
      s.m_default_bg_color = contra::dict::rgb(0xFF, 0xFF, 0xFF);
      // s.m_default_fg_color = contra::dict::rgb(0xD0, 0xD0, 0xD0);//@color
      // s.m_default_bg_color = contra::dict::rgb(0x00, 0x00, 0x00);
      manager.add_app(std::move(sess));
      return true;
    }
  public:
    int m_exit_code = 0;
    int do_loop() {
      if (!this->setup_session()) return 2;

      MSG msg;
      while (hWnd) {
        bool processed = manager.do_events();
        if (manager.m_dirty) {
          render_window();
          manager.m_dirty = false;
          if (m_ime_composition_active)
            this->adjust_ime_composition();
        }

        while (::PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE)) {
          processed = true;
          if (!GetMessage(&msg, 0, 0, 0)) {
            m_exit_code = msg.wParam;
            goto exit;
          }

          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }

        if (!manager.is_alive()) break;
        if (!processed)
          contra::term::msleep(10);
      }
    exit:
      manager.terminate();
      return 0;
    }
  };

  twin_window_t main_window;

  LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return main_window.process_message(hWnd, msg, wParam, lParam);
  }
}
}

// from http://www.kumei.ne.jp/c_lang/index_sdk.html
extern "C" int WINAPI _tWinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPreInst, [[maybe_unused]] LPTSTR lpszCmdLine, int nCmdShow) {
  auto& win = contra::twin::main_window;
  HWND const hWnd = win.create_window(hInstance);
  ::ShowWindow(hWnd, nCmdShow);
  ::UpdateWindow(hWnd);
  return win.do_loop();
}

// from https://cat-in-136.github.io/2012/04/unicodemingw32twinmainwwinmain.html
#if defined(_UNICODE) && !defined(_tWinMain)
int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
#ifdef contra_dbgout
  dbgout = std::fopen("/home/murase/twin-debug.txt", "w");
#endif
  ::ShowWindow(GetConsoleWindow(), SW_HIDE);
  HINSTANCE const hInstance = GetModuleHandle(NULL);
  int const retval = _tWinMain(hInstance, NULL, (LPTSTR) TEXT("") /* lpCmdLine is not available*/, SW_SHOW);
  return retval;
}
#endif
