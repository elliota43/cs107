#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *saveptr;

bool is_delim(char c, const char *delim) {
  for (const char *d = delim; *d != '\0'; d++) {
    if (*d == c)
      return true;
  }
  return false;
}

char *strtok(char *str, const char *delim) {
  char *s = str ? str : saveptr;
  if (!s)
    return NULL;

  while (*s && is_delim(*s, delim))
    s++;
  if (!*s) {
    saveptr = NULL;
    return NULL;
  } // nothing left

  char *token = s;

  while (*s && !is_delim(*s, delim))
    s++;

  if (*s) {
    *s = '\0';
    saveptr = s + 1;
  } else {
    saveptr = NULL;
  }

  return token;
}

int main(void) {

  char gfg[100] = " 1997 Ford E350 ac 3000.00";

  const char s[4] = " ";
  char *tok;

  tok = strtok(gfg, s);

  while (tok != 0) {
    printf("%s, ", tok);

    tok = strtok(0, s);
  }
  return 0;
}
