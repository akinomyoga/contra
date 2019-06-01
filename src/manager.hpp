// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_manager_hpp
#define contra_manager_hpp
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <sstream>
#include <iterator>
#include <memory>
#include <chrono>
#include "ansi/line.hpp"
#include "ansi/term.hpp"
#include "enc.utf8.hpp"

namespace contra {
namespace term {

  using curpos_t = contra::ansi::curpos_t;
  using coord_t = contra::ansi::coord_t;
  using board_t = contra::ansi::board_t;
  using tstate_t = contra::ansi::tstate_t;
  using term_t = contra::ansi::term_t;

  class terminal_application {
    curpos_t m_width = 80, m_height = 24;
    coord_t m_xpixel = 7, m_ypixel = 13;
  public:
    curpos_t width() const { return m_width; }
    curpos_t height() const { return m_height; }
    curpos_t xpixel() const { return m_xpixel; }
    curpos_t ypixel() const { return m_ypixel; }

    virtual void reset_size(curpos_t width, curpos_t height) {
      this->reset_size(width, height, m_xpixel, m_ypixel);
    }
    virtual void reset_size(curpos_t width, curpos_t height, coord_t xpixel, coord_t ypixel) {
      m_width = std::clamp(width, limit::minimal_terminal_col, limit::maximal_terminal_col);
      m_height = std::clamp(height, limit::minimal_terminal_row, limit::maximal_terminal_row);
      m_xpixel = std::clamp(xpixel, limit::minimal_terminal_xpixel, limit::maximal_terminal_xpixel);
      m_ypixel = std::clamp(ypixel, limit::minimal_terminal_ypixel, limit::maximal_terminal_ypixel);
      if (m_term) m_term->board().reset_size(m_width, m_height);
    }

  private:
    std::unique_ptr<contra::ansi::term_t> m_term;
  public:
    term_t& term() { return *m_term; }
    term_t const& term() const { return *m_term; }
    board_t& board() { return m_term->board(); }
    board_t const& board() const { return m_term->board(); }
    tstate_t& state() { return m_term->state(); }
    tstate_t const& state() const { return m_term->state(); }

  public:
    terminal_application() {
      reset_size(80, 24, 13, 7);
    }
    virtual ~terminal_application() {}

    bool initialize() {
      if (m_term) return true;
      m_term = std::make_unique<contra::ansi::term_t>(m_width, m_height);
      return true;
    }

  public:
    virtual bool process() { return false; }
    virtual bool is_active() const { return true; }
    virtual bool is_alive() { return true; }
    virtual void terminate() {}

  public:
    virtual bool input_key(key_t key) {
      return m_term->input_key(key);
    }
    virtual bool input_mouse(key_t key, coord_t px, coord_t py, curpos_t x, curpos_t y) {
      return m_term->input_mouse(key, px, py, x, y);
    }
    virtual bool input_paste(std::u32string const& data) {
      return m_term->input_paste(data);
    }
  };

  class mouse_events {
  public:
    virtual ~mouse_events() {}
    virtual void on_select_initialize([[maybe_unused]] key_t key, [[maybe_unused]] curpos_t x, [[maybe_unused]] curpos_t y) {}
    virtual void on_select_finalize([[maybe_unused]] key_t key, [[maybe_unused]] curpos_t x, [[maybe_unused]] curpos_t y) {}
    virtual bool on_select_update([[maybe_unused]] key_t key, [[maybe_unused]] curpos_t x, [[maybe_unused]] curpos_t y) { return false; }
    virtual void on_select([[maybe_unused]] key_t key, [[maybe_unused]] curpos_t x, [[maybe_unused]] curpos_t y) {}
    virtual void on_click([[maybe_unused]] key_t key, [[maybe_unused]] curpos_t x, [[maybe_unused]] curpos_t y) {}
    virtual void on_multiple_click([[maybe_unused]] key_t key, [[maybe_unused]] curpos_t x, [[maybe_unused]] curpos_t y, [[maybe_unused]] int count) {}
  };

  class mouse_event_detector {
    bool button_state[5] = {};
    int button_count = 0;

    int drag_state = 0;
    int drag_button = 0;

    int multiple_click_threshold = 500;
    int multiple_click_button = 0;
    int multiple_click_count = 0;
    std::chrono::high_resolution_clock::time_point multiple_click_time;

    mouse_events* events = nullptr;

  public:
    mouse_event_detector() {}
    mouse_event_detector(mouse_events* events): events(events) {}

  private:
    bool process_mouse_down(key_t key, curpos_t x, curpos_t y, int button_index) {
      if (this->button_state[button_index - 1]) return false;
      this->button_state[button_index - 1] = true;
      this->button_count++;

      if (this->button_count == 1) {
        this->drag_state = 1;
        events->on_select_initialize(key, x, y);
      } else if (this->drag_state) {
        this->drag_state = 0;
        events->on_select_finalize(key, x, y);
      }

      // 複数クリックの検出
      if (this->multiple_click_button != button_index) {
        if (this->button_count == 1) {
          this->multiple_click_button = button_index;
          this->multiple_click_count = 1;
        } else
          this->multiple_click_button = 0;
      } else {
        // Note: double click, triple click, etc. の検出。
        //   普通と違って前回のマウスボタンの解放からの時間で判定する。
        //   これは自分の趣味。
        auto const current_time = std::chrono::high_resolution_clock::now();
        auto const msec = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - this->multiple_click_time).count();
        if (msec < this->multiple_click_threshold) {
          if (++this->multiple_click_count >= 2)
            events->on_multiple_click(key, x, y, this->multiple_click_count);
        } else {
          this->multiple_click_count = 1;
        }
      }

      return true;
    }
    bool process_mouse_drag(key_t key, curpos_t x, curpos_t y, int button_index) {
      if (this->drag_state) {
        this->drag_state = 2;
        if (!events->on_select_update(key, x, y)) {
          this->drag_state = 0;
          events->on_select_finalize(key, x, y);
        }
      }

      if (button_index == this->multiple_click_button) {
        // Note: 移動したら mouse down 回数を 1 に戻して
        //   次は double click しか起こさない様にする。これは自分の趣味。
        this->multiple_click_count = 1;
      } else {
        this->multiple_click_button = 0;
      }

      return true;
    }
    bool process_mouse_up(key_t key, curpos_t x, curpos_t y, int button_index) {
      if (!this->button_state[button_index - 1]) return false;
      this->button_state[button_index - 1] = false;
      this->button_count--;

      if (this->drag_state) {
        if (this->drag_state == 2) {
          if (events->on_select_update(key, x, y))
            events->on_select(key, x, y);
        } else if (this->drag_state == 1) {
          if (button_index != this->multiple_click_button ||
            this->multiple_click_count == 1)
            events->on_click(key, x, y);
        }
        this->drag_state = 0;
        events->on_select_finalize(key, x, y);
      }

      if (this->button_count == 0) {
        if (button_index == this->multiple_click_button) {
          this->multiple_click_time = std::chrono::high_resolution_clock::now();
        } else {
          this->multiple_click_button = 0;
        }
      }

      return true;
    }
    bool process_mouse_move([[maybe_unused]] key_t key, [[maybe_unused]] curpos_t x, [[maybe_unused]] curpos_t y) {
      this->drag_state = 0;
      this->multiple_click_button = 0;
      return true;
    }

  public:
    bool input_mouse(key_t key, [[maybe_unused]] coord_t px, [[maybe_unused]] coord_t py, curpos_t x, curpos_t y) {
      using namespace contra::ansi;
      int const button_index = (key & _key_mouse_button_mask) >> _key_mouse_button_shft;
      if (1 <= button_index && button_index <= 5) {
        switch (key & _key_mouse_event_mask) {
        case _key_mouse_event_down:
          return process_mouse_down(key, x, y, button_index);
        case _key_mouse_event_move:
          return process_mouse_drag(key, x, y, button_index);
        case _key_mouse_event_up:
          return process_mouse_up(key, x, y, button_index);
        }
      } else {
        if (key == key_mouse_move)
          return process_mouse_move(key, x, y);
      }
      return false;
    }
  };

  class terminal_events {
  public:
    virtual ~terminal_events() {}

    virtual bool get_clipboard([[maybe_unused]] std::u32string& data) { return false; }
    virtual bool set_clipboard([[maybe_unused]] std::u32string const& data) { return false; }
    virtual bool request_change_size(
      [[maybe_unused]] curpos_t col,
      [[maybe_unused]] curpos_t row,
      [[maybe_unused]] coord_t xpixel,
      [[maybe_unused]] coord_t ypixel
    ) { return false; }
  };

  class terminal_manager {
    std::vector<std::shared_ptr<terminal_application> > m_apps;
    terminal_events* m_events = nullptr;

    curpos_t m_width = 80, m_height = 24;
    coord_t m_xpixel = 7, m_ypixel = 13;

  public:
    terminal_manager() {
      this->initialize_zoom();
    }

  public:
    terminal_application& app() const {
      mwg_check(m_apps.size());
      return *m_apps[0].get();
    }
    template<typename T>
    void add_app(T&& app) { m_apps.emplace_back(std::forward<T>(app)); }
    void set_events(terminal_events& events) { this->m_events = &events; }

  private:
    // ToDo: foreground/background で優先順位をつけたい。
    // 後、現在 process1 を呼び出す度に全ての app に対して処理を行っているが、
    // 一度 processed = false になった物は何度も試さない様にする。
    bool process1() {
      bool processed = false;
      for (auto const& app : m_apps)
        if (app->process())
          processed = true;
      return processed;
    }
  public:
    bool do_events() {
      bool processed = false;
      auto const time0 = std::chrono::high_resolution_clock::now();
      while (this->process1()) {
        processed = true;
        auto const time1 = std::chrono::high_resolution_clock::now();
        auto const msec = std::chrono::duration_cast<std::chrono::milliseconds>(time1 - time0);
        if (msec.count() > 20) break;
      }
      if (processed) m_dirty = true;
      return processed;
    }

    bool is_active() {
      for (auto const& app : m_apps)
        if (app->is_active()) return true;
      return false;
    }
    bool is_alive() {
      // ToDo: 死んだ物に関しては閉じる?
      for (auto const& app : m_apps)
        if (app->is_alive()) return true;
      return false;
    }
    void terminate() {
      for (auto const& app : m_apps)
        app->terminate();
    }

  public:
    bool input_key(key_t key) {
      using namespace contra::ansi;
      if (key & modifier_application) {
        key &= ~modifier_application;
        switch (key) {
        case ascii_0:
          this->update_zoom(0);
          return true;
        case ascii_plus:
          this->update_zoom(m_zoom_level + 1);
          return true;
        case ascii_minus:
          this->update_zoom(m_zoom_level - 1);
          return true;
        default:
          return false;
        }
      } else
        return app().input_key(key);
    }

  private:
    curpos_t      m_sel_beg_x = 0;
    curpos_t      m_sel_beg_y = 0;
    bool          m_sel_beg_online = false;
    std::uint32_t m_sel_beg_lineid = 0;

    int      m_sel_type = 0;
    curpos_t m_sel_end_x = 0;
    curpos_t m_sel_end_y = 0;

  private:
    void selection_clear() {
      m_sel_type = 0;
      for (auto& line : app().board().m_lines)
        m_dirty |= line.clear_selection();
    }

    void selection_initialize(curpos_t x, curpos_t y) {
      selection_clear();
      m_sel_type = 0;
      m_sel_beg_x = x;
      m_sel_beg_y = y;
      m_sel_beg_online = y < (curpos_t) app().board().m_lines.size();
      if (m_sel_beg_online)
        m_sel_beg_lineid = app().board().m_lines[y].id();
    }

    bool selection_find_start_line() {
      using namespace contra::ansi;

      auto const& lines = app().board().m_lines;
      if (!m_sel_beg_online)
        return m_sel_beg_y >= (curpos_t) lines.size();

      if (m_sel_beg_y < (curpos_t) lines.size() &&
        lines[m_sel_beg_y].id() == m_sel_beg_lineid)
        return true;

      for (std::size_t i = 0; i < lines.size(); i++) {
        if (lines[i].id() == m_sel_beg_lineid) {
          m_sel_beg_y = i;
          return true;
        }
      }

      return false;
    }
    bool selection_update(key_t key, curpos_t x, curpos_t y) {
      using namespace contra::ansi;

      m_sel_type = 1 | (key & _modifier_mask);
      m_sel_end_x = x;
      m_sel_end_y = y;

      board_t& b = app().board();
      tstate_t& s = app().state();

      bool const gatm =  s.get_mode(mode_gatm);
      bool const truncate = !(m_sel_type & modifier_control);
      if (!selection_find_start_line()) {
        selection_clear();
        return false;
      }

      // 選択範囲 (データ部での範囲に変換)
      curpos_t x1 = m_sel_beg_x;
      curpos_t y1 = m_sel_beg_y;
      curpos_t x2 = m_sel_end_x;
      curpos_t y2 = m_sel_end_y;
      curpos_t const iN = b.m_lines.size();
      if (y1 < iN) x1 = b.to_data_position(y1, x1);
      if (y2 < iN) x2 = b.to_data_position(y2, x2);

      if (m_sel_type & modifier_meta) {
        // 矩形選択
        if (y1 > y2) std::swap(y1, y2);
        if (x1 > x2) std::swap(x1, x2);
        if (y1 >= iN) {
          y1 = 0; y2 = -1;
        } else if (y2 >= iN) {
          y2 = iN - 1;
        }

        curpos_t i = 0;
        while (i < y1)
          m_dirty |= b.m_lines[i++].clear_selection();
        while (i <= y2)
          m_dirty |= b.m_lines[i++].set_selection(x1, x2 + 1, truncate, gatm, true);
        while (i < iN)
          m_dirty |= b.m_lines[i++].clear_selection();
      } else {
        if (y1 > y2) {
          std::swap(y1, y2);
          std::swap(x1, x2);
        } else if (y1 == y2 && x1 > x2) {
          std::swap(x1, x2);
        }
        if (y1 >= iN) {
          y1 = 0; y2 = -1;
        } else if (y2 >= iN) {
          y2 = iN - 1;
          x2 = b.m_width + 1;
        }

        // 選択状態の更新 (前回と同じ場合は skip できたりしないか?)
        curpos_t i = 0;
        while (i < y1)
          m_dirty |= b.m_lines[i++].clear_selection();
        if (y1 == y2) {
          m_dirty |= b.m_lines[i++].set_selection(x1, x2 + 1, truncate, gatm, true);
        } else if (y1 < y2) {
          m_dirty |= b.m_lines[i++].set_selection(x1, b.m_width, truncate, gatm, true);
          while (i < y2)
            m_dirty |= b.m_lines[i++].set_selection(0, b.m_width + 1, truncate, gatm, true);
          m_dirty |= b.m_lines[i++].set_selection(0, x2 + 1, truncate, gatm, true);
        }
        while (i < iN)
          m_dirty |= b.m_lines[i++].clear_selection();
      }

      return true;
    }

    void selection_extract_rectangle(std::u32string& data) {
      using namespace contra::ansi;
      board_t const& b = app().board();
      curpos_t y1 = m_sel_beg_y;
      curpos_t y2 = m_sel_end_y;
      if (y1 > y2) std::swap(y1, y2);

      std::vector<std::pair<curpos_t, std::u32string> > lines;
      curpos_t min_x = -1;
      {
        curpos_t const nline = b.m_lines.size();
        curpos_t skipped_line_count = 0;
        bool started = false;
        std::u32string line_data;
        for (curpos_t iline = 0; iline < nline; iline++) {
          curpos_t const x = b.m_lines[iline].extract_selection(line_data);
          if (line_data.size() || (y1 <= iline && iline <= y2)) {
            if (started && skipped_line_count)
              lines.resize(lines.size() + skipped_line_count, std::make_pair(0, std::u32string()));
            if (line_data.size() && (min_x < 0 || x < min_x)) min_x = x;
            lines.emplace_back(std::make_pair(x, line_data));
            skipped_line_count = 0;
            started = true;
          } else
            skipped_line_count++;
        }
      }

      data.clear();
      bool isfirst = true;
      for (auto& [x, line_data] : lines) {
        // Note: OS 依存の CR LF への変換はもっと上のレイヤーで行う。
        if (isfirst)
          isfirst = false;
        else
          data.append(1, U'\n');
        if (min_x < x) data.append(x - min_x, U' ');
        data.append(line_data);
      }
    }

    void selection_extract_characters(std::u32string& data) {
      using namespace contra::ansi;
      board_t const& b = app().board();
      curpos_t const nline = b.m_lines.size();

      // 開始点と範囲
      curpos_t x1 = -1, x2 = -1, y1 = -1, y2 =-1;
      if (m_sel_type) {
        x1 = m_sel_beg_x;
        y1 = m_sel_beg_y;
        y2 = m_sel_end_y;
        x2 = m_sel_end_x;
        if (y1 < nline) x1 = b.to_data_position(y1, x1);
        if (y2 < nline) x2 = b.to_data_position(y2, x2);
        if (y1 > y2) {
          std::swap(y1, y2);
          std::swap(x1, x2);
        } else if (y1 == y2 && x1 > x2) {
          std::swap(x1, x2);
        }
      }

      std::u32string result;
      {
        bool started = false;
        curpos_t skipped_line_count = 0;
        std::u32string line_data;
        for (curpos_t iline = 0; iline < nline; iline++) {
          curpos_t x = b.m_lines[iline].extract_selection(line_data);
          if (line_data.size() || (y1 <= iline && iline <= y2)) {
            if (started) result.append(skipped_line_count + 1, U'\n');
            //if (iline == y1 && x >= x1) x = 0;
            if (!started) x = 0;
            if (x) result.append(x, U' ');
            result.append(line_data);
            skipped_line_count = 0;
            started = true;
          } else
            skipped_line_count++;
        }
      }
      if (!result.empty()) data.swap(result);
    }

    void selection_extract(std::u32string& data) {
      if (m_sel_type & contra::ansi::modifier_meta)
        selection_extract_rectangle(data);
      else
        selection_extract_characters(data);
    }

  private:
    std::u32string m_clipboard_data;
    void clipboard_paste() {
      if (m_events) m_events->get_clipboard(m_clipboard_data);
      app().input_paste(m_clipboard_data);
    }
    void clipboard_copy() {
      selection_extract(m_clipboard_data);
      if (m_events) m_events->set_clipboard(m_clipboard_data);
    }

  public:
    bool m_dirty = false;
  private:
    class manager_mouse_events: public mouse_events {
      terminal_manager* manager;
    public:
      manager_mouse_events(terminal_manager* m): manager(m) {}
    private:
      virtual void on_select_initialize(key_t key, curpos_t x, curpos_t y) override {
        if (key == contra::ansi::key_mouse1_down)
          manager->selection_initialize(x, y);
      }
      virtual bool on_select_update(key_t key, curpos_t x, curpos_t y) override {
        return key == contra::ansi::key_mouse1_drag && manager->selection_update(key, x, y);
      }
      virtual void on_select(key_t key, curpos_t x, curpos_t y) {
        contra_unused(key);
        contra_unused(x);
        contra_unused(y);
        manager->do_select();
      }
      virtual void on_click(key_t key, curpos_t x, curpos_t y) {
        contra_unused(x);
        contra_unused(y);
        switch (key & contra::ansi::_key_mouse_button_mask) {
        case contra::ansi::_key_mouse1:
          manager->do_click(key);
          break;
        case contra::ansi::_key_mouse3:
          manager->do_right_click(key);
          break;
        }
      }
      virtual void on_multiple_click(key_t key, curpos_t x, curpos_t y, int count) {
        manager->do_multiple_click(key, count, x, y);
      }
    };

    manager_mouse_events m_mouse_events {this};
    mouse_event_detector m_mouse_detector {&this->m_mouse_events};
    void do_click(key_t key) { contra_unused(key); }
    void do_select() { this->clipboard_copy(); }
    void do_multiple_click(key_t key, int count, curpos_t x, curpos_t y) {
      using namespace contra::ansi;
      if (key == key_mouse1_down) {
        board_t& b = app().board();
        curpos_t const nline = b.m_height;
        bool const gatm = app().state().get_mode(mode_gatm);
        for (curpos_t y1 = 0; y1 < nline; y1++) {
          line_t& line = b.m_lines[y1];
          if (y1 == y) {
            word_selection_type wtype;
            switch (count) {
            case 2:
              wtype = word_selection_cword;
              goto set_selection_word;
            case 3:
              wtype = word_selection_sword;
              goto set_selection_word;
            set_selection_word:
              m_dirty |= line.set_selection_word(b.to_data_position(y1, x), wtype, gatm);
              break;
            default:
              m_dirty |= line.set_selection(0, b.m_width + 1, key & modifier_shift, gatm, true);
              break;
            }
          } else
            m_dirty |= line.clear_selection();
        }
        if (y < nline) clipboard_copy();
      }
    }
    void do_right_click(key_t key) {
      if (key == contra::ansi::key_mouse3_up)
        this->clipboard_paste();
    }

  private:
    coord_t m_zoom_xpixel0 = 7;
    coord_t m_zoom_ypixel0 = 13;
    int m_zoom_level_min = 0;
    int m_zoom_level_max = 0;
    int m_zoom_level = 0;
    static constexpr double zoom_ratio = 1.05;

    static double calculate_zoom(coord_t base, int zoom_level) {
      return std::ceil(base * std::pow(zoom_ratio, zoom_level) + zoom_level);
    }
    void initialize_zoom(coord_t xpixel = -1, coord_t ypixel = -1) {
      if (xpixel >= 0) m_zoom_xpixel0 = std::clamp(xpixel, limit::minimal_terminal_xpixel, limit::maximal_terminal_xpixel);
      if (ypixel >= 0) m_zoom_ypixel0 = std::clamp(ypixel, limit::minimal_terminal_ypixel, limit::maximal_terminal_ypixel);

      coord_t const ypixel_min_from_xrange = contra::ceil_div((limit::minimal_terminal_xpixel - 1) * m_zoom_ypixel0 + 1, m_zoom_xpixel0);
      coord_t const ypixel_max_from_xrange = limit::maximal_terminal_xpixel * m_zoom_ypixel0 / m_zoom_xpixel0;
      coord_t const ypixel_min = std::max(limit::minimal_terminal_ypixel, ypixel_min_from_xrange);
      coord_t const ypixel_max = std::min(limit::maximal_terminal_ypixel, ypixel_max_from_xrange);
      m_zoom_level_min = std::ceil(std::log((double) ypixel_min / m_zoom_ypixel0) / std::log(zoom_ratio));
      m_zoom_level_max = std::ceil(std::log((double) ypixel_max / m_zoom_ypixel0) / std::log(zoom_ratio));
      while (calculate_zoom(m_zoom_ypixel0, m_zoom_level_max) > ypixel_max) m_zoom_level_max--;
      while (calculate_zoom(m_zoom_ypixel0, m_zoom_level_min) < ypixel_min) m_zoom_level_min++;
      this->update_zoom(0);
    }
    void update_zoom(int zoom_level) {
      m_zoom_level = std::clamp(zoom_level, m_zoom_level_min, m_zoom_level_max);
      coord_t const ypixel_zoom = calculate_zoom(m_zoom_ypixel0, m_zoom_level);
      coord_t const xpixel_zoom = contra::ceil_div(ypixel_zoom * m_zoom_xpixel0, m_zoom_ypixel0);
      coord_t const ypixel = std::clamp(ypixel_zoom, limit::minimal_terminal_ypixel, limit::maximal_terminal_ypixel);
      coord_t const xpixel = std::clamp(xpixel_zoom, limit::minimal_terminal_xpixel, limit::maximal_terminal_xpixel);
      if (m_events) m_events->request_change_size(-1, -1, xpixel, ypixel);
    }

  public:
    bool input_mouse(key_t key, [[maybe_unused]] coord_t px, [[maybe_unused]] coord_t py, curpos_t x, curpos_t y) {
      if (app().input_mouse(key, px, py, x, y)) return true;

      using namespace contra::ansi;

      switch (key & _character_mask) {
      case key_wheel_down:
        if ((key & _modifier_mask) == modifier_control) {
          this->update_zoom(m_zoom_level - 1);
          return true;
        }
        break;
      case key_wheel_up:
        if ((key & _modifier_mask) == modifier_control) {
          this->update_zoom(m_zoom_level + 1);
          return true;
        }
        break;
      default:
        return m_mouse_detector.input_mouse(key, px, py, x, y);
      }

      return false;
    }
    void reset_size(curpos_t width, curpos_t height) {
      this->m_width = width;
      this->m_height = height;
      app().reset_size(width, height);
    }
    void reset_size(curpos_t width, curpos_t height, coord_t xpixel, coord_t ypixel) {
      this->m_width = width;
      this->m_height = height;
      this->m_xpixel = xpixel;
      this->m_ypixel = ypixel;
      app().reset_size(width, height, xpixel, ypixel);
    }

  };
}
}
#endif
