// RUN: mlir-opt %s --test-transform-dialect-interpreter --split-input-file | FileCheck %s

// CHECK-LABEL: func @genbool_1d
// CHECK: %[[T0:.*]] = arith.constant dense<[true, true, true, true, false, false, false, false]> : vector<8xi1>
// CHECK: return %[[T0]] : vector<8xi1>

func.func @genbool_1d() -> vector<8xi1> {
  %0 = vector.constant_mask [4] : vector<8xi1>
  return %0 : vector<8xi1>
}

// CHECK-LABEL: func @genbool_2d
// CHECK: %[[C1:.*]] = arith.constant dense<[true, true, false, false]> : vector<4xi1>
// CHECK: %[[C2:.*]] = arith.constant dense<false> : vector<4x4xi1>
// CHECK: %[[T0:.*]] = vector.insert %[[C1]], %[[C2]] [0] : vector<4xi1> into vector<4x4xi1>
// CHECK: %[[T1:.*]] = vector.insert %[[C1]], %[[T0]] [1] : vector<4xi1> into vector<4x4xi1>
// CHECK: return %[[T1]] : vector<4x4xi1>

func.func @genbool_2d() -> vector<4x4xi1> {
  %v = vector.constant_mask [2, 2] : vector<4x4xi1>
  return %v: vector<4x4xi1>
}

// CHECK-LABEL: func @genbool_3d
// CHECK: %[[C1:.*]] = arith.constant dense<[true, true, true, false]> : vector<4xi1>
// CHECK: %[[C2:.*]] = arith.constant dense<false> : vector<3x4xi1>
// CHECK: %[[C3:.*]] = arith.constant dense<false> : vector<2x3x4xi1>
// CHECK: %[[T0:.*]] = vector.insert %[[C1]], %[[C2]] [0] : vector<4xi1> into vector<3x4xi1>
// CHECK: %[[T1:.*]] = vector.insert %[[T0]], %[[C3]] [0] : vector<3x4xi1> into vector<2x3x4xi1>
// CHECK: return %[[T1]] : vector<2x3x4xi1>

func.func @genbool_3d() -> vector<2x3x4xi1> {
  %v = vector.constant_mask [1, 1, 3] : vector<2x3x4xi1>
  return %v: vector<2x3x4xi1>
}

// CHECK-LABEL: func @genbool_var_1d(
// CHECK-SAME: %[[A:.*]]: index)
// CHECK:      %[[T0:.*]] = vector.create_mask %[[A]] : vector<3xi1>
// CHECK:      return %[[T0]] : vector<3xi1>

func.func @genbool_var_1d(%arg0: index) -> vector<3xi1> {
  %0 = vector.create_mask %arg0 : vector<3xi1>
  return %0 : vector<3xi1>
}

// CHECK-LABEL: func @genbool_var_2d(
// CHECK-SAME: %[[A:.*0]]: index,
// CHECK-SAME: %[[B:.*1]]: index)
// CHECK:      %[[C1:.*]] = arith.constant dense<false> : vector<3xi1>
// CHECK:      %[[C2:.*]] = arith.constant dense<false> : vector<2x3xi1>
// CHECK:      %[[c0:.*]] = arith.constant 0 : index
// CHECK:      %[[c1:.*]] = arith.constant 1 : index
// CHECK:      %[[T0:.*]] = vector.create_mask %[[B]] : vector<3xi1>
// CHECK:      %[[T1:.*]] = arith.cmpi sgt, %[[A]], %[[c0]] : index
// CHECK:      %[[T2:.*]] = arith.select %[[T1]], %[[T0]], %[[C1]] : vector<3xi1>
// CHECK:      %[[T3:.*]] = vector.insert %[[T2]], %[[C2]] [0] : vector<3xi1> into vector<2x3xi1>
// CHECK:      %[[T4:.*]] = arith.cmpi sgt, %[[A]], %[[c1]] : index
// CHECK:      %[[T5:.*]] = arith.select %[[T4]], %[[T0]], %[[C1]] : vector<3xi1>
// CHECK:      %[[T6:.*]] = vector.insert %[[T5]], %[[T3]] [1] : vector<3xi1> into vector<2x3xi1>
// CHECK:      return %[[T6]] : vector<2x3xi1>

func.func @genbool_var_2d(%arg0: index, %arg1: index) -> vector<2x3xi1> {
  %0 = vector.create_mask %arg0, %arg1 : vector<2x3xi1>
  return %0 : vector<2x3xi1>
}

// CHECK-LABEL: func @genbool_var_3d(
// CHECK-SAME: %[[A:.*0]]: index,
// CHECK-SAME: %[[B:.*1]]: index,
// CHECK-SAME: %[[C:.*2]]: index)
// CHECK-DAG:  %[[C1:.*]] = arith.constant dense<false> : vector<7xi1>
// CHECK-DAG:  %[[C2:.*]] = arith.constant dense<false> : vector<1x7xi1>
// CHECK-DAG:  %[[C3:.*]] = arith.constant dense<false> : vector<2x1x7xi1>
// CHECK-DAG:  %[[c0:.*]] = arith.constant 0 : index
// CHECK-DAG:  %[[c1:.*]] = arith.constant 1 : index
// CHECK:      %[[T0:.*]] = vector.create_mask %[[C]] : vector<7xi1>
// CHECK:      %[[T1:.*]] = arith.cmpi sgt, %[[B]], %[[c0]] : index
// CHECK:      %[[T2:.*]] = arith.select %[[T1]], %[[T0]], %[[C1]] : vector<7xi1>
// CHECK:      %[[T3:.*]] = vector.insert %[[T2]], %[[C2]] [0] : vector<7xi1> into vector<1x7xi1>
// CHECK:      %[[T4:.*]] = arith.cmpi sgt, %[[A]], %[[c0]] : index
// CHECK:      %[[T5:.*]] = arith.select %[[T4]], %[[T3]], %[[C2]] : vector<1x7xi1>
// CHECK:      %[[T6:.*]] = vector.insert %[[T5]], %[[C3]] [0] : vector<1x7xi1> into vector<2x1x7xi1>
// CHECK:      %[[T7:.*]] = arith.cmpi sgt, %[[A]], %[[c1]] : index
// CHECK:      %[[T8:.*]] = arith.select %[[T7]], %[[T3]], %[[C2]] : vector<1x7xi1>
// CHECK:      %[[T9:.*]] = vector.insert %[[T8]], %[[T6]] [1] : vector<1x7xi1> into vector<2x1x7xi1>
// CHECK:      return %[[T9]] : vector<2x1x7xi1>

func.func @genbool_var_3d(%arg0: index, %arg1: index, %arg2: index) -> vector<2x1x7xi1> {
  %0 = vector.create_mask %arg0, %arg1, %arg2 : vector<2x1x7xi1>
  return %0 : vector<2x1x7xi1>
}

transform.sequence failures(propagate) {
^bb1(%module_op: !pdl.operation):
  %f = transform.structured.match ops{["func.func"]} in %module_op 
    : (!pdl.operation) -> !pdl.operation

  transform.vector.lower_mask %f
      : (!pdl.operation) -> !pdl.operation
}
