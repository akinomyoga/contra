// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_ansi_render_hpp
#define contra_ansi_render_hpp
#include <mwg/except.h>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <utility>
#include <algorithm>
#include <tuple>
#include <vector>
#include "../contradef.hpp"
#include "line.hpp"
#include "term.hpp"

namespace contra {
namespace ansi {

  struct window_state_t {
    curpos_t m_col = 80;//@size
    curpos_t m_row = 30;
    coord_t m_xpixel = 7;
    coord_t m_ypixel = 13;
    coord_t m_xframe = 1;
    coord_t m_yframe = 1;

    coord_t calculate_client_width() const {
      return m_xpixel * m_col + 2 * m_xframe;
    }
    coord_t calculate_client_height() const {
      return m_ypixel * m_row + 2 * m_yframe;
    }

    // 実際の幅 (要求した幅と実際の幅が異なるかもしれないので)
    coord_t m_window_width;
    coord_t m_window_height;

  public:
    window_state_t() {
      m_window_width = calculate_client_width();
      m_window_height = calculate_client_height();
    }

  public:
    std::uint32_t m_blinking_count = 0;
    std::uint32_t m_cursor_timer_count = 0;
    bool m_hide_cursor = false;
    coord_t m_caret_underline_min_height = 2;
    coord_t m_caret_vertical_min_width = 2;

    // カーソルが表示されるかどうかを取得します。
    // 点滅中で一時的に消えている場合は true です。
    bool is_cursor_visible(term_view_t const& view) const {
      return !m_hide_cursor && view.is_cursor_visible();
    }
    // カーソルが今の瞬間表示されているかどうかを取得します。
    // 点滅中で一時的に消えている場合は false です。
    bool is_cursor_appearing(term_view_t const& view) const {
      return is_cursor_visible(view) && (!view.is_cursor_blinking() || !(m_cursor_timer_count & 1));
    }
  };

  class status_tracer_t {
  public:
    status_tracer_t() {}

    void store(window_state_t const& wstat, term_view_t const& view) {
      store_metric(wstat);
      store_content(view);
      store_cursor(wstat, view);
      store_blinking_state(wstat);
    }

  private:
    coord_t m_xframe = 0;
    coord_t m_yframe = 0;
    curpos_t m_col = 0;
    curpos_t m_row = 0;
    coord_t m_xpixel = 0;
    coord_t m_ypixel = 0;
  public:
    bool is_metric_changed(window_state_t const& wstat) const {
      if (m_xframe != wstat.m_xframe) return true;
      if (m_yframe != wstat.m_yframe) return true;
      if (m_col != wstat.m_col) return true;
      if (m_row != wstat.m_row) return true;
      if (m_xpixel != wstat.m_xpixel) return true;
      if (m_ypixel != wstat.m_ypixel) return true;
      return false;
    }
    void store_metric(window_state_t const& wstat) {
      m_xframe = wstat.m_xframe;
      m_yframe = wstat.m_yframe;
      m_col = wstat.m_col;
      m_row = wstat.m_row;
      m_xpixel = wstat.m_xpixel;
      m_ypixel = wstat.m_ypixel;
    }

  private:
    struct line_trace_t {
      std::uint32_t id = (std::uint32_t) -1;
      std::uint32_t version = 0;
      bool has_blinking = false;
    };
    std::vector<line_trace_t> m_lines;
  public:
    bool is_content_changed(term_view_t const& view) const {
      std::size_t const height = view.height();
      if (height != m_lines.size()) return true;
      for (std::size_t iline = 0; iline < height; iline++) {
        line_t const& line = view.line(iline);
        if (line.id() != m_lines[iline].id) return true;
        if (line.version() != m_lines[iline].version) return true;
      }
      return false;
    }
    void store_content(term_view_t const& view) {
      curpos_t const height = view.height();
      m_lines.resize(height);
      for (curpos_t i = 0; i < height; i++) {
        line_t const& original_line = view.line(i);
        m_lines[i].id = original_line.id();
        m_lines[i].version = original_line.version();
        m_lines[i].has_blinking = original_line.has_blinking_cells();
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
    bool is_blinking_changed(window_state_t const& wstat) const {
      if (!this->has_blinking_cells()) return false;
      return this->m_blinking_count != wstat.m_blinking_count;
    }
    void store_blinking_state(window_state_t const& wstat) {
      this->m_blinking_count = wstat.m_blinking_count;
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
    bool is_cursor_changed(window_state_t const& wstat, term_view_t const& view) const {
      if (m_cur_visible != wstat.is_cursor_visible(view)) return true;
      if (m_cur_x != view.x() || m_cur_y != view.y() || m_cur_xenl != view.xenl()) return true;
      if (m_cur_shape != view.cursor_shape()) return true;
      if (m_cur_blinking != view.is_cursor_blinking()) return true;
      return false;
    }
    void store_cursor(window_state_t const& wstat, term_view_t const& view) {
      m_cur_visible = wstat.is_cursor_visible(view);
      m_cur_x = view.x();
      m_cur_y = view.y();
      m_cur_xenl = view.xenl();
      m_cur_shape = view.cursor_shape();
    }
  };

  class color_resolver_t {
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

  class font_metric_t {
  private:
    coord_t m_width, m_height;

  public:
    font_metric_t(coord_t xpixel, coord_t ypixel) {
      m_width = xpixel;
      m_height = ypixel;
    }

    coord_t width() const { return m_width; }
    coord_t height() const { return m_height; }
    void set_size(coord_t width, coord_t height) {
      this->m_width = width;
      this->m_height = height;
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
          dy -= 0.2 * this->m_height;
        else if (font & font_layout_sub) {
          // Note: 下端は baseline より下なので下端に合わせれば十分。
          dy += this->m_height - this->small_height();
        }
        if (font & (font_layout_framed | font_layout_sup | font_layout_sub)) {
          dxW = 0.5 * (this->m_width - this->small_width());
          dy += 0.5 * (this->m_height - this->small_height());
        }
        if (font & font_flag_italic) {
          coord_t width = this->m_width;
          if (font & (font_layout_framed | font_layout_sup | font_layout_sub))
            width = this->small_width();
          dx -= 0.4 * width;
        }
        if (font & (font_layout_upper_half | font_layout_lower_half)) dy *= 2;
        if (font & font_decdwl) dxI *= 2;
        if (font & font_layout_lower_half) dy -= this->m_height;
        dx = std::round(dxI + dxW);
        dy = std::round(dy);
      }
      return {dx, dy, dxW};
    }
  };

  class font_resolver_t {
    using attribute_t = contra::dict::attribute_t;
    using xflags_t = contra::dict::xflags_t;
    using aflags_t = contra::dict::aflags_t;

  private:
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

  class window_renderer_t {
    window_state_t& wstat;

  public:
    window_renderer_t(window_state_t& wstat): wstat(wstat) {}

  public:
    template<typename Graphics>
    void draw_cursor(Graphics& graphics, term_view_t const& view) {
      int const cursor_shape = view.cursor_shape();

      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const ypixel = wstat.m_ypixel;
      coord_t const xpixel = wstat.m_xpixel;

      coord_t x0 = xorigin + xpixel * view.x();
      coord_t const y0 = yorigin + ypixel * view.y();

      coord_t size;
      bool underline = false;
      if (view.xenl()) {
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
        coord_t const height = std::min(ypixel, std::max(size, wstat.m_caret_underline_min_height));
        coord_t const x1 = x0, y1 = y0 + ypixel - height;
        graphics.invert_rectangle(x1, y1, x1 + xpixel, y1 + height);
      } else {
        coord_t const width = std::min(xpixel, std::max(size, wstat.m_caret_vertical_min_width));
        graphics.invert_rectangle(x0, y0, x0 + width, y0 + ypixel);
      }
    }

  public:
    template<typename Graphics>
    void draw_background(Graphics& graphics, term_view_t const& view, std::vector<std::vector<cell_t>>& content) {
      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const ypixel = wstat.m_ypixel;
      coord_t const xpixel = wstat.m_xpixel;
      curpos_t const height = view.height();
      tstate_t const& s = view.state();
      color_resolver_t _color(s);

      color_t const bg = _color.resolve(s.m_default_bg_space, s.m_default_bg_color);
      graphics.fill_rectangle(0, 0, wstat.m_window_width, wstat.m_window_height, bg);

      coord_t x = xorigin, y = yorigin;
      coord_t x0 = x;
      color_t bg0 = 0;
      auto _fill = [=, &graphics, &x, &y, &x0, &bg0] () {
        if (x0 >= x || !bg0) return;
        graphics.fill_rectangle(x0, y, x, y + ypixel, bg0);
      };

      for (curpos_t iline = 0; iline < height; iline++, y += ypixel) {
        std::vector<cell_t>& cells = content[iline];
        x = xorigin;
        x0 = x;
        bg0 = 0;

        for (std::size_t i = 0; i < cells.size(); ) {
          auto const& cell = cells[i++];
          color_t const bg1 = _color.resolve_bg(cell.attribute);
          if (bg1 != bg0) {
            _fill();
            bg0 = bg1;
            x0 = x;
          }
          x += cell.width * xpixel;
        }
        _fill();
      }
    }

  private:
    template<typename Graphics>
    void clip_decdhl_upper(Graphics& g, coord_t y) {
      g.clip_rectangle(0, 0, wstat.m_window_width, y + wstat.m_ypixel);
    }
    template<typename Graphics>
    void clip_decdhl_lower(Graphics& g, coord_t y) {
      g.clip_rectangle(0, y, wstat.m_window_width, wstat.m_window_height);
    }

  public:
    template<typename Graphics>
    void draw_characters(Graphics& g, term_view_t const& view, std::vector<std::vector<contra::ansi::cell_t>>& content) {
      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const ypixel = wstat.m_ypixel;
      coord_t const xpixel = wstat.m_xpixel;
      curpos_t const height = view.height();
      tstate_t const& s = view.state();

      constexpr std::uint32_t flag_processed = character_t::flag_private1;

      aflags_t invisible_flags = attribute_t::is_invisible_set;
      if (wstat.m_blinking_count & 1) invisible_flags |= attribute_t::is_rapid_blink_set;
      if (wstat.m_blinking_count & 2) invisible_flags |= attribute_t::is_blink_set;
      auto _visible = [invisible_flags] (std::uint32_t code, aflags_t aflags, bool sp_visible = false) {
        return code != ascii_nul && (sp_visible || code != ascii_sp)
          && character_t::is_char(code & flag_processed)
          && !(aflags & invisible_flags);
      };

      color_resolver_t _color(s);
      font_resolver_t _font;
      font_metric_t _fmetric(wstat.m_xpixel, wstat.m_ypixel);

      coord_t x = xorigin, y = yorigin, xL = xorigin, xR = xorigin;
      coord_t dx = 0, dy = 0;
      double dxW = 0.0;

      typename Graphics::character_buffer charbuff;
      auto _push_char = [&charbuff, &xR, &dx, &dxW] (std::uint32_t code, coord_t prog, curpos_t width) {
        xR += prog;

        // 文字位置の補正
        if (dxW && width > 1) {
          int const shift = dxW * (width - 1);
          if (charbuff.empty())
            dx += shift;
          else
            charbuff.shift(shift);
          prog -= shift;
        }

        charbuff.add_char(code, prog);
      };

      for (curpos_t iline = 0; iline < height; iline++, y += ypixel) {
        std::vector<cell_t>& cells = content[iline];
        x = xorigin;
        charbuff.reserve(cells.size());

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
          std::tie(dx, dy, dxW) = _fmetric.get_displacement(font);

          charbuff.clear();
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
              charbuff.shift(cell2_progress);
            } else if (is_cluster || (fg == fg2 && font == font2)) {
              _push_char(code2, cell2_progress, cell2.width);
            } else {
              if (font & font_layout_proportional) break;
              charbuff.shift(cell2_progress);
              continue;
            }

            if (i == j) {
              i++;
              x += cell2_progress;
            } else
              cells[j].character.value |= flag_processed;
          }

          // DECDHL用の制限
          if (font & font_layout_upper_half)
            this->clip_decdhl_upper(g, y);
          else if (font & font_layout_lower_half)
            this->clip_decdhl_lower(g, y);

          if (font & font_rotation_mask) {
            g.draw_rotated_characters(xL, y, dx, dy, xR - xL, charbuff, font, fg);
          } else if (font & font_layout_proportional) {
            // 配置を GDI に任せる
            g.draw_text(xL + dx, y + dy, charbuff, font, fg);
          } else {
            g.draw_characters(xL + dx, y + dy, charbuff, font, fg);
          }

          // DECDHL用の制限の解除
          if (font & (font_layout_upper_half | font_layout_lower_half))
            g.clip_clear();
        }

        // clear private flags
        for (cell_t& cell : cells) cell.character.value &= ~flag_processed;
      }
    }
    template<typename Graphics>
    void draw_characters_mono(Graphics& g, term_view_t const& view, std::vector<std::vector<cell_t>> const& content) {
      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const ypixel = wstat.m_ypixel;
      coord_t const xpixel = wstat.m_xpixel;
      curpos_t const height = view.height();

      std::vector<cell_t> cells;
      typename Graphics::character_buffer charbuff;
      for (curpos_t y = 0; y < height; y++) {
        std::vector<cell_t> const& cells = content[y];

        coord_t xoffset = xorigin;
        coord_t yoffset = yorigin + y * ypixel;
        charbuff.clear();
        charbuff.reserve(cells.size());
        for (auto const& cell : cells) {
          std::uint32_t code = cell.character.value;
          if (code & character_t::flag_cluster_extension)
            code &= ~character_t::flag_cluster_extension;
          if (code == ascii_nul || code == ascii_sp) {
            if (charbuff.empty())
              xoffset += cell.width * xpixel;
            else
              charbuff.shift(cell.width * xpixel);
          } else if (code == (code & character_t::unicode_mask)) {
            charbuff.add_char(code, cell.width * xpixel);
          }
          // ToDo: その他の文字・マーカーに応じた処理。
        }
        if (!charbuff.empty())
          g.draw_characters(xoffset, yoffset, charbuff, 0, 0);
      }
    }

  private:
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
  public:
    template<typename Graphics>
    void draw_decoration(Graphics& g, term_view_t const& view, std::vector<std::vector<cell_t>>& content) {
      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const ypixel = wstat.m_ypixel;
      coord_t const xpixel = wstat.m_xpixel;
      curpos_t const height = view.height();
      tstate_t const& s = view.state();
      color_resolver_t _color(s);

      coord_t x = xorigin, y = yorigin;

      color_t color0 = 0;

      decoration_horizontal_t dec_ul([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        coord_t h = (coord_t) std::ceil(ypixel * 0.05);
        switch (style & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_upper_half: return;
        case attribute_t::decdhl_lower_half:
          h *= 2;
          break;
        }

        g.fill_rectangle(x1, y + ypixel - h, x2, y + ypixel, color0);
        if ((style & ~attribute_t::decdhl_mask) == 2)
          g.fill_rectangle(x1, y + ypixel - 3 * h, x2, y + ypixel - 2 * h, color0);
      });

      decoration_horizontal_t dec_sl([&] (coord_t x1, coord_t x2, std::uint32_t style) {
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
            g.fill_rectangle(x1, y2 - 2 * h, x2, y2 - h, color0);
            y2 += h;
            break;
          }
        }
        g.fill_rectangle(x1, y2 - h, x2, y2, color0);
      });

      decoration_horizontal_t dec_ol([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        coord_t h = (coord_t) std::ceil(ypixel * 0.05);
        switch (style & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_lower_half: return;
        case attribute_t::decdhl_upper_half:
          h *= 2;
          return;
        }
        g.fill_rectangle(x1, y, x2, y + h, color0);
        if ((style & ~attribute_t::decdhl_mask) == 2)
          g.fill_rectangle(x1, y + 2 * h, x2, y + 3 * h, color0);
      });

      auto _draw_frame = [&] (coord_t x1, coord_t x2, xflags_t xflags, int lline, int rline) {
        // 上の線と下の線は dec_ul, dec_ol に任せる。
        coord_t w = (coord_t) std::ceil(ypixel * 0.05);
        if (xflags & attribute_t::decdhl_mask) w *= 2;
        if (lline) {
          g.fill_rectangle(x1, y, x1 + w, y + ypixel, color0);
          if (lline > 1)
            g.fill_rectangle(x1 + 2 * w, y, x1 + 3 * w, y + ypixel, color0);
        }
        if (rline) {
          g.fill_rectangle(x2 - w, y, x2, y + ypixel, color0);
          if (rline > 1)
            g.fill_rectangle(x2 - 3 * w, y, x2 - 2 * w, y + ypixel, color0);
        }
      };

      auto _draw_circle = [&] (coord_t x1, coord_t x2, xflags_t xflags) {
        coord_t w = (coord_t) std::ceil(ypixel * 0.04);
        coord_t y1 = y, y2 = y + ypixel;
        bool is_clipped = false;
        switch (xflags & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_upper_half:
          is_clipped = true;
          this->clip_decdhl_upper(g, y);
          w *= 2;
          y2 = y + 2 * ypixel;
          break;
        case attribute_t::decdhl_lower_half:
          is_clipped = true;
          this->clip_decdhl_lower(g, y);
          w *= 2;
          y1 = y - ypixel;
          y2 = y + ypixel;
          break;
        }
        g.draw_ellipse(x1, y1, x2, y2, color0, w);
        if (is_clipped)
          g.clip_clear();
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
        g.fill_ellipse(xL, yT, xR + 1, yB + 1, color0);
      };

      for (curpos_t iline = 0; iline < height; iline++, y += ypixel) {
        std::vector<cell_t>& cells = content[iline];
        x = xorigin;
        color0 = 0;

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
  };

}
}

#endif