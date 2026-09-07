  snprintf(d->reason, d->reason_cap,
           "no vectorizer recognized this loop's shape: by the time the "
           "vectorizers ran, its body had no call, branch, unsupported "
           "element width or carried dependence left to blame");
  snprintf(d->fix, d->fix_cap,
           "compare the loop against the shapes that do vectorize: "
           "unit-stride `a[i]` (not `a[i*k]`) over int8/int32/float32/float64, "
           "a straight-line body, and one of a map (`a[i] = expr`), a '+' "
           "reduction (`s = s + expr`), or a dot product");
  ir_simd_advisory(d);