
#include "ir/ml_obs.h"

#include <stdio.h>

int main(int argc, char **argv) {
  const char *golden = argc > 1 ? argv[1] : "tools/mlopt/obs_golden.txt";
  FILE *probe = fopen(golden, "r");
  if (!probe) {

    printf("RESULT: SKIP (no %s)\n", golden);
    return 0;
  }
  fclose(probe);

  int bad = ml_obs_selftest(golden);
  if (bad != 0) {
    printf("RESULT: FAIL (%d mismatches)\n", bad);
    return 1;
  }
  printf("RESULT: PASS\n");
  return 0;
}
