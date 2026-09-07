#ifndef IR_OPT_COST_H
#define IR_OPT_COST_H

typedef struct {
  int op;
  int load;
  int store;
  int branch;
  int multiply;
  int multiply_float;
  int divide;
  int divide_float;
  int call;
  int allocate;
  int vector_width;
  int subgroup_width;
} IROptCost;

const IROptCost *ir_opt_cost(void);
void ir_opt_cost_describe(const IROptCost *cost);
void ir_opt_cost_reset(void);

#endif
