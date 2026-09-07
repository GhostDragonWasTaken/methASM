#include "ir_opt_cost.h"

static const IROptCost IR_OPT_COST_DEFAULT = {1, 4, 1, 1, 3, 4, 26, 14, 4, 120, 16, 1};

static IROptCost g_cost = {1, 4, 1, 1, 3, 4, 26, 14, 4, 120, 16, 1};

const IROptCost *ir_opt_cost(void) { return &g_cost; }

void ir_opt_cost_describe(const IROptCost *cost) {
  if (!cost) {
    return;
  }
  g_cost = *cost;
  if (g_cost.vector_width <= 0) {
    g_cost.vector_width = IR_OPT_COST_DEFAULT.vector_width;
  }
  if (g_cost.subgroup_width <= 0) {
    g_cost.subgroup_width = IR_OPT_COST_DEFAULT.subgroup_width;
  }
}

void ir_opt_cost_reset(void) { g_cost = IR_OPT_COST_DEFAULT; }
