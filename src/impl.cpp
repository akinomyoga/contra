#include "impl.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

namespace contra {

void msleep(int milliseconds) {
  struct timespec tv;
  tv.tv_sec = 0;
  tv.tv_nsec = milliseconds * 1000000;
  nanosleep(&tv, NULL);
}

bool read_from_fd(int fdsrc, contra::idevice* dst, char* buff, std::size_t size) {
  ssize_t const nread = read(fdsrc, buff, size);
  if (nread <= 0) return false;
  dst->write(buff, nread);
  return true;
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

bool is_child_terminated(pid_t pid) {
  int status;
  pid_t const result = waitpid(pid, &status, WNOHANG);
  if (result < 0) {
    perror("sample_openpt (waitpid)");
    exit(1);
  }
  return result > 0;
}

bool create_session(session* sess, struct termios& termios, const char* shell, int cols, int rows) {
  sess->pid = -1;
  sess->masterfd = -1;

  struct winsize winsize;
  ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &winsize);
  if (cols > 0) winsize.ws_col = cols;
  if (rows > 0) winsize.ws_row = rows;

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
    tcsetattr(slavefd, TCSANOW, &termios);
    ioctl(slavefd, TIOCSWINSZ, &winsize);

    dup2(slavefd, STDIN_FILENO);
    dup2(slavefd, STDOUT_FILENO);
    dup2(slavefd, STDERR_FILENO);
    close(slavefd);
    close(masterfd);
    //execl(shell, shell, "--norc", NULL);
    execl(shell, shell, NULL);
    perror("sample_openpt (exec(SHELL))");
    _exit(1);
  } else {
    close(slavefd);
    sess->pid = pid;
    sess->masterfd = masterfd;
    return true;
  }
}

}