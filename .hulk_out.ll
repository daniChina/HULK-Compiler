; ModuleID = 'hulk_program'
source_filename = "hulk_program"
target triple = "x86_64-pc-linux-gnu"

%HulkInstance_Vector = type { ptr, double, double }
%BoxedValue = type { i32, [8 x i8] }

@PI = constant double 0x400921FB54442D18, align 8
@E = constant double 0x4005BF0A8B145769, align 8
@0 = private constant [7 x i8] c"Vector\00"
@1 = private unnamed_addr constant [13 x i8] c"invalid cast\00", align 1
@2 = private constant [3 x i8] c"ok\00"
@3 = private constant [5 x i8] c"fail\00"
@4 = private unnamed_addr constant [13 x i8] c"invalid cast\00", align 1
@5 = private unnamed_addr constant [13 x i8] c"invalid cast\00", align 1
@6 = private unnamed_addr constant [13 x i8] c"invalid cast\00", align 1
@7 = private unnamed_addr constant [13 x i8] c"invalid cast\00", align 1

declare i64 @time(ptr)

declare void @srand(i32)

declare ptr @malloc(i64)

declare double @pow(double, double)

declare double @fmod(double, double)

declare void @hulk_print_double(double)

declare void @hulk_print_bool(i32)

declare void @hulk_print_null()

declare void @hulk_print_newline()

declare void @hulk_print_boxed(ptr)

declare ptr @hulk_string_concat(ptr, ptr)

declare ptr @hulk_string_concat_ws(ptr, ptr)

declare i32 @hulk_string_equals(ptr, ptr)

declare i32 @hulk_boxed_equals(ptr, ptr)

declare ptr @hulk_box_number(double)

declare double @hulk_unbox_number(ptr)

declare ptr @hulk_box_bool(i32)

declare void @hulk_print_instance(ptr)

declare double @sin(double)

declare double @cos(double)

declare double @sqrt(double)

declare double @log(double)

declare double @exp(double)

declare i32 @rand()

declare ptr @hulk_range_create(double, double)

define i32 @main() {
entry:
  %0 = call ptr @malloc(i64 24)
  %1 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %0, i32 0, i32 0
  store ptr @0, ptr %1, align 8
  %2 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %0, i32 0, i32 1
  store double 3.000000e+00, ptr %2, align 8
  %3 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %0, i32 0, i32 2
  store double 4.000000e+00, ptr %3, align 8
  %4 = alloca ptr, align 8
  store ptr %0, ptr %4, align 8
  %5 = load ptr, ptr %4, align 8
  %6 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %5, i32 0, i32 1
  %7 = load double, ptr %6, align 8
  %8 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %0, i32 0, i32 1
  store double %7, ptr %8, align 8
  %9 = load ptr, ptr %4, align 8
  %10 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %9, i32 0, i32 2
  %11 = load double, ptr %10, align 8
  %12 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %0, i32 0, i32 2
  store double %11, ptr %12, align 8
  %13 = alloca ptr, align 8
  store ptr %0, ptr %13, align 8
  %14 = call ptr @malloc(i64 24)
  %15 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %14, i32 0, i32 0
  store ptr @0, ptr %15, align 8
  %16 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %14, i32 0, i32 1
  store double 1.000000e+00, ptr %16, align 8
  %17 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %14, i32 0, i32 2
  store double 2.000000e+00, ptr %17, align 8
  %18 = alloca ptr, align 8
  store ptr %14, ptr %18, align 8
  %19 = load ptr, ptr %18, align 8
  %20 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %19, i32 0, i32 1
  %21 = load double, ptr %20, align 8
  %22 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %14, i32 0, i32 1
  store double %21, ptr %22, align 8
  %23 = load ptr, ptr %18, align 8
  %24 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %23, i32 0, i32 2
  %25 = load double, ptr %24, align 8
  %26 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %14, i32 0, i32 2
  store double %25, ptr %26, align 8
  %27 = alloca ptr, align 8
  store ptr %14, ptr %27, align 8
  %28 = load ptr, ptr %13, align 8
  %29 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %28, i32 0, i32 0
  %30 = load ptr, ptr %29, align 8
  %31 = icmp eq ptr %30, @0
  br i1 %31, label %meth.call.Vector, label %meth.cont.Vector

meth.merge:                                       ; preds = %meth.call.Vector
  %meth.result = phi ptr [ %32, %meth.call.Vector ]
  %boxed_num_data = getelementptr inbounds nuw %BoxedValue, ptr %meth.result, i32 0, i32 1
  %unboxed_num = load double, ptr %boxed_num_data, align 8
  %cmp = fcmp oeq double %unboxed_num, 2.500000e+01
  br i1 %cmp, label %if.then, label %if.else

meth.call.Vector:                                 ; preds = %entry
  %32 = call ptr @hulk_meth_Vector_length_sq_0(ptr %28)
  br label %meth.merge

meth.cont.Vector:                                 ; preds = %entry
  call void @hulk_runtime_error_at(i32 0, i32 0, ptr @1)
  unreachable

if.then:                                          ; preds = %meth.merge
  %33 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%BoxedValue, ptr null, i32 1) to i64))
  %34 = getelementptr inbounds nuw %BoxedValue, ptr %33, i32 0, i32 0
  store i32 2, ptr %34, align 4
  %35 = getelementptr inbounds nuw %BoxedValue, ptr %33, i32 0, i32 1
  store ptr @2, ptr %35, align 8
  call void @hulk_print_boxed(ptr %33)
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %if.result = phi double [ 0.000000e+00, %if.then ], [ 0.000000e+00, %if.else ]
  %36 = load ptr, ptr %27, align 8
  %37 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %36, i32 0, i32 0
  %38 = load ptr, ptr %37, align 8
  %39 = icmp eq ptr %38, @0
  br i1 %39, label %meth.call.Vector3, label %meth.cont.Vector4

if.else:                                          ; preds = %meth.merge
  %40 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%BoxedValue, ptr null, i32 1) to i64))
  %41 = getelementptr inbounds nuw %BoxedValue, ptr %40, i32 0, i32 0
  store i32 2, ptr %41, align 4
  %42 = getelementptr inbounds nuw %BoxedValue, ptr %40, i32 0, i32 1
  store ptr @3, ptr %42, align 8
  call void @hulk_print_boxed(ptr %40)
  br label %if.end

meth.merge1:                                      ; preds = %meth.call.Vector3
  %meth.result2 = phi ptr [ %43, %meth.call.Vector3 ]
  %boxed_num_data5 = getelementptr inbounds nuw %BoxedValue, ptr %meth.result2, i32 0, i32 1
  %unboxed_num6 = load double, ptr %boxed_num_data5, align 8
  %cmp7 = fcmp oeq double %unboxed_num6, 5.000000e+00
  br i1 %cmp7, label %if.then8, label %if.else10

meth.call.Vector3:                                ; preds = %if.end
  %43 = call ptr @hulk_meth_Vector_length_sq_0(ptr %36)
  br label %meth.merge1

meth.cont.Vector4:                                ; preds = %if.end
  call void @hulk_runtime_error_at(i32 0, i32 0, ptr @4)
  unreachable

if.then8:                                         ; preds = %meth.merge1
  %44 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%BoxedValue, ptr null, i32 1) to i64))
  %45 = getelementptr inbounds nuw %BoxedValue, ptr %44, i32 0, i32 0
  store i32 2, ptr %45, align 4
  %46 = getelementptr inbounds nuw %BoxedValue, ptr %44, i32 0, i32 1
  store ptr @2, ptr %46, align 8
  call void @hulk_print_boxed(ptr %44)
  br label %if.end9

if.end9:                                          ; preds = %if.else10, %if.then8
  %if.result11 = phi double [ 0.000000e+00, %if.then8 ], [ 0.000000e+00, %if.else10 ]
  %47 = load ptr, ptr %13, align 8
  %48 = load ptr, ptr %27, align 8
  %49 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %47, i32 0, i32 0
  %50 = load ptr, ptr %49, align 8
  %51 = icmp eq ptr %50, @0
  br i1 %51, label %meth.call.Vector14, label %meth.cont.Vector15

if.else10:                                        ; preds = %meth.merge1
  %52 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%BoxedValue, ptr null, i32 1) to i64))
  %53 = getelementptr inbounds nuw %BoxedValue, ptr %52, i32 0, i32 0
  store i32 2, ptr %53, align 4
  %54 = getelementptr inbounds nuw %BoxedValue, ptr %52, i32 0, i32 1
  store ptr @3, ptr %54, align 8
  call void @hulk_print_boxed(ptr %52)
  br label %if.end9

meth.merge12:                                     ; preds = %meth.call.Vector14
  %meth.result13 = phi ptr [ %55, %meth.call.Vector14 ]
  %boxed_num_data16 = getelementptr inbounds nuw %BoxedValue, ptr %meth.result13, i32 0, i32 1
  %unboxed_num17 = load double, ptr %boxed_num_data16, align 8
  %cmp18 = fcmp oeq double %unboxed_num17, 1.100000e+01
  br i1 %cmp18, label %if.then19, label %if.else21

meth.call.Vector14:                               ; preds = %if.end9
  %55 = call ptr @hulk_meth_Vector_dot_1(ptr %47, ptr %48)
  br label %meth.merge12

meth.cont.Vector15:                               ; preds = %if.end9
  call void @hulk_runtime_error_at(i32 0, i32 0, ptr @5)
  unreachable

if.then19:                                        ; preds = %meth.merge12
  %56 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%BoxedValue, ptr null, i32 1) to i64))
  %57 = getelementptr inbounds nuw %BoxedValue, ptr %56, i32 0, i32 0
  store i32 2, ptr %57, align 4
  %58 = getelementptr inbounds nuw %BoxedValue, ptr %56, i32 0, i32 1
  store ptr @2, ptr %58, align 8
  call void @hulk_print_boxed(ptr %56)
  br label %if.end20

if.end20:                                         ; preds = %if.else21, %if.then19
  %if.result22 = phi double [ 0.000000e+00, %if.then19 ], [ 0.000000e+00, %if.else21 ]
  %59 = load ptr, ptr %13, align 8
  %60 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %59, i32 0, i32 0
  %61 = load ptr, ptr %60, align 8
  %62 = icmp eq ptr %61, @0
  br i1 %62, label %meth.call.Vector25, label %meth.cont.Vector26

if.else21:                                        ; preds = %meth.merge12
  %63 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%BoxedValue, ptr null, i32 1) to i64))
  %64 = getelementptr inbounds nuw %BoxedValue, ptr %63, i32 0, i32 0
  store i32 2, ptr %64, align 4
  %65 = getelementptr inbounds nuw %BoxedValue, ptr %63, i32 0, i32 1
  store ptr @3, ptr %65, align 8
  call void @hulk_print_boxed(ptr %63)
  br label %if.end20

meth.merge23:                                     ; preds = %meth.call.Vector25
  %meth.result24 = phi ptr [ %70, %meth.call.Vector25 ]
  %66 = load ptr, ptr %13, align 8
  %67 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %66, i32 0, i32 0
  %68 = load ptr, ptr %67, align 8
  %69 = icmp eq ptr %68, @0
  br i1 %69, label %meth.call.Vector29, label %meth.cont.Vector30

meth.call.Vector25:                               ; preds = %if.end20
  %70 = call ptr @hulk_meth_Vector_scale_1(ptr %59, double 2.000000e+00)
  br label %meth.merge23

meth.cont.Vector26:                               ; preds = %if.end20
  call void @hulk_runtime_error_at(i32 0, i32 0, ptr @6)
  unreachable

meth.merge27:                                     ; preds = %meth.call.Vector29
  %meth.result28 = phi ptr [ %71, %meth.call.Vector29 ]
  %boxed_num_data31 = getelementptr inbounds nuw %BoxedValue, ptr %meth.result28, i32 0, i32 1
  %unboxed_num32 = load double, ptr %boxed_num_data31, align 8
  %cmp33 = fcmp oeq double %unboxed_num32, 1.000000e+02
  br i1 %cmp33, label %if.then34, label %if.else36

meth.call.Vector29:                               ; preds = %meth.merge23
  %71 = call ptr @hulk_meth_Vector_length_sq_0(ptr %66)
  br label %meth.merge27

meth.cont.Vector30:                               ; preds = %meth.merge23
  call void @hulk_runtime_error_at(i32 0, i32 0, ptr @7)
  unreachable

if.then34:                                        ; preds = %meth.merge27
  %72 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%BoxedValue, ptr null, i32 1) to i64))
  %73 = getelementptr inbounds nuw %BoxedValue, ptr %72, i32 0, i32 0
  store i32 2, ptr %73, align 4
  %74 = getelementptr inbounds nuw %BoxedValue, ptr %72, i32 0, i32 1
  store ptr @2, ptr %74, align 8
  call void @hulk_print_boxed(ptr %72)
  br label %if.end35

if.end35:                                         ; preds = %if.else36, %if.then34
  %if.result37 = phi double [ 0.000000e+00, %if.then34 ], [ 0.000000e+00, %if.else36 ]
  %expr_tmp = alloca double, align 8
  store double %if.result37, ptr %expr_tmp, align 8
  ret i32 0

if.else36:                                        ; preds = %meth.merge27
  %75 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%BoxedValue, ptr null, i32 1) to i64))
  %76 = getelementptr inbounds nuw %BoxedValue, ptr %75, i32 0, i32 0
  store i32 2, ptr %76, align 4
  %77 = getelementptr inbounds nuw %BoxedValue, ptr %75, i32 0, i32 1
  store ptr @3, ptr %77, align 8
  call void @hulk_print_boxed(ptr %75)
  br label %if.end35
}

define ptr @hulk_meth_Vector_length_sq_0(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  store ptr %0, ptr %1, align 8
  %2 = load ptr, ptr %1, align 8
  %3 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %2, i32 0, i32 1
  %4 = load double, ptr %3, align 8
  %5 = load ptr, ptr %1, align 8
  %6 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %5, i32 0, i32 1
  %7 = load double, ptr %6, align 8
  %mul = fmul double %4, %7
  %8 = load ptr, ptr %1, align 8
  %9 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %8, i32 0, i32 2
  %10 = load double, ptr %9, align 8
  %11 = load ptr, ptr %1, align 8
  %12 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %11, i32 0, i32 2
  %13 = load double, ptr %12, align 8
  %mul1 = fmul double %10, %13
  %add = fadd double %mul, %mul1
  %boxed_double = call ptr @hulk_box_number(double %add)
  ret ptr %boxed_double
}

define ptr @hulk_meth_Vector_dot_1(ptr %0, ptr %1) {
entry:
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = alloca ptr, align 8
  store ptr %1, ptr %3, align 8
  %4 = load ptr, ptr %2, align 8
  %5 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %4, i32 0, i32 1
  %6 = load double, ptr %5, align 8
  %7 = load ptr, ptr %3, align 8
  %8 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %7, i32 0, i32 1
  %9 = load double, ptr %8, align 8
  %mul = fmul double %6, %9
  %10 = load ptr, ptr %2, align 8
  %11 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %10, i32 0, i32 2
  %12 = load double, ptr %11, align 8
  %13 = load ptr, ptr %3, align 8
  %14 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %13, i32 0, i32 2
  %15 = load double, ptr %14, align 8
  %mul1 = fmul double %12, %15
  %add = fadd double %mul, %mul1
  %boxed_double = call ptr @hulk_box_number(double %add)
  ret ptr %boxed_double
}

define ptr @hulk_meth_Vector_scale_1(ptr %0, double %1) {
entry:
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = alloca double, align 8
  store double %1, ptr %3, align 8
  %4 = load ptr, ptr %2, align 8
  %5 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %4, i32 0, i32 1
  %6 = load double, ptr %5, align 8
  %7 = load double, ptr %3, align 8
  %mul = fmul double %6, %7
  %8 = load ptr, ptr %2, align 8
  %9 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %8, i32 0, i32 1
  store double %mul, ptr %9, align 8
  %10 = load ptr, ptr %2, align 8
  %11 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %10, i32 0, i32 2
  %12 = load double, ptr %11, align 8
  %13 = load double, ptr %3, align 8
  %mul1 = fmul double %12, %13
  %14 = load ptr, ptr %2, align 8
  %15 = getelementptr inbounds nuw %HulkInstance_Vector, ptr %14, i32 0, i32 2
  store double %mul1, ptr %15, align 8
  %boxed_double = call ptr @hulk_box_number(double %mul1)
  ret ptr %boxed_double
}

declare void @hulk_runtime_error_at(i32, i32, ptr)
