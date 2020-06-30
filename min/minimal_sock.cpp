#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <iterator>
#include <vector>
#include <poll.h>

#include <cstdint>
#include <time.h>
void msleep(std::int64_t msec) {
  timespec tv { msec / 1000, msec % 1000 * 1000000};
  while (nanosleep(&tv, &tv) == -1 && errno == EINTR);
}

void server(const char* filename) {
  // socket(2) でソケット生成
  int s = socket(AF_UNIX, SOCK_STREAM, 0);
  if (s == -1) {
    std::cerr << "minimal_sock: failed to create a socket" << std::endl;
    perror("minimal_sock");
    std::exit(1);
  }

  // bind(2) でファイルを作成。
  sockaddr_un addr { AF_UNIX };
  if (std::strlen(filename) + 1 > sizeof addr.sun_path) {
    std::cerr << "minimal_sock: socket name too long" << std::endl;
    std::exit(1);
  }
  std::strcpy(addr.sun_path, filename);
  std::remove(filename);
  if (bind(s, reinterpret_cast<sockaddr const*>(&addr), sizeof addr)) {
    std::cerr << "minimal_sock: failed to bind socket" << std::endl;
    perror("minimal_sock");
    std::exit(1);
  }

  // listen(2) で接続待ち状態に設定する。
  if (listen(s, SOMAXCONN)) {
    std::cerr << "minimal_sock: failed to start listening" << std::endl;
    perror("minimal_sock");
    std::exit(1);
  }

  // accept(2)
  pollfd fds[1];
  fds[0].fd = s;
  fds[0].events = POLLIN;
  fds[0].revents = 0;
  int result;
  while ((result = poll(&fds[0], (nfds_t) std::size(fds), 0)) <= 0) {
    if (result == -1) {
      std::cerr << "minimal_sock: failed while listening" << std::endl;
      perror("minimal_sock");
      std::exit(1);
    } else {
      std::cerr << "server: wait..." << std::endl;
      msleep(50);
    }
  }

  int fd;
  while ((fd = accept(s, NULL, NULL)) == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      msleep(50);
    } else {
      std::cerr << "minimal_sock: failed while listening" << std::endl;
      perror("minimal_sock");
      std::exit(1);
    }
  }

  std::cout << "server: Connect!" << std::endl;
  const char* msg = "\x1b[1;34mserver_message\x1b[m";
  std::size_t len = std::strlen(msg) + 1;
  write(fd, &len, sizeof len);
  write(fd, msg, len);

  close(fd);

  close(s);
  std::remove(filename);
}

void client(const char* filename) {
  // socket(2) でソケット生成
  int s = socket(AF_UNIX, SOCK_STREAM, 0);
  if (s == -1) {
    std::cerr << "minimal_sock: failed to create a socket" << std::endl;
    perror("minimal_sock");
    std::exit(1);
  }

  // bind(2) でファイルを作成。
  sockaddr_un addr { AF_UNIX };
  if (std::strlen(filename) + 1 > sizeof addr.sun_path) {
    std::cerr << "minimal_sock: socket name too long" << std::endl;
    std::exit(1);
  }
  std::strcpy(addr.sun_path, filename);
  if (connect(s, reinterpret_cast<sockaddr const*>(&addr), sizeof addr)) {
    std::cerr << "minimal_sock: failed to connect to socket" << std::endl;
    perror("minimal_sock");
    std::exit(1);
  }

  std::cout << "client: Connect!" << std::endl;
  std::size_t len;
  read(s, &len, sizeof len);
  std::vector<char> data(len);
  read(s, &data[0], len);
  std::cout << "client: received " << &data[0] << std::endl;

  close(s);
}

int main() {
  const char* filename = "a.sock";

  pid_t pid = fork();
  if (pid == -1) {
    std::cerr << "minimal_sock: failed to fork" << std::endl;
    return 1;
  } else if (pid == 0) {
    setsid();
    signal(SIGHUP, SIG_IGN);
    server(filename);
    std::exit(0);
  } else {
    msleep(200);
    client(filename);
    _exit(0);
  }

  return 0;
}
