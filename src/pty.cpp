#ifdef __CYGWIN__
# define _XOPEN_SOURCE 600
#endif
#include "pty.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

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

  bool set_fd_nonblock(int fd, bool value) {
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

  bool pty_session::is_alive() {
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

  bool pty_session::start(const char* shell, winsize const* ws, struct termios* termios) {
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
      if (ws) ioctl(slavefd, TIOCSWINSZ, ws);

      dup2(slavefd, STDIN_FILENO);
      dup2(slavefd, STDOUT_FILENO);
      dup2(slavefd, STDERR_FILENO);
      close(slavefd);
      close(masterfd);
      //execl(shell, shell, "--norc", NULL);
      execl(shell, shell, NULL);
      perror("sample_openpt (exec(SHELL))");
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

  void pty_session::terminate() {
    if (m_active) {
      m_active = false;
      ::kill(slave_pid, SIGTERM);
    }
  }

}
}
