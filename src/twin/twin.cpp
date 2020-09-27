#define __STDC_FORMAT_MACROS
#define CONTRA_TWIN_SUPPORT_HIGHDPI

#ifdef CONTRA_TWIN_SUPPORT_HIGHDPI
# define _WIN32_WINNT_WIN6 0x0600
# define WINVER        _WIN32_WINNT_WIN6
# define _WIN32_WINNT  _WIN32_WINNT_WIN6
#endif

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
#include <cinttypes>
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
#include "../ansi/render.hpp"
#include "win_messages.hpp"
#include "../context.hpp"

#define _tcslen wcslen
#define _tcscpy_s wcscpy_s
#define _tcscpy wcscpy

namespace contra::twin {
namespace {

  template<typename XCH>
  int xcscpy_s(XCH* dst, std::size_t sz, const XCH* src) {
    if (!sz--) return 1;
    char c;
    while (sz-- && (c = *src++)) *dst++ = c;
    *dst = '\0';
    return 0;
  }

  static std::u16string get_error_message(DWORD error_code) {
    LPTSTR result = nullptr;
    ::FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error_code, LANG_USER_DEFAULT, (LPTSTR) &result, 0, NULL);
    std::u16string ret;
    if (result) {
      static_assert(sizeof(wchar_t) == sizeof(char16_t)); // Windows
      ret = reinterpret_cast<char16_t*>(result);
      ::LocalFree(result);
    }
    return ret;
  }

  static void report_error_message(const char* title, DWORD error_code) {
    std::fprintf(stderr, "%s: (Error: %" PRIu16 ") ", title, (std::uint16_t) error_code);
    {
      auto message = get_error_message(error_code);
      std::vector<char32_t> buffer;
      std::uint64_t state = 0;
      contra::encoding::utf16_decode(&message[0], &message[message.size()], buffer, state);
      for (char32_t u : buffer) contra::encoding::put_u8(u, stderr);
    }
    std::putc('\n', stderr);
  }

  using namespace contra::term;
  using namespace contra::ansi;

  struct twin_settings: public ansi::window_settings_base {
    coord_t window_size_xadjust = 0;
    coord_t window_size_yadjust = 0;

    bool m_caret_hide_on_ime = true;
    UINT m_caret_interval = 400;
    UINT m_blinking_interval = 200;

    bool twin_disableMouseReportOnScrLock = true;

    int m_debug_print_window_messages = 0; // 0: none, 1: unprocessed, 2: all
    bool m_debug_print_unknown_key = false;


  public:
    coord_t calculate_window_width(window_state_t const& wstat) const {
      return wstat.calculate_client_width() + window_size_xadjust;
    }
    coord_t calculate_window_height(window_state_t const& wstat) const {
      return wstat.calculate_client_height() + window_size_yadjust;
    }

    void configure(contra::app::context& actx) {
      window_settings_base::configure(actx);
      actx.read("twin_disable_mouse_report_on_scrlock", twin_disableMouseReportOnScrLock = true);
    }
  };

  class win_font_factory {
    static_assert(sizeof(TCHAR) == 2 && sizeof(wchar_t) == 2);
    LOGFONT m_logfont_normal;

    // 縦横比がフォントによって異なる事があるので、その補正用
    struct average_metric {
      double widht, height;
    };

    struct font_info {
      std::u16string name;
      double width = 0.0;
      double height = 1.0;

    private:
      static int CALLBACK EnumFontFamExProc(LOGFONT const* lplf, TEXTMETRIC const* lptm, DWORD nFontType, LPARAM lParam) {
        contra_unused(lplf);
        if (nFontType == TRUETYPE_FONTTYPE) {
          auto const finfo = reinterpret_cast<font_info*>(lParam);
          auto const lpntm = reinterpret_cast<NEWTEXTMETRICEX const*>(lptm);
          finfo->width = (double) lpntm->ntmTm.ntmAvgWidth / lpntm->ntmTm.ntmSizeEM;
          finfo->height = (double) lpntm->ntmTm.ntmCellHeight / lpntm->ntmTm.ntmSizeEM;
          return 0;
        } else {
          return 1;
        }
      }

    public:
      void initialize_metric() {
        this->width = 1.0;
        this->height = 1.0;
        HDC hdc = ::GetDC(0);
        LOGFONT logfont = {};
        logfont.lfCharSet = DEFAULT_CHARSET;
        logfont.lfPitchAndFamily = 0;
        xcscpy_s(logfont.lfFaceName, LF_FACESIZE, (LPCTSTR) name.c_str());
        ::EnumFontFamiliesEx(hdc, &logfont, (FONTENUMPROC) &EnumFontFamExProc, reinterpret_cast<LPARAM>(this), 0);
        ::ReleaseDC(0, hdc);
      }
    } m_fontinfo[16];

    std::unordered_map<std::u16string, average_metric> m_width_factors;

  private:
    void initialize_fontnames(contra::app::context& actx) {
      m_fontinfo[0].name  = u"MeiryoKe_Console";
      m_fontinfo[1].name  = u"MS Gothic";
      m_fontinfo[2].name  = u"MS Mincho";
      m_fontinfo[3].name  = u"HGMaruGothicMPRO";
      m_fontinfo[4].name  = u"HGKyokashotai";
      m_fontinfo[5].name  = u"HGGyoshotai";
      m_fontinfo[6].name  = u"HGSeikaishotaiPRO";
      m_fontinfo[7].name  = u"HGSoeiKakupoptai";
      m_fontinfo[8].name  = u"HGGothicM";
      m_fontinfo[9].name  = u"Times New Roman";
      m_fontinfo[10].name = u"aghtex_mathfrak";
      actx.read("twin_font_default", m_fontinfo[0].name , &parse_fontname);
      actx.read("twin_font_ansi1"  , m_fontinfo[1].name , &parse_fontname);
      actx.read("twin_font_ansi2"  , m_fontinfo[2].name , &parse_fontname);
      actx.read("twin_font_ansi3"  , m_fontinfo[3].name , &parse_fontname);
      actx.read("twin_font_ansi4"  , m_fontinfo[4].name , &parse_fontname);
      actx.read("twin_font_ansi5"  , m_fontinfo[5].name , &parse_fontname);
      actx.read("twin_font_ansi6"  , m_fontinfo[6].name , &parse_fontname);
      actx.read("twin_font_ansi7"  , m_fontinfo[7].name , &parse_fontname);
      actx.read("twin_font_ansi8"  , m_fontinfo[8].name , &parse_fontname);
      actx.read("twin_font_ansi9"  , m_fontinfo[9].name , &parse_fontname);
      actx.read("twin_font_frak"   , m_fontinfo[10].name, &parse_fontname);
    }

    // Note: initialize_fontnames の後に呼び出す事
    void initialize_logfont() {
      m_logfont_normal.lfHeight = 14;
      m_logfont_normal.lfWidth  = 7;
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
      xcscpy_s(m_logfont_normal.lfFaceName, LF_FACESIZE, (LPCTSTR) m_fontinfo[0].name.c_str());
    }
  public:
    win_font_factory(contra::app::context& actx) {
      initialize_fontnames(actx);
      initialize_logfont(); // Note: initialize_fontnames の後に呼び出す事
    }

  public:
    LOGFONT create_logfont(font_t font, ansi::font_metric_t const& metric) {
      double widthFactor = 1.0;

      LOGFONT logfont = m_logfont_normal;
      logfont.lfHeight = metric.height();
      logfont.lfWidth  = metric.width();
      font_t const face = (font & font_face_mask) >> font_face_shft;
      if (auto& info = m_fontinfo[face]; !info.name.empty()) {
        xcscpy_s(logfont.lfFaceName, LF_FACESIZE, (LPCTSTR) info.name.c_str());
        if (info.width == 0.0) info.initialize_metric();
        widthFactor = 2.0 * info.width;
      }
      if (font & font_flag_italic)
        logfont.lfItalic = TRUE;

      switch (font & font_weight_mask) {
      case font_weight_bold: logfont.lfWeight = FW_BOLD; break;
      case font_weight_faint: logfont.lfWeight = FW_THIN; break;
      case font_weight_heavy: logfont.lfWeight = FW_HEAVY; break;
      }

      if (font & font_flag_small) {
        logfont.lfHeight = metric.small_height();
        logfont.lfWidth = metric.small_width();
      }
      if (font & font_decdhl) logfont.lfHeight *= 2;
      if (font & font_decdwl) logfont.lfWidth *= 2;
      if (font_t const rotation = (font & font_rotation_mask) >> font_rotation_shft) {
        logfont.lfOrientation = 450 * rotation;
        logfont.lfEscapement  = 450 * rotation;
      }

      logfont.lfWidth = (LONG) (logfont.lfWidth * widthFactor);
      return logfont; /* NRVO */
    }

  public:
    typedef HFONT font_type;

    font_type create_font(font_t font, ansi::font_metric_t const& metric) {
      LOGFONT const logfont = create_logfont(font, metric);
      return ::CreateFontIndirect(&logfont);
    }

    void delete_font(font_type hfont) const {
      ::DeleteObject(hfont);
    }

  private:
    static bool parse_fontname(std::u16string& str, const char* text) {
      std::size_t const len = std::strlen(text);
      std::vector<char32_t> buffer(len);
      char32_t* const p0 = &buffer[0];
      char32_t* const pN = p0 + len;
      char32_t* p = p0;
      {
        std::uint64_t state = 0;
        contra::encoding::utf8_decode(text, text + len, p, pN, state);
        if (state) return false;
      }

      std::u16string ret;
      std::uint64_t state = 0;
      contra::encoding::utf16_encode(p0, p, ret, state);
      str = ret;
      return true;
    }
  };

  class win_font_manager_t: public ansi::font_manager_t<win_font_factory> {
    typedef ansi::font_manager_t<win_font_factory> base;
  public:
    win_font_manager_t(contra::app::context& actx): base(actx) {}
    LOGFONT create_logfont(font_t font) {
      return factory().create_logfont(font, static_cast<ansi::font_metric_t const&>(*this));
    }
  };

  LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
        m_brush = ::CreateSolidBrush(contra::ansi::rgba2rgb(color));
      return m_brush;
    }
    HPEN get_pen(color_t color, int width) {
      if (m_color != color) release();
      m_color = color;
      if (!m_pen || m_pen_width != width) {
        if (m_pen) ::DeleteObject(m_pen);
        m_pen_width = width;
        m_pen = ::CreatePen(PS_SOLID, width, contra::ansi::rgba2rgb(color));
      }
      return m_pen;
    }
    ~brush_holder_t() {
      release();
    }
  };

  struct twin_graphics_t;

  class twin_graphics_buffer {
    coord_t m_width = -1, m_height = -1;

    struct layer_t {
      HBITMAP hbmp = NULL;
      HDC hctx = NULL;
    };
    std::vector<layer_t> m_layers;

    void release_layer(layer_t& layer) {
      if (layer.hctx) {
        ::DeleteDC(layer.hctx);
        layer.hctx = NULL;
      }
      if (layer.hbmp) {
        ::DeleteObject(layer.hbmp);
        layer.hbmp = NULL;
      }
    }
    void initialize_layer(layer_t& layer) {
      release_layer(layer);
      layer.hbmp = ::CreateCompatibleBitmap(m_hdc, m_width, m_height);
      layer.hctx = ::CreateCompatibleDC(m_hdc);
      ::SelectObject(layer.hctx, layer.hbmp);
      ::SetBkMode(layer.hctx, TRANSPARENT);
    }

  private:
    void release() {
      for (layer_t& layer : m_layers) release_layer(layer);
      m_layers.clear();
    }

  private:
    win_font_manager_t m_fstore;
    brush_holder_t m_bstore;
  public:
    win_font_manager_t& fstore() { return m_fstore; }
    brush_holder_t& bstore() { return m_bstore; }

  public:
    twin_graphics_buffer(contra::app::context& actx): m_fstore(actx) {}
    ~twin_graphics_buffer() {
      release();
    }

    coord_t width() const { return m_width; }
    coord_t height() const { return m_height; }
    void update_window_size(coord_t width, coord_t height, bool* out_resized = NULL) {
      bool const is_resized = width != m_width || height != m_height;
      if (is_resized) {
        release();
        m_width = width;
        m_height = height;
      }
      if (out_resized) *out_resized = is_resized;
    }

    HWND m_hWnd = NULL;
    HDC m_hdc = NULL;
    void setup(HWND hWnd, HDC hdc) {
      this->m_hWnd = hWnd;
      this->m_hdc = hdc;
    }

  public:
    typedef HDC context_t;

    context_t layer(std::size_t index) {
      if (index < m_layers.size() && m_layers[index].hctx)
        return m_layers[index].hctx;

      if (index >= m_layers.size()) m_layers.resize(index + 1);
      layer_t& layer = m_layers[index];
      initialize_layer(layer);
      return layer.hctx;
    }

    static void bitblt(
      context_t ctx1, coord_t x1, coord_t y1, coord_t w, coord_t h,
      context_t ctx2, coord_t x2, coord_t y2
    ) {
      ::BitBlt(ctx1, x1, y1, w, h, ctx2, x2, y2, SRCCOPY);
    }
    void render(coord_t x1, coord_t y1, coord_t w, coord_t h, context_t ctx2, coord_t x2, coord_t y2) {
      ::BitBlt(m_hdc, x1, y1, w, h, ctx2, x2, y2, SRCCOPY);
    }

    typedef twin_graphics_t graphics_t;
  };

  struct twin_graphics_t {
    HDC hdc;
    win_font_manager_t& fstore;
    brush_holder_t& bstore;
  public:
    twin_graphics_t(twin_graphics_buffer& target, HDC hdc): hdc(hdc), fstore(target.fstore()), bstore(target.bstore()) {}
    ~twin_graphics_t() {
      clip_release();
    }

  public:
    void fill_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color) const {
      RECT rc;
      ::SetRect(&rc, x1, y1, x2, y2);
      ::FillRect(hdc, &rc, bstore.get_brush(color));
    }
    void invert_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2) const {
      ::PatBlt(hdc, x1, y1, x2 - x1, y2 - y1, DSTINVERT);
    }
    void checked_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color) const {
      // CreatePatternBrush による実装を試みたが背景色を透明にできないので使えなかった
      int odd = (x1 + y1) & 1;
      ::SelectObject(hdc, bstore.get_pen(color, 1));
      ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
      for (coord_t x = x1 + odd; x < x2; x += 2) {
        coord_t const d = std::min(x2 - x, y2 - y1);
        ::MoveToEx(hdc, x, y1, NULL);
        ::LineTo(hdc, x + d, y1 + d);
      }
      for (coord_t y = y1 + 2 - odd; y < y2; y += 2) {
        coord_t const d = std::min(y2 - y, x2 - x1);
        ::MoveToEx(hdc, x1, y, NULL);
        ::LineTo(hdc, x1 + d, y + d);
      }
      ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
    }
    void draw_ellipse(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color, int line_width) {
      ::SelectObject(hdc, bstore.get_pen(color, line_width));
      ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
      ::Ellipse(hdc, x1, y1, x2, y2);
      ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
    }
    void fill_ellipse(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color) {
      ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
      ::SelectObject(hdc, bstore.get_brush(color));
      ::Ellipse(hdc, x1, y1, x2, y2);
      ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    }
    void fill_polygon(coord_t const (*points)[2], std::size_t count, color_t color) {
      ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
      ::SelectObject(hdc, bstore.get_brush(color));
      ::Polygon(hdc, reinterpret_cast<POINT const*>(points), count);
      ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    }
    void draw_line(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color, int line_width) {
      ::SelectObject(hdc, bstore.get_pen(color, line_width));
      ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
      ::MoveToEx(hdc, x1, y1, NULL);
      ::LineTo(hdc, x2, y2);
      ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
    }

  private:
    HRGN rgn = NULL;
    void clip_release() {
      if (rgn) {
        ::DeleteObject(rgn);
        rgn = NULL;
      }
    }
  public:
    void clip_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2) {
      if (rgn)
        SetRectRgn(rgn, x1, y1, x2, y2);
      else
        rgn = ::CreateRectRgn(x1, y1, x2, y2);
      ::SelectClipRgn(hdc, rgn);
    }
    void clip_clear() {
      ::SelectClipRgn(hdc, NULL);
    }

    struct character_buffer {
      std::vector<TCHAR> characters;
      std::vector<INT> position;

    public:
      bool empty() const { return position.empty(); }
      void add_char(std::uint32_t code, coord_t x, bool is_extension) {
        contra_unused(is_extension);
        if (code < 0x10000) {
          characters.push_back(code);
          position.push_back(x);
        } else {
          // surrogate pair
          code -= 0x10000;
          characters.push_back(0xD800 | (code >> 10 & 0x3FF));
          position.push_back(x);
          characters.push_back(0xDC00 | (code & 0x3FF));
          position.push_back(x);
        }
      }
      void reserve(std::size_t capacity) {
        characters.reserve(capacity);
        position.reserve(capacity);
      }
      void clear() {
        characters.clear();
        position.clear();
      }

    public:
      void resolve(coord_t& dx) {
        dx += position[0];
        for (std::size_t i = 0, iN = position.size() - 1; i < iN; i++)
          position[i] = position[i + 1] - position[i];
      }
    };

    void draw_text(coord_t x1, coord_t y1, character_buffer& buff, font_t font, color_t color) {
      if (buff.empty()) return;
      ::SetTextColor(hdc, contra::ansi::rgba2rgb(color));
      ::SelectObject(hdc, fstore.get_font(font));
      ::TextOut(hdc, x1 + buff.position[0], y1, &buff.characters[0], buff.characters.size());
    }
    void draw_characters(coord_t x1, coord_t y1, character_buffer& buff, font_t font, color_t color) {
      if (buff.empty()) return;
      ::SetTextColor(hdc, contra::ansi::rgba2rgb(color));
      ::SelectObject(hdc, fstore.get_font(font));
      buff.resolve(x1);
      ::ExtTextOut(hdc, x1, y1, 0, NULL, &buff.characters[0], buff.characters.size(), &buff.position[0]);
    }
    void draw_rotated_characters(
      coord_t x0, coord_t y0, coord_t dx, coord_t dy, coord_t width,
      character_buffer& buff, font_t font, color_t color
    ) {
      if (buff.empty()) return;
      buff.resolve(dx);

      ::SetTextColor(hdc, contra::ansi::rgba2rgb(color));
      ::SelectObject(hdc, fstore.get_font(font));

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
      ::ExtTextOut(hdc, x0 + dx - rot_dx, y0 + dy - rot_dy, 0, NULL,
        &buff.characters[0], buff.characters.size(), &buff.position[0]);
    }
  };

  class twin_window_t {
    static constexpr LPCTSTR szClassName = TEXT("Contra.Twin.Main");

    contra::app::context& actx;

    window_state_t wstat;
    terminal_manager manager;

    twin_settings settings;
    twin_graphics_buffer gbuffer;
    window_renderer_t<twin_graphics_t> renderer { wstat };

    HWND hWnd = NULL;

  public:
    twin_window_t(contra::app::context& actx): actx(actx), gbuffer(actx) {
#ifdef CONTRA_TWIN_SUPPORT_HIGHDPI
      HDC hdc = ::GetWindowDC(NULL);
      double const dpiscale = std::max(1.0, ::GetDeviceCaps(hdc, LOGPIXELSX) / 96.0);
      ::ReleaseDC(NULL, hdc);
#else
      double const dpiscale = 1.0;
#endif

      std::fill(std::begin(m_kbflags), std::end(m_kbflags), 0);

      // size and dimension
      wstat.configure_metric(actx, dpiscale);
      manager.initialize_zoom(wstat.m_xunit, wstat.m_yunit);
      manager.reset_size(wstat.m_col, wstat.m_row, wstat.m_xunit, wstat.m_yunit);

      // other settings
      settings.configure(actx);
    }

  private:
    class twin_events: public contra::term::terminal_events {
      twin_window_t* win;
    public:
      twin_events(twin_window_t* win): win(win) {}
    private:
      static bool read_clipboard(std::u16string& buffer) {
        bool result = false;
        if (!::OpenClipboard(NULL)) {
          report_error_message("twin.clipboard", ::GetLastError());
          return false;
        }

        HANDLE clip;
        LPVOID data;

        clip = ::GetClipboardData(CF_UNICODETEXT);
        if (!clip) {
          report_error_message("twin.clipboard", ::GetLastError());
          goto close_clipboard;
        }

        data = ::GlobalLock(clip);
        if (data == NULL) {
          report_error_message("twin.clipboard", ::GetLastError());
          goto close_clipboard;
        }

        static_assert(sizeof(TCHAR) == sizeof(char16_t));
        buffer = reinterpret_cast<char16_t const*>(data);
        result = true;
        ::GlobalUnlock(clip);
      close_clipboard:
        ::CloseClipboard();
        return result;
      }

      static bool write_clipboard(std::u16string const& buffer, HWND hWnd) {
        std::size_t const sz = buffer.size();
        HGLOBAL const hg = ::GlobalAlloc(GHND | GMEM_SHARE , (sz + 1) * sizeof(char16_t));
        if (!hg) {
          report_error_message("twin.clipboard", ::GetLastError());
          return false;
        }

        LPVOID mem = ::GlobalLock(hg);
        if (!mem) {
          report_error_message("twin.clipboard", ::GetLastError());
          goto global_free;
        }

        static_assert(sizeof(TCHAR) == sizeof(char16_t));
        std::copy(buffer.c_str(), buffer.c_str() + sz + 1, reinterpret_cast<char16_t*>(mem));
        ::GlobalUnlock(hg);

        if (::OpenClipboard(hWnd)) {
          ::EmptyClipboard();
          HANDLE cdata = ::SetClipboardData(CF_UNICODETEXT , hg);
          if (!cdata) report_error_message("twin.clipboard", ::GetLastError());
          ::CloseClipboard();
          return true;
        }

      global_free:
        ::GlobalFree(hg);
        return false;
      }

      static std::u16string convert_lf_to_crlf(std::u16string const& src) {
        std::u16string dst;
        bool cr = false;
        for (char16_t const c : src) {
          if (c == u'\n' && !cr)
            dst.append(1, u'\r');
          dst.append(1, c);
          cr = c == u'\r';
        }
        return dst;
      }
      static std::u16string convert_crlf_to_lf(std::u16string const& src) {
        std::u16string dst;
        bool cr = false;
        for (char16_t const c : src) {
          if (cr && c != '\n')
            dst.append(1, U'\r');
          if (!(cr = c == '\r'))
            dst.append(1, c);
        }
        if (cr) dst.append(1, U'\r');
        return dst;
      }
    private:
      virtual bool get_clipboard(std::u32string& data) override {
        std::u16string data3;
        if (!read_clipboard(data3)) return false;
        std::u16string const data2 = convert_crlf_to_lf(data3);
        data.clear();
        std::uint64_t state = 0;
        contra::encoding::utf16_decode(data2.c_str(), data2.c_str() + data2.size(), data, state);
        return true;
      }
      virtual bool set_clipboard(std::u32string const& data) override {
        if (!win->hWnd) return false;
        std::u16string data2;
        std::uint64_t state = 0;
        contra::encoding::utf16_encode(data.c_str(), data.c_str() + data.size(), data2, state);
        std::u16string const data3 = convert_lf_to_crlf(data2);
        return write_clipboard(data3, win->hWnd);
      }

    private:
      virtual bool request_change_size(curpos_t col, curpos_t row, coord_t xunit, coord_t yunit) override {
        auto& wm = win->wstat;
        if (xunit >= 0) xunit = limit::term_xunit.clamp(xunit);
        if (yunit >= 0) yunit = limit::term_yunit.clamp(yunit);

        if (col < 0 && row < 0) {
          // 文字サイズだけを変更→自動的に列・行を変えてウィンドウサイズを変えなくて済む様にする。
          if (wm.m_xunit != xunit || wm.m_yunit != yunit) {
            wm.m_xunit = xunit;
            wm.m_yunit = yunit;
            win->gbuffer.fstore().set_size(wm.m_xunit, wm.m_yunit);
            win->adjust_terminal_size_using_current_client_size();
            return true;
          }
        }

        bool changed = false;
        if (xunit >= 0 && wm.m_xunit != xunit) { wm.m_xunit = xunit; changed = true; }
        if (yunit >= 0 && wm.m_yunit != yunit) { wm.m_yunit = yunit; changed = true; }
        if (changed) win->gbuffer.fstore().set_size(wm.m_xunit, wm.m_yunit);
        if (col >= 0) {
          col = limit::term_col.clamp(col);
          if (col != wm.m_col) { wm.m_col = col; changed = true; }
        }
        if (row >= 0) {
          row = limit::term_row.clamp(row);
          if (row != wm.m_row) { wm.m_row = row; changed = true; }
        }
        if (changed) {
          win->adjust_window_size();
          win->manager.reset_size(wm.m_col, wm.m_row, wm.m_xunit, wm.m_yunit);
        }
        return true;
      }

    private:
      virtual bool create_new_session() override {
        return win->add_terminal_session();
      }

    };

    twin_events events {this};

  private:
    bool is_session_ready() { return hWnd && manager.is_active(); }
  public:
    HWND create_window(HINSTANCE hInstance) {
      gbuffer.fstore().set_size(wstat.m_xunit, wstat.m_yunit);

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
        myProg.hCursor       = ::LoadCursor(NULL, IDC_IBEAM);
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
        settings.calculate_window_width(wstat),
        settings.calculate_window_height(wstat),
        NULL,
        NULL,
        hInstance,
        NULL);

      this->start_blinking_timer();
      this->manager.set_events(events);
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
      coord_t const xadjust = wstat.calculate_client_width() - (coord_t) width;
      coord_t const yadjust = wstat.calculate_client_height() - (coord_t) height;
      if (xadjust || yadjust) {
        settings.window_size_xadjust += xadjust;
        settings.window_size_yadjust += yadjust;
        ::SetWindowPos(
          hWnd, NULL, 0, 0,
          settings.calculate_window_width(wstat), settings.calculate_window_height(wstat),
          SWP_NOZORDER | SWP_NOMOVE);
      }
    }
    void adjust_terminal_size_using_current_client_size() {
      if (!m_window_size_adjusted) return;
      if (!is_session_ready()) return;
      RECT rcClient;
      ::GetClientRect(hWnd, &rcClient);
      auto& wm = wstat;
      coord_t const width = rcClient.right - rcClient.left;
      coord_t const height = rcClient.bottom - rcClient.top;
      curpos_t const new_col = std::max(1, (width - 2 * wm.m_xframe) / wm.m_xunit);
      curpos_t const new_row = std::max(1, (height - 2 * wm.m_yframe) / wm.m_yunit);

      if (new_col != wm.m_col || new_row != wm.m_row) {
        wm.m_col = new_col;
        wm.m_row = new_row;
        manager.reset_size(new_col, new_row);
      }
    }
    void adjust_window_size() {
      if (!is_session_ready()) return;
      ::SetWindowPos(hWnd, NULL, 0, 0,
        settings.calculate_window_width(wstat), settings.calculate_window_height(wstat),
        SWP_NOZORDER | SWP_NOMOVE);
    }
    std::pair<coord_t, coord_t> get_window_size() {
      RECT rcClient;
      ::GetClientRect(hWnd, &rcClient);
      coord_t const width = rcClient.right - rcClient.left;
      coord_t const height = rcClient.bottom - rcClient.top;
      return {width, height};
    }

  private:
    static constexpr UINT BLINKING_TIMER_ID = 11; // 適当
    UINT m_blinking_timer_id = 0;
    void start_blinking_timer() {
      if (m_blinking_timer_id)
        ::KillTimer(hWnd, m_blinking_timer_id);
      m_blinking_timer_id = ::SetTimer(hWnd, BLINKING_TIMER_ID, settings.m_blinking_interval, NULL);
    }
    void reset_blinking_timer() {
      if (!is_session_ready()) return;
      start_blinking_timer();
    }
    void process_blinking_timer() {
      wstat.m_blinking_count++;
      if (renderer.has_blinking_cells())
        render_window();
    }

  private:
    static constexpr UINT CURSOR_TIMER_ID = 10; // 適当
    UINT m_cursor_timer_id = 0;

    friend class ansi::window_renderer_t<twin_graphics_t>;
    void unset_cursor_timer() {
      if (m_cursor_timer_id)
        ::KillTimer(hWnd, m_cursor_timer_id);
    }
    void reset_cursor_timer() {
      if (!is_session_ready()) return;
      this->unset_cursor_timer();
      m_cursor_timer_id = ::SetTimer(hWnd, CURSOR_TIMER_ID, settings.m_caret_interval, NULL);
      wstat.m_cursor_timer_count = 0;
    }
    void process_cursor_timer() {
      wstat.m_cursor_timer_count++;
      render_window();
    }

  private:

    // Note: background buffer として 3 枚使う。
    //   hdc0 ... 背景+内容+カーソル
    //   hdc1 ... 背景+内容
    //   hdc2 ... 背景 (未実装)
    void render_window() {
      if (!is_session_ready()) return;
      HDC hdc = GetDC(hWnd);
      {
        bool resized = false;
        auto [width, height] = get_window_size();
        gbuffer.setup(hWnd, hdc);
        gbuffer.update_window_size(width, height, &resized);
        renderer.render_view(*this, gbuffer, manager.app().view(), resized);
      }
      ReleaseDC(hWnd, hdc);
      ::ValidateRect(hWnd, NULL);
    }
    // WM_PAINT による再描画要求
    void redraw_window(HDC hdc, RECT const& rc) {
      bool resized = false;
      auto [width, height] = get_window_size();
      gbuffer.setup(hWnd, hdc);
      gbuffer.update_window_size(width, height, &resized);
      if (resized) {
        // 全体を再描画して全体を転送
        ::SelectClipRgn(hdc, NULL);
        renderer.render_view(*this, gbuffer, manager.app().view(), resized);
      } else {
        // 必要な領域だけ転送
        gbuffer.render(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, gbuffer.layer(0), rc.left, rc.top);
      }
    }

    enum extended_key_flags {
      toggle_capslock   = 0x100,
      toggle_numlock    = 0x200,
      toggle_scrolllock = 0x400,
    };

    void process_input(std::uint32_t key) {
      //contra::term::print_key(key, stderr);
      manager.input_key(key);
    }

    key_t get_modifiers() {
      key_t ret = 0;
      if (GetKeyState(VK_LSHIFT) & 0x8000) ret |= settings.term_mod_lshift;
      if (GetKeyState(VK_RSHIFT) & 0x8000) ret |= settings.term_mod_rshift;
      if (GetKeyState(VK_LCONTROL) & 0x8000) ret |= settings.term_mod_lcontrol;
      if (GetKeyState(VK_RCONTROL) & 0x8000) ret |= settings.term_mod_rcontrol;
      if (GetKeyState(VK_LMENU) & 0x8000) ret |= settings.term_mod_lalter;
      if (GetKeyState(VK_RMENU) & 0x8000) ret |= settings.term_mod_ralter;
      if (GetKeyState(VK_APPS) & 0x8000) ret |= settings.term_mod_menu;

      if (GetKeyState(VK_CAPITAL) & 1) ret |= toggle_capslock;
      if (GetKeyState(VK_NUMLOCK) & 1) ret |= toggle_numlock;
      if (GetKeyState(VK_SCROLL) & 1) ret |= toggle_scrolllock;
      return ret;
    }

    static bool is_keypad_shifted(key_t modifiers) {
      if (modifiers & modifier_shift) {
        for (UINT k = VK_NUMPAD0; k <= VK_NUMPAD9; k++)
          if (GetKeyState(k) & 0x8000) return true;
        if (GetKeyState(VK_DECIMAL) & 0x8000) return true;
      }
      return false;
    }

    byte m_kbflags[256];
    enum kbflag_flags {
      kbflag_pressed = 1,
    };
    void process_keyup(WPARAM wParam) {
      if (wParam < 256)
        m_kbflags[wParam] &= ~kbflag_pressed;
    }
    bool process_key(WPARAM wParam, key_t modifiers) {
      if (wParam < 256) {
        // Note #D0238: autorepeat 検出
        if (m_kbflags[wParam] & kbflag_pressed)
          modifiers |= modifier_autorepeat;
        else
          m_kbflags[wParam] |= kbflag_pressed;
      }

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
                std::fprintf(stderr, "key (unknown): wparam=%08" PRIx64 " flags=%x\n", (std::uint64_t) wParam, modifiers);
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
      key |= modifiers & _modifier_mask;
      if (settings.twin_disableMouseReportOnScrLock && (modifiers & toggle_scrolllock))
        key |= modifier_application;
      curpos_t const x1 = (x - wstat.m_xframe) / wstat.m_xunit;
      curpos_t const y1 = (y - wstat.m_yframe) / wstat.m_yunit;
      manager.input_mouse(key, x, y, x1, y1);
      if (manager.m_dirty) render_window();
    }

  private:
    bool m_ime_composition_active = false;
    void start_ime_composition() {
      this->m_ime_composition_active = true;
      if (settings.m_caret_hide_on_ime) wstat.m_hide_cursor = true;
      this->render_window();
      this->adjust_ime_composition();
    }
    void adjust_ime_composition() {
      if (!is_session_ready() || !m_ime_composition_active) return;
      util::raii hIMC(::ImmGetContext(hWnd), [this] (auto hIMC) { ::ImmReleaseContext(hWnd, hIMC); });

      win_font_manager_t& font_manager = gbuffer.fstore();

      font_t const current_font = font_resolver_t().resolve_font(manager.app().term().cursor().abuild.attr()) & ~font_rotation_mask;
      auto const [dx, dy, dxW] = font_manager.get_displacement(current_font);
      contra_unused(dx);
      contra_unused(dxW);
      LOGFONT logfont = font_manager.create_logfont(current_font);
      logfont.lfWidth = font_manager.width() * (current_font & font_decdwl ? 2 : 1);
      ::ImmSetCompositionFont(hIMC, const_cast<LOGFONT*>(&logfont));

      RECT rcClient;
      if (::GetClientRect(hWnd, &rcClient)) {
        auto& view = manager.app().view();
        COMPOSITIONFORM form;
        coord_t const x0 = rcClient.left + wstat.m_xframe + wstat.m_xunit * view.client_x();
        coord_t const y0 = rcClient.top + wstat.m_yframe + wstat.m_yunit * view.client_y();
        form.dwStyle = CFS_POINT;
        form.ptCurrentPos.x = x0;
        form.ptCurrentPos.y = y0 + std::round(dy);
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
      wstat.m_hide_cursor = false;
      this->render_window();
    }

    static void  debug_print_window_message(UINT msg) {
      const char* name = get_window_message_name(msg);
      if (name)
        std::fprintf(stderr, "message: %s\n", name);
      else
        std::fprintf(stderr, "message: %08x\n", msg);
    }
  public:
    LRESULT process_message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
      if (settings.m_debug_print_window_messages == 2)
        debug_print_window_message(msg);

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
      case WM_KEYUP:
      case WM_SYSKEYUP:
        if (hWnd != this->hWnd) goto defproc;
        process_keyup(wParam);
        return 0L;
      case WM_IME_CHAR:
        if (hWnd != this->hWnd) goto defproc;
        process_char(wParam, get_modifiers());
        return 0L;
      case WM_CHAR:
      case WM_DEADCHAR:
      case WM_SYSCHAR:
      case WM_SYSDEADCHAR:
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
        }
        goto mouse_event;
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
        if (m_cursor_timer_id && wParam == m_cursor_timer_id) {
          process_cursor_timer();
          return 0L;
        } else if (m_blinking_timer_id && wParam == m_blinking_timer_id) {
          process_blinking_timer();
          return 0L;
        }
        goto defproc;

      default:
        if (settings.m_debug_print_window_messages == 1)
          debug_print_window_message(msg);
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
    bool add_terminal_session() {
      term::terminal_session_parameters params;
      params.col = wstat.m_col;
      params.row = wstat.m_row;
      params.xunit = wstat.m_xunit;
      params.yunit = wstat.m_yunit;
      params.exec_error_handler = &exec_error_handler;
      actx.read("session_term", params.env["TERM"] = "xterm-256color");
      actx.read("session_shell", params.shell = "/bin/bash");
      std::unique_ptr<term::terminal_application> sess = contra::term::create_terminal_session(params);
      if (!sess) return false;

      contra::ansi::tstate_t& s = sess->state();
      s.m_default_fg_space = contra::ansi::color_space_rgb;
      s.m_default_bg_space = contra::ansi::color_space_rgb;
      s.m_default_fg_color = contra::ansi::rgb(0x00, 0x00, 0x00);
      s.m_default_bg_color = contra::ansi::rgb(0xFF, 0xFF, 0xFF);
      // s.m_default_fg_color = contra::ansi::rgb(0xD0, 0xD0, 0xD0);//@color
      // s.m_default_bg_color = contra::ansi::rgb(0x00, 0x00, 0x00);
      manager.add_app(std::move(sess));
      return true;
    }
  public:
    int m_exit_code = 0;
    bool do_loop() {
      if (!this->add_terminal_session()) return false;

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
      return true;
    }
  };

  twin_window_t* main_window = nullptr;

  LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return main_window->process_message(hWnd, msg, wParam, lParam);
  }
}

  bool run(contra::app::context& actx, HINSTANCE hInstance, int nCmdShow) {
#ifdef __CYGWIN__
    static bool cygwin_initialized = false;
    if (!cygwin_initialized) {
      cygwin_initialized = true;
      bool disable_pcon = false;
      actx.read("twin_disable_pcon", disable_pcon);
      if (disable_pcon) {
        std::string value = "disable_pcon";
        if (const char* env = std::getenv("CYWGIN"); env && *env) {
          value += " ";
          value += env;
        }
        ::setenv("CYGWIN", value.c_str(), 1);
      }
    }
#endif

#ifdef CONTRA_TWIN_SUPPORT_HIGHDPI
    SetProcessDPIAware();
#endif
    twin_window_t win(actx);
    main_window = &win;

    HWND const hWnd = win.create_window(hInstance);
    ::ShowWindow(hWnd, nCmdShow);
    ::UpdateWindow(hWnd);
    return win.do_loop();
  }
  bool run(contra::app::context& actx) {
    HINSTANCE const hInstance = GetModuleHandle(NULL);
    return run(actx, hInstance, SW_SHOW);
  }
}

#ifdef twin_main
// from http://www.kumei.ne.jp/c_lang/index_sdk.html
extern "C" int WINAPI _tWinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPreInst, [[maybe_unused]] LPTSTR lpszCmdLine, int nCmdShow) {
  contra::app::context actx;
  std::string config_dir = contra::term::get_config_directory();
  actx.load((config_dir + "/contra/twin.conf").c_str());
  if (!contra::twin::run(actx, hInstance, nCmdShow)) return 1;
  return 0;
}

// from https://cat-in-136.github.io/2012/04/unicodemingw32twinmainwwinmain.html
#if defined(_UNICODE) && !defined(_tWinMain)
int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
  HINSTANCE const hInstance = GetModuleHandle(NULL);
  int const retval = _tWinMain(hInstance, NULL, (LPTSTR) TEXT("") /* lpCmdLine is not available*/, SW_SHOW);
  return retval;
}
#endif

#endif
