// RUN: not llvm-mc -triple=aarch64 -show-encoding -mattr=+sve  2>&1 < %s| FileCheck %s

// ------------------------------------------------------------------------- //
// Invalid result register

uqdecp sp, p0
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid operand
// CHECK-NEXT: uqdecp sp, p0
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:

uqdecp z0.b, p0
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid element width
// CHECK-NEXT: uqdecp z0.b, p0
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:

uqdecp x0, p0.b, w0
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid operand
// CHECK-NEXT: uqdecp x0, p0.b, w0
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:

uqdecp x0, p0.b, x1
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid operand
// CHECK-NEXT: uqdecp x0, p0.b, x1
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:


// ------------------------------------------------------------------------- //
// Invalid predicate operand

uqdecp x0, p0
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid predicate register
// CHECK-NEXT: uqdecp x0, p0
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:

uqdecp x0, p0/z
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid predicate register
// CHECK-NEXT: uqdecp x0, p0/z
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:

uqdecp x0, p0/m
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid predicate register
// CHECK-NEXT: uqdecp x0, p0/m
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:

uqdecp x0, p0.q
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid predicate register
// CHECK-NEXT: uqdecp x0, p0.q
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:


// --------------------------------------------------------------------------//
// Negative tests for instructions that are incompatible with movprfx

movprfx z0.d, p0/z, z7.d
sqincp  z0.d, p0
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: instruction is unpredictable when following a predicated movprfx, suggest using unpredicated movprfx
// CHECK-NEXT: sqincp  z0.d, p0
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:
