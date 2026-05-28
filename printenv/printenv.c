// Assignment:
//
// Write function replicating the functionality of
// printenv(1).
//
// printenv - print all or part of the environment
//
// printenv [OPTION] [VARIABLE]...
//
// Print the value of the specified environment VARIABLE(s).
// If no VARIABLE is specified, print name and value pairs
// for them all.
//

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

extern char **environ;

void print_env(char terminator);

int main(int argc, char *argv[]) {

  int opt;
  char terminator = '\n';

  static struct option long_options[] = {{"null", no_argument, 0, '0'},
                                         {0, 0, 0, 0}};

  while ((opt = getopt_long(argc, argv, "0", long_options, NULL)) != -1) {
    switch (opt) {
    case '0':
      terminator = '\0';
      break;
    case '?':
      return 2;
    default:
      abort();
    }
  }

  if (optind >= argc) {
    print_env(terminator);
    return 0;
  }

  int exit_status = 0;

  for (int i = optind; i < argc; i++) {
    char *key = argv[i];
    char *val = getenv(key);

    if (val != NULL) {
      if (printf("%s%c", val, terminator) < 0) {
        return 2; // write error
      }
    } else {
      // env var not found
      exit_status = 1;
    }
  }

  return exit_status;
}

void print_env(char terminator) {

  int count = 0;

  while (environ[count] != NULL) {
    if (printf("%s%c", environ[count], terminator) < 0)
      exit(2);

    count++;
  }
}
