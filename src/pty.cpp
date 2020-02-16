#ifdef __CYGWIN__
# define _XOPEN_SOURCE 600
#endif
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pwd.h> /* for getuid, getpwuid */

#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>

#include "pty.hpp"
#include "contradef.hpp"
#include "manager.hpp"

namespace contra {
namespace term {

  void msleep(int milliseconds) {
    struct timespec tv;
    tv.tv_sec = 0;
    tv.tv_nsec = milliseconds * 1000000;
    nanosleep(&tv, NULL);
  }
  void usleep(int usec) {
    struct timespec tv;
    tv.tv_sec = 0;
    tv.tv_nsec = usec * 1000;
    nanosleep(&tv, NULL);
  }

  std::size_t read_from_fd(int fdsrc, contra::idevice* dst, char* buff, std::size_t size) {
    char* p = buff;
    ssize_t nread;
    while ((nread = ::read(fdsrc, p, size)) > 0) {
      size -= nread;
      p += nread;
    }
    //if(nread < 0 && errno != EAGAIN) fdclosed?;
    if (p != buff) dst->dev_write(buff, p - buff);
    return p - buff;
  }

  bool fd_set_nonblock(int fd, bool value) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
      perror("contra::term (fd_set_nonblock/fcntl(0, F_GETFL))");
      exit(1);
    }

    bool const old_status = flags & O_NONBLOCK;
    if (old_status == value) return old_status;

    int const result = fcntl(fd, F_SETFL, flags ^ O_NONBLOCK);
    if (result < 0) {
      perror("contra::term (fd_set_nonblock/fcntl(0, F_SETFL))");
      exit(1);
    }
    return old_status;
  }
  void fd_set_winsize(int fd, struct winsize const* ws) {
    if (ws) ioctl(fd, TIOCSWINSZ, ws);
  }

  void fd_device::write(char const* data, std::size_t size) const {
    const char* p = data;
    if (!size) return;
    for (;;) {
      ssize_t const sz_write = (ssize_t) std::min<std::size_t>(size, SSIZE_MAX);
      ssize_t const n = ::write(m_fd, p, sz_write);
      if (n < 0 && errno != EAGAIN) return;
      if (n > 0) {
        if ((std::size_t) n >= size) break;
        p += n;
        size -= n;
      } else
        msleep(m_write_interval);
    }
  }

  class pty_connection: public fd_device {
  private:
    bool m_active = false;
    int slave_pid = -1;
    std::vector<char> m_read_buffer;
    struct winsize ws;
  public:
    pty_connection() {}
    pty_connection(terminal_session_parameters const& params) {
      start(params);
    }

    bool start(terminal_session_parameters const& params) {
      if (m_active) return true;

      // restrict in the range from 256 bytes to 64 kbytes
      m_read_buffer.resize(contra::clamp(params.fd_read_buffer_size, 0x100, 0x10000));

      this->slave_pid = -1;
      this->m_fd = -1;
      this->m_active = false;

      int masterfd, slavefd;
      char const* slavedevice;
      if ((masterfd = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0
        || ::grantpt(masterfd) < 0
        || ::unlockpt(masterfd) < 0
        || (slavedevice = ::ptsname(masterfd)) == NULL
        || (slavefd = ::open(slavedevice, O_RDWR | O_NOCTTY)) < 0)
        return false;

      pid_t const pid = ::fork();
      if (pid < 0) return false;

      if (pid == 0) {
        setsid();
        ioctl(slavefd, TIOCSCTTY, 0);
        if (params.termios) tcsetattr(slavefd, TCSANOW, params.termios);
        {
          ws.ws_col = params.col;
          ws.ws_row = params.row;
          ws.ws_xpixel = params.xunit * params.col;
          ws.ws_ypixel = params.yunit * params.row;
          fd_set_winsize(slavefd, &ws);
        }
        close(masterfd);
        dup2(slavefd, STDIN_FILENO);
        dup2(slavefd, STDOUT_FILENO);
        dup2(slavefd, STDERR_FILENO);
        if (slavefd != STDIN_FILENO &&
          slavefd != STDOUT_FILENO &&
          slavefd != STDERR_FILENO) close(slavefd);

        for (auto const& pair : params.env) {
          ::setenv(pair.first.c_str(), pair.second.c_str(), 1);
          if (pair.first == "HOME")
            ::chdir(pair.second.c_str());
        }

        //execl(shell, shell, "-l", NULL);
        const char* shell = params.shell.c_str();
        execl(shell, shell, NULL);

        // exec 失敗時
        if (params.exec_error_handler)
          params.exec_error_handler(errno, params.exec_error_param);
        else
          perror("contra::term (exec SHELL)");
        _exit(1);
        return false;
      } else {
        close(slavefd);
        this->slave_pid = pid;
        this->m_fd = masterfd;
        this->m_active = true;
        return true;
      }
    }

    bool is_active() const { return m_active; }

  public:
    void set_winsize(curpos_t col, curpos_t row, curpos_t xunit, curpos_t yunit) {
      if (ws.ws_col == col && ws.ws_row == row &&
        ws.ws_xpixel / ws.ws_col == xunit && ws.ws_ypixel / ws.ws_row == yunit) return;
      ws.ws_col = col;
      ws.ws_row = row;
      ws.ws_xpixel = xunit * col;
      ws.ws_ypixel = yunit * row;
      fd_set_winsize(m_fd, &ws);
    }

    std::size_t read(contra::idevice* dst) {
      if (!m_active) return 0;
      return read_from_fd(m_fd, dst, &m_read_buffer[0], m_read_buffer.size());
    }

    bool is_alive() {
      if (!m_active) return false;

      int status;
      pid_t const result = waitpid(slave_pid, &status, WNOHANG);
      if (result < 0) {
        perror("contra::term (pty_connection::is_alive/waitpid)");
        exit(1);
      }

      if (result == 0)
        return true;
      else
        return m_active = false;
    }

    void terminate() {
      // ToDo: 間違って新しく生成した別のプロセスを殺したりしないのか?
      if (m_active) {
        m_active = false;
        ::kill(slave_pid, SIGTERM);
      }
    }
  };

  class terminal_session: public terminal_application {
    typedef terminal_application base;
  private:
    contra::term::pty_connection m_pty; // ユーザ入力書込先
    contra::multicast_device m_dev;  // 受信データ書込先
  public:
    contra::idevice& input_device() { return m_pty; }
    contra::multicast_device& output_device() { return m_dev; }

  private:
    std::unique_ptr<contra::term::fd_device> m_dev_tee;
  public:
    // これはテストの為に受信したデータをその儘画面に流すための設定。
    void setup_fd_tee(int fd) {
      if (m_dev_tee) return;
      m_dev_tee = std::make_unique<contra::term::fd_device>(fd);
      m_dev.push(m_dev_tee.get());
    }

  private:
    std::unique_ptr<contra::sequence_printer> m_dev_seq;
  public:
    // これはテストの為に受信したシーケンスをファイルに出力する設定。
    void setup_sequence_log(const char* fname) {
      if (m_dev_seq) return;
      m_dev_seq = std::make_unique<contra::sequence_printer>(fname);
      m_dev.push(m_dev_seq.get());
    }

  public:
    bool initialize(terminal_session_parameters const& params) {
      if (m_pty.is_active()) return true;

      if (!base::initialize(params.col, params.row, params.xunit, params.yunit)) return false;

      if (!m_pty.start(params)) return false;

      base::term().set_input_device(m_pty);
      m_dev.push(&base::term());
      base::term().set_scroll_capacity(params.scroll_buffer_size);

      // for diagnostics
      if (params.dbg_fd_tee)
        setup_fd_tee(params.dbg_fd_tee);
      if (params.dbg_sequence_logfile)
        setup_sequence_log(params.dbg_sequence_logfile);

      return true;
    }
    virtual bool process() override { return m_pty.read(&m_dev); }
    virtual bool is_active() const override { return m_pty.is_active(); }
    virtual bool is_alive() override { return m_pty.is_alive(); }
    virtual void terminate() override { return m_pty.terminate(); }

  public:
    virtual void reset_size(curpos_t width, curpos_t height, coord_t xunit, coord_t yunit) override {
      base::reset_size(width, height, xunit, yunit);
      if (m_pty.is_active())
        m_pty.set_winsize(width, height, xunit, yunit);
    }
  };

  std::unique_ptr<terminal_application> create_terminal_session(terminal_session_parameters& params) {
    // buffer size の設定 (Note: 64kb 読み取って処理するのに 30ms かかった @ mag)
    params.fd_read_buffer_size = 16 * 1024;

    // 環境変数の設定
    struct passwd* pw = ::getpwuid(::getuid());
    if (pw) {
      if (pw->pw_name)
        params.env["USER"] = pw->pw_name;
      if (pw->pw_dir)
        params.env["HOME"] = pw->pw_dir;
      if (pw->pw_shell) {
        params.env["SHELL"] = pw->pw_shell;
        if (params.shell.empty()) params.shell = pw->pw_shell;
      }
    }
    if (params.shell.empty()) params.shell = "/bin/sh";

    if (params.env.find("TERM") == params.env.end())
      params.env["TERM"] = "xterm-256color";

    std::string path = "/usr/local/bin:/usr/bin:/bin";
    if (char const* original_path = std::getenv("PATH"); original_path && *original_path) {
      path += ":";
      path += original_path;
    }
    params.env["PATH"] = std::move(path);

    char hostname[256];
    if (::gethostname(hostname, sizeof hostname) == 0)
      params.env["HOSTNAME"] = hostname;

    auto sess = std::make_unique<terminal_session>();
    if (!sess->initialize(params)) {
      sess.reset();
      return sess;
    }

    return sess;
  }

}
}
