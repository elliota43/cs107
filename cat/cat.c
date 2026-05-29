#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_FILES 25

#define BUF_SIZE 4096

bool open_file(int *fds, char *filename);

void read_file(int fd);

int main(int argc, char *argv[]) {

  int fds[MAX_FILES];

  int num_fds = 0;

  if (argc < 2) {

    read_file(STDIN_FILENO);
  } else {

    for (int i = 0; i < argc - 1; i++) {

      if (strcmp(argv[i + 1], "-") == 0) {
        read_file(STDIN_FILENO);
      } else {
        if (open_file(&fds[i], argv[i + 1]))
          num_fds++;
        else {
          fprintf(stderr, "cat: %s. No such file or directory.\n", argv[i + 1]);
          exit(EXIT_FAILURE);
        }
      }
    }

    for (int i = 0; i < num_fds; i++) {
      read_file(fds[i]);
    }
  }
}

bool open_file(int *fds, char *filename) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1)
    return false;

  fds[0] = fd;
  return true;
}

void read_file(int fd) {
  char buf[BUF_SIZE];
  ssize_t bytes_read;
  size_t total_read = 0;

  while ((bytes_read = read(fd, buf, sizeof(buf) - 1)) > 0) {
    total_read += bytes_read;

    write(STDOUT_FILENO, buf, bytes_read);
  }

  if (bytes_read == -1) {
    fprintf(stderr, "error reading file: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
}
