static void ir_simd_explain_bail(const IRFunction *function, size_t begin,
                                 size_t end, char *reason, size_t reason_cap,
                                 char *fix, size_t fix_cap, int *diagnosis_out,
                                 int *advisory_out) {
  reason[0] = '\0';
  fix[0] = '\0';
  if (advisory_out) {
    *advisory_out = 0;
  }
  IR_SIMD_SET_DIAG(IR_SIMD_BAIL_UNRECOGNIZED_SHAPE);

  const char *callee = NULL;
  int has_indirect_call = 0, has_new = 0, has_asm = 0;
  int branch_count = 0, jump_count = 0;
  const char *branch_targets[8];
  size_t branch_target_count = 0;
  int has_return_in_body = 0;
  int has_i16 = 0, has_i64 = 0, has_f32 = 0, has_f64 = 0;
  int has_byte_load = 0, has_i32_load = 0, has_int_accum = 0;
  const char *int_accum_sym = NULL;
  int has_float_accum = 0, has_float_mul = 0;
  int load_count = 0, store_count = 0, byte_store_count = 0;
  int reloaded_base_count = 0;
  const char *reloaded_base_sym = NULL;
  int past_header = 0;
  const char *body_local = NULL;
  const char *rebased_array = NULL;

