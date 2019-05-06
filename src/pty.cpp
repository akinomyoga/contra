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

  std::size_t read_from_fd(int fdsrc, contra::idevice* dst, char* buff, std::size_t size) {
    ssize_t const nread = read(fdsrc, buff, size);
    if (nread <= 0) return 0;
    dst->dev_write(buff, nread);
    return nread;
  }

  bool fd_set_nonblock(int fd, bool value) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
      perror("sample_openpt (fcntl(0, F_GETFL))");
      exit(1);
    }

    bool const old_status = flags & O_NONBLOCK;
    if (old_status == value) return old_status;

    int const result = fcntl(fd, F_SETFL, flags ^ O_NONBLOCK);
    if (result < 0) {
      perror("sample_openpt (fcntl(0, F_SETFL))");
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

    std::size_t m_read_buffer_size = 4096;
    std::vector<char> m_read_buffer;

    exec_error_handler_t m_exec_error_handler = NULL;
    std::uintptr_t m_exec_error_param = 0;

    std::unordered_map<std::string, std::string> m_env;
  public:
    pty_connection() {}
    pty_connection(const char* shell, winsize const* ws = NULL, struct termios* termios = NULL) {
      start(shell, ws, termios);
    }

    void set_read_buffer_size(std::size_t value) {
      m_read_buffer_size = contra::clamp(value, 0x100, 0x10000);
    }
    void set_exec_error_handler(exec_error_handler_t handler, std::uintptr_t param) {
      this->m_exec_error_handler = handler;
      this->m_exec_error_param = param;
    }
    void set_environment_variable(const char* name, const char* value)  {
      m_env[name] = value;
    }
    bool start(const char* shell, winsize const* ws = NULL, struct termios* termios = NULL);

    std::size_t read(contra::idevice* dst) {
      if (!m_active) return 0;
      return read_from_fd(m_fd, dst, &m_read_buffer[0], m_read_buffer.size());
    }

    void terminate();

    bool is_active() const { return m_active; }
    bool is_alive();

    virtual ~pty_connection() {}

    void set_winsize(struct winsize const* ws) {
      fd_set_winsize(m_fd, ws);
    }
  };

  bool pty_connection::is_alive() {
    if (!m_active) return false;

    int status;
    pid_t const result = waitpid(slave_pid, &status, WNOHANG);
    if (result < 0) {
      perror("sample_openpt (waitpid)");
      exit(1);
    }

    if (result == 0)
      return true;
    else
      return m_active = false;
  }

  bool pty_connection::start(const char* shell, winsize const* ws, struct termios* termios) {
    if (m_active) return true;

    // restrict in the range from 256 bytes to 64 kbytes
    m_read_buffer.resize(m_read_buffer_size);

    this->slave_pid = -1;
    this->m_fd = -1;
    this->m_active = false;

    int masterfd, slavefd;
    char const* slavedevice;
    if ((masterfd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0
      || grantpt(masterfd) < 0
      || unlockpt(masterfd) < 0
      || (slavedevice = ptsname(masterfd)) == NULL
      || (slavefd = open(slavedevice, O_RDWR | O_NOCTTY)) < 0)
      return false;

    pid_t const pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
      setsid();
      if (termios) tcsetattr(slavefd, TCSANOW, termios);
      if (ws) fd_set_winsize(slavefd, ws);
      dup2(slavefd, STDIN_FILENO);
      dup2(slavefd, STDOUT_FILENO);
      dup2(slavefd, STDERR_FILENO);
      close(slavefd);
      close(masterfd);

      for (auto const& pair : this->m_env) {
        ::setenv(pair.first.c_str(), pair.second.c_str(), 1);
        if (pair.first == "HOME")
          ::chdir(pair.second.c_str());
      }
      //execl(shell, shell, "-l", NULL);
      execl(shell, shell, NULL);

      // exec 失敗時
      if (m_exec_error_handler)
        m_exec_error_handler(errno, m_exec_error_param);
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

  void pty_connection::terminate() {
    if (m_active) {
      m_active = false;
      ::kill(slave_pid, SIGTERM);
    }
  }

  // contra/ttty
  class terminal_session: public terminal_application {
    typedef terminal_application base;
  public:
    struct termios* init_termios = NULL;
    std::size_t init_read_buffer_size = 4096;

    exec_error_handler_t init_exec_error_handler = nullptr;
    std::uintptr_t init_exec_error_param = 0;
  public:
    void init_environment_variable(const char* name, const char* value) {
      m_pty.set_environment_variable(name, value);
    }

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
    bool initialize() {
      if (m_pty.is_active()) return true;

      if (!base::initialize()) return false;

      m_pty.set_read_buffer_size(init_read_buffer_size);
      m_pty.set_exec_error_handler(init_exec_error_handler, init_exec_error_param);
      if (!m_pty.start("/bin/bash", &base::winsize(), init_termios)) return false;

      base::term().set_input_device(m_pty);
      m_dev.push(&base::term());
      return true;
    }
    virtual bool process() override { return m_pty.read(&m_dev); }
    virtual bool is_active() const override { return m_pty.is_active(); }
    virtual bool is_alive() override { return m_pty.is_alive(); }
    virtual void terminate() override { return m_pty.terminate(); }

  public:
    virtual void reset_size(std::size_t width, std::size_t height) override {
      base::reset_size(width, height);
      m_pty.set_winsize(&base::winsize());
    }
  };

  std::unique_ptr<terminal_application> create_terminal_session(terminal_session_parameters const& params) {
    auto sess = std::make_unique<terminal_session>();
    sess->init_size(params.col, params.row, params.xpixel, params.ypixel);
    sess->init_read_buffer_size = 64 * 1024;
    sess->init_exec_error_handler = params.exec_error_handler;
    if (params.termios) sess->init_termios = params.termios;

    // 環境変数
    struct passwd* pw = ::getpwuid(::getuid());
    if (pw) {
      if (pw->pw_name)
        sess->init_environment_variable("USER", pw->pw_name);
      if (pw->pw_dir)
        sess->init_environment_variable("HOME", pw->pw_dir);
      if (pw->pw_shell)
        sess->init_environment_variable("SHELL", pw->pw_shell);
    }
    char hostname[256];
    if (::gethostname(hostname, sizeof hostname) == 0)
      sess->init_environment_variable("HOSTNAME", hostname);
    if (params.env_term)
      sess->init_environment_variable("TERM", params.env_term);
    std::string path = "/usr/local/bin:/usr/bin:/bin";
    path += std::getenv("PATH");
    sess->init_environment_variable("PATH", path.c_str());

    if (!sess->initialize()) sess.reset();

    if (params.dbg_fd_tee)
      sess->setup_fd_tee(params.dbg_fd_tee);
    if (params.dbg_sequence_logfile)
      sess->setup_sequence_log(params.dbg_sequence_logfile);

    return sess;
  }

}
}
