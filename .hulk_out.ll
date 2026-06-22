; ModuleID = 'hulk_program'
source_filename = "hulk_program"
target triple = "x86_64-pc-linux-gnu"

%HulkInstance_Circle = type { ptr, double }
%HulkInstance_Shape = type { ptr }
%BoxedValue = type { i32, [8 x i8] }

@PI = constant double 0x400921FB54442D18, align 8
@E = constant double 0x4005BF0A8B145769, align 8
@0 = private constant [7 x i8] c"Circle\00"

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

declare double @sin(double)

declare double @cos(double)

declare double @sqrt(double)

declare double @log(double)

declare double @exp(double)

declare i32 @rand()

declare ptr @hulk_range_create(double, double)

define i32 @main() {
entry:
  %0 = call ptr @malloc(i64 16)
  %1 = getelementptr inbounds nuw %HulkInstance_Circle, ptr %0, i32 0, i32 0
  store ptr @0, ptr %1, align 8
  %2 = getelementptr inbounds nuw %HulkInstance_Circle, ptr %0, i32 0, i32 1
  store double 5.000000e+00, ptr %2, align 8
  %3 = alloca ptr, align 8
  store ptr %0, ptr %3, align 8
  %4 = alloca ptr, align 8
  store ptr %0, ptr %4, align 8
  %5 = load ptr, ptr %4, align 8
  %6 = getelementptr inbounds nuw %HulkInstance_Circle, ptr %5, i32 0, i32 1
  %7 = load double, ptr %6, align 8
  %8 = getelementptr inbounds nuw %HulkInstance_Circle, ptr %0, i32 0, i32 1
  store double %7, ptr %8, align 8
  %9 = alloca ptr, align 8
  store ptr %0, ptr %9, align 8
  %10 = load ptr, ptr %9, align 8
  %11 = getelementptr inbounds nuw %HulkInstance_Shape, ptr %10, i32 0, i32 0
  %12 = load ptr, ptr %11, align 8
  %13 = icmp eq ptr %12, @0
  %14 = or i1 false, %13
  br i1 %14, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %15 = load ptr, ptr %9, align 8
  %16 = getelementptr inbounds nuw %HulkInstance_Shape, ptr %15, i32 0, i32 0
  %17 = load ptr, ptr %16, align 8
  %18 = icmp eq ptr %17, @0
  %19 = or i1 false, %18
  br i1 %19, label %as.ok, label %as.fail

if.end:                                           ; preds = %as.end, %entry
  %if.result = phi double [ 0.000000e+00, %as.end ], [ 0.000000e+00, %entry ]
  %expr_tmp = alloca double, align 8
  store double %if.result, ptr %expr_tmp, align 8
  ret i32 0

as.ok:                                            ; preds = %if.then
  br label %as.end

as.fail:                                          ; preds = %if.then
  call void @hulk_runtime_cast_error()
  unreachable

as.end:                                           ; preds = %as.ok
  %20 = getelementptr inbounds nuw %HulkInstance_Circle, ptr %15, i32 0, i32 1
  %21 = load double, ptr %20, align 8
  call void @hulk_print_double(double %21)
  br label %if.end
}

define ptr @hulk_meth_Shape_area_0(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  store ptr %0, ptr %1, align 8
  %2 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%BoxedValue, ptr null, i32 1) to i64))
  %3 = getelementptr inbounds nuw %BoxedValue, ptr %2, i32 0, i32 0
  store i32 1, ptr %3, align 4
  %4 = getelementptr inbounds nuw %BoxedValue, ptr %2, i32 0, i32 1
  store double 0.000000e+00, ptr %4, align 8
  ret ptr %2
}

define ptr @hulk_meth_Circle_area_0(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  store ptr %0, ptr %1, align 8
  %2 = load double, ptr @PI, align 8
  %3 = load ptr, ptr %1, align 8
  %4 = getelementptr inbounds nuw %HulkInstance_Circle, ptr %3, i32 0, i32 1
  %5 = load double, ptr %4, align 8
  %pow = call double @pow(double %5, double 2.000000e+00)
  %mul = fmul double %2, %pow
  %6 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%BoxedValue, ptr null, i32 1) to i64))
  %7 = getelementptr inbounds nuw %BoxedValue, ptr %6, i32 0, i32 0
  store i32 1, ptr %7, align 4
  %8 = getelementptr inbounds nuw %BoxedValue, ptr %6, i32 0, i32 1
  store double %mul, ptr %8, align 8
  ret ptr %6
}

declare void @hulk_runtime_cast_error()
