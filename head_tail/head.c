
#include <sys/fcntl.h>
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUF_SIZE 65536

enum count_mode { COUNT_LINES, COUNT_BYTES };
enum header_mode { HEADER_AUTO, HEADER_QUIET, HEADER_VERBOSE };

typedef struct cli_options {
  enum header_mode header;
  enum count_mode mode;
  size_t count;
  bool from_end;
  bool zero_term;
} cli_options;

static void write_file_to_stdout(int fd, const char *name,
                                 const cli_options *opts);

static void parse_opt_args(int argc, char **argv, cli_options *opts);

int main(int argc, char *argv[]) {
  cli_options opts = {
      .mode = COUNT_LINES,
      .header = HEADER_AUTO,
      .count = 10,
      .from_end = false,
      .zero_term = false,
  };

  parse_opt_args(argc, argv, &opts);

  if (optind >= argc) {
    write_file_to_stdout(STDIN_FILENO, "standard input", &opts);
    return fflush(stdout) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  int nfiles = argc - optind;
  bool print_headers = opts.header == HEADER_VERBOSE ||
                       (opts.header == HEADER_AUTO && nfiles > 1);

  int status = EXIT_SUCCESS;
  bool first = true;
  for (int i = optind; i < argc; i++) {
    const char *name = argv[i];
    int fd;

    if (strcmp(name, "-") == 0) {
      fd = STDIN_FILENO;
      name = "standard input";
    } else {
      fd = open(name, O_RDONLY);
      if (fd == -1) {
        fprintf(stderr, "head: cannot open '%s' for reading: %s\n", name,
                strerror(errno));
        status = EXIT_FAILURE;
        continue;
      }
    }

    if (print_headers)
      printf("%s==> %s <==\n", first ? "" : "\n", name);

    write_file_to_stdout(fd, name, &opts);

    if (fd != STDIN_FILENO)
      close(fd);
    first = false;
  }

  if (fflush(stdout) != 0)
    status = EXIT_FAILURE;
  return status;
}

static ssize_t read_retry(int fd, void *buf, size_t n) {
  ssize_t r;
  do {
    r = read(fd, buf, n);
  } while (r < 0 && errno == EINTR);
  return r;
}

static void read_error(const char *name) {
  fprintf(stderr, "head: error reading '%s': %s\n", name, strerror(errno));
}

static void copy_first_bytes(int fd, size_t count, const char *name) {
  char buf[BUF_SIZE];
  while (count > 0) {
    size_t want = count < BUF_SIZE ? count : BUF_SIZE;
    ssize_t n = read_retry(fd, buf, want);
    if (n < 0) {
      read_error(name);
      return;
    }
    if (n == 0)
      break;
    fwrite(buf, 1, (size_t)n, stdout);
    count -= (size_t)n;
  }
}

static void copy_first_lines(int fd, size_t count, char delim,
                             const char *name) {
  if (count == 0)
    return;
  char buf[BUF_SIZE];
  ssize_t n;
  while ((n = read_retry(fd, buf, BUF_SIZE)) > 0) {
    for (ssize_t i = 0; i < n; i++) {
      if (buf[i] == delim && --count == 0) {
        fwrite(buf, 1, (size_t)(i + 1), stdout);
        return;
      }
    }
    fwrite(buf, 1, (size_t)n, stdout);
  }

  if (n < 0)
    read_error(name);
}

static void copy_all_but_last_bytes(int fd, size_t count, const char *name) {
  char buf[BUF_SIZE];
  ssize_t n;

  if (count == 0) {
    while ((n = read_retry(fd, buf, BUF_SIZE)) > 0)
      fwrite(buf, 1, (size_t)n, stdout);
    if (n < 0)
      read_error(name);
    return;
  }

  char *ring = malloc(count);
  if (!ring) {
    fprintf(stderr, "head: out of memory\n");
    exit(EXIT_FAILURE);
  }

  size_t head = 0, filled = 0;

  while ((n = read_retry(fd, buf, BUF_SIZE)) > 0) {
    for (ssize_t i = 0; i < n; i++) {
      if (filled < count) {
        ring[(head + filled) % count] = buf[i];
        filled++;
      } else {
        putchar(ring[head]);
        ring[head] = buf[i];
        head = (head + 1) % count;
      }
    }
  }
  if (n < 0)
    read_error(name);
  free(ring);
}

typedef struct {
  char *data;
  size_t len;
} line_t;

static void ring_push(line_t *ring, size_t count, size_t *head, size_t *filled,
                      char *data, size_t len) {
  if (*filled == count) {
    line_t *old = &ring[*head];
    fwrite(old->data, 1, old->len, stdout);
    free(old->data);
    old->data = data;
    old->len = len;
    *head = (*head + 1) % count;
  } else {
    size_t idx = (*head + *filled) % count;
    ring[idx].data = data;
    ring[idx].len = len;
    (*filled)++;
  }
}

static void copy_all_but_last_lines(int fd, size_t count, char delim,
                                    const char *name) {
  char buf[BUF_SIZE];
  ssize_t n;

  if (count == 0) {
    while ((n = read_retry(fd, buf, BUF_SIZE)) > 0)
      fwrite(buf, 1, (size_t)n, stdout);
    if (n < 0)
      read_error(name);
    return;
  }

  line_t *ring = calloc(count, sizeof *ring);
  if (!ring) {
    fprintf(stderr, "head: out of memory\n");
    exit(EXIT_FAILURE);
  }
  size_t head = 0, filled = 0;

  char *cur = NULL;
  size_t cur_len = 0, cur_cap = 0;

  while ((n = read_retry(fd, buf, BUF_SIZE)) > 0) {
    for (ssize_t i = 0; i < n; i++) {
      if (cur_len == cur_cap) {
        cur_cap = cur_cap ? cur_cap * 2 : 128;
        char *tmp = realloc(cur, cur_cap);
        if (!tmp) {
          fprintf(stderr, "head: out of memory\n");
          exit(EXIT_FAILURE);
        }
        cur = tmp;
      }

      cur[cur_len++] = buf[i];
      if (buf[i] == delim) {
        ring_push(ring, count, &head, &filled, cur, cur_len);
        cur = NULL;
        cur_len = cur_cap = 0;
      }
    }
  }

  if (n < 0)
    read_error(name);

  if (cur_len > 0)
    ring_push(ring, count, &head, &filled, cur, cur_len);
  else
    free(cur);

  for (size_t k = 0; k < filled; k++)
    free(ring[(head + k) % count].data);
  free(ring);
}

static void write_file_to_stdout(int fd, const char *name,
                                 const cli_options *opts) {
  char delim = opts->zero_term ? '\0' : '\n';
  if (opts->mode == COUNT_BYTES) {
    if (opts->from_end)
      copy_all_but_last_bytes(fd, opts->count, name);
    else
      copy_first_bytes(fd, opts->count, name);
  } else {
    if (opts->from_end)
      copy_all_but_last_lines(fd, opts->count, delim, name);
    else
      copy_first_lines(fd, opts->count, delim, name);
  }
}

static void parse_opt_args(int argc, char **argv, cli_options *opts) {
  static struct option long_options[] = {
      {"bytes", required_argument, 0, 'c'},
      {"lines", required_argument, 0, 'n'},
      {"quiet", no_argument, 0, 'q'},
      {"silent", no_argument, 0, 'q'},
      {"verbose", no_argument, 0, 'v'},
      {"zero-terminated", no_argument, 0, 'z'},
      {0, 0, 0, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, "c:n:qvz", long_options, NULL)) != -1) {
    switch (opt) {
    case 'c':
    case 'n': {
      opts->mode = (opt == 'c') ? COUNT_BYTES : COUNT_LINES;
      char *endptr;
      long val = strtol(optarg, &endptr, 10);
      if (endptr == optarg || *endptr != '\0') {
        fprintf(stderr, "head: invalid number of %s: '%s'\n",
                (opt == 'c') ? "bytes" : "lines", optarg);
        exit(EXIT_FAILURE);
      }
      if (optarg[0] == '-') {
        opts->from_end = true;
        opts->count = (size_t)(-val);
      } else {
        opts->from_end = false;
        opts->count = (size_t)val;
      }
      break;
    }
    case 'q':
      opts->header = HEADER_QUIET;
      break;
    case 'v':
      opts->header = HEADER_VERBOSE;
      break;
    case 'z':
      opts->zero_term = true;
      break;
    default:
      fprintf(stderr,
              "usage: %s [-n lines | -c bytes] [-q] [-v] [-z] [file...]\n",
              argv[0]);
      exit(EXIT_FAILURE);
    }
  }
}
