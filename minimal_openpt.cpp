#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include <stdio.h>
#include <time.h>
#include <cstdlib>

void msleep(int milliseconds) {
  struct timespec tv;
  tv.tv_sec = 0;
  tv.tv_nsec = milliseconds * 1000000;
  nanosleep(&tv, NULL);
}

void set_fd_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    perror("sample_openpt (fcntl(0, F_GETFL))");
    exit(1);
  }

  if (flags & O_NONBLOCK) return;

  int const result = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (result < 0) {
    perror("sample_openpt (fcntl(0, F_SETFL))");
    exit(1);
  }
}

bool transfer_stream_data(int fdsrc, int fddst, char* buff, std::size_t size, int millisleep) {
  int const nread = read(fdsrc, buff, size);
  if (nread <= 0) return false;

  int nrest = nread;
  const char* p = buff;
  for (; nrest > 0; ) {
    int const n = write(fddst, p, nrest);
    if (n > 0) {
      p += n;
      nrest -= n;
    } else
      msleep(millisleep);
  }

  return true;
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

int main() {
  struct termios termios;
  struct winsize winsize;
  tcgetattr(STDIN_FILENO, &termios);
  ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&winsize);

  int masterfd, slavefd;
  char const* slavedevice;
  if ((masterfd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0
    || grantpt(masterfd) < 0
    || unlockpt(masterfd) < 0
    || (slavedevice = ptsname(masterfd)) == NULL
    || (slavefd = open(slavedevice, O_RDWR | O_NOCTTY)) < 0)
    exit(1);

  pid_t const pid = fork();
  if (pid < 0) exit(1);

  if (pid == 0) {
    setsid();
    tcsetattr(slavefd, TCSANOW, &termios);
    ioctl(slavefd, TIOCSWINSZ, &winsize);

    dup2(slavefd, STDIN_FILENO);
    dup2(slavefd, STDOUT_FILENO);
    dup2(slavefd, STDERR_FILENO);
    close(slavefd);
    close(masterfd);
    execl("/bin/bash", "/bin/bash", NULL);
    perror("sample_openpt (exec(\"/bin/bash\"))");
    _exit(1);
  } else {
    close(slavefd);

    struct termios oldTermios = termios;
    termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    termios.c_cflag &= ~(CSIZE | PARENB);
    termios.c_cflag |= CS8;
    termios.c_oflag &= ~(OPOST);
    termios.c_cc[VMIN]  = 1;
    termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios);

    set_fd_nonblock(STDIN_FILENO);

    char buff[4096];
    for (;;) {
      if (transfer_stream_data(masterfd, STDOUT_FILENO, buff, sizeof(buff), 1)) continue;
      if (transfer_stream_data(STDIN_FILENO, masterfd, buff, sizeof(buff), 1)) continue;
      if (is_child_terminated(pid)) break;
      msleep(10);
    }

    kill(pid, SIGTERM);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTermios);
  }

  return 0;
}
