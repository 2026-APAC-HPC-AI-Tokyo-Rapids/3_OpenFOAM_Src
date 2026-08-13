# Symmetric `lduMatrix::Amul` CSR experiment

## Scope

`lduMatrix` represents a symmetric matrix with `diagPtr_` and `upperPtr_`;
`lowerPtr_` remains null. The constructors and ordinary coefficient access do
not call `lowerCSR()`, so `hasLowerCSR()` is normally false. Before this
experiment, `Amul()` treated that cache-presence query as its path-selection
flag and therefore used the face-scatter loop for an ordinarily constructed
symmetric matrix.

For a symmetric matrix, this experiment also selects the existing cell-gather
path. Its existing `lowerCSR()` call lazily copies `upperPtr_` into losort order
on the first `Amul()`. The mutable cache remains owned by the matrix, so later
PCG iterations reuse it. Explicit-CSR matrices retain the existing behavior;
asymmetric matrices without an explicit CSR cache retain the face loop.

## Arithmetic and residual ordering

The mathematical operation is unchanged. For valid LDU addressing, internal
faces are ordered by increasing owner cell, and by increasing neighbour within
an owner. For each target cell, the face loop therefore accumulates:

1. the diagonal product;
2. incoming-face products in original face order; and
3. outgoing-face products in original face order.

`calcLosort()` scans faces in original order and preserves that order within
each neighbour-cell bucket. The CSR loop accumulates the same three groups in
the same order. `map()` copies the symmetric upper coefficient without changing
its value. Thus the C++ source does not reassociate the per-cell sum, change the
PCG recurrence, stopping test, tolerance, or maximum iteration count. Interface
updates also remain after the internal multiplication.

The loop shape presented to the optimizing compiler does change. A particular
compiler or floating-point mode (especially fast-math or contraction settings)
may consequently generate different instructions even though source-level
operand order is preserved. Treat bitwise equality as something to verify for
the production toolchain, not as an assumption. Any residual difference can
propagate through PCG and, near a tolerance boundary, alter the reported
iteration count. Such a change fails this experiment's correctness criterion.

The cache costs one scalar per internal face. The CSR addressing also causes
the demand-driven losort, losort-start, lower-CSR-address, and owner-start data
to exist. First-use construction is part of the first multiplication; it is not
repeated per PCG iteration.

## End-to-end comparison plan

1. Build both the unchanged base commit and the experiment commit with the
   same production compiler, flags, MPI, precision, and dependencies. This is
   a source change, so both require traceable fresh builds.
2. Run `Test-lduMatrixCSR` for the experiment build. It requires the symmetric
   and equivalent asymmetric matrices to produce exactly equal `Amul` fields,
   verifies lazy materialization, and verifies reuse of the cached allocation.
3. Run the official cavity workload and official result parser for both builds
   with identical input, decomposition, PCG/preconditioner settings, rank and
   thread placement, node type, and filesystem placement. Use the same resource
   allocation shape and do not enable profiling in timing runs.
4. In a separate diagnostic run, set the `lduMatrix` debug switch to 2 and
   confirm `cell-based looping` for the symmetric solve. Keep debug output out
   of performance measurements.
5. Compare every solver invocation in order: field name, initial residual,
   final residual, and iteration count. Require identical iteration counts and
   acceptance by the official parser. Compare final result fields bitwise when
   the production toolchain permits it; otherwise record the maximum absolute
   and relative field differences and reject any unexplained drift.
6. Run multiple baseline and experiment repetitions, preferably interleaved,
   and compare solver-stage time separately from provisioning, case preparation,
   and artifact collection. Retain raw solver logs and official parser output
   with the run artifacts.
