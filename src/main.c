#include <stdio.h>

#include "../include/cli.h"

int main(int argc, char** argv) {
  setvbuf(stdout, NULL, _IOLBF, 0);   /* line-buffered so parent/child output interleaves readably */
  return cliRun(argc, argv);
}
