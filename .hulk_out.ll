; ModuleID = 'hulk_program'
source_filename = "hulk_program"

%HulkInstance_Sub = type { double }
%BoxedValue = type { i32, [8 x i8] }

@PI = constant double 0x400921FB54442D18, align 8
@E = constant double 0x4005BF0A8B145769, align 8

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

declare double @sin(double)

declare double @cos(double)

declare double @sqrt(double)

declare double @log(double)

declare double @exp(double)

declare i32 @rand()

declare ptr @hulk_range_create(double, double)

define i32 @main() {
entry:
  %0 = call ptr @malloc(i64 8)
  %1 = getelementptr inbounds nuw %HulkInstance_Sub, ptr %0, i32 0, i32 0
  store double 5.000000e+00, ptr %1, align 8
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  %4 = getelementptr inbounds nuw %HulkInstance_Sub, ptr %3, i32 0, i32 0
  %5 = load double, ptr %4, align 8
  %6 = getelementptr inbounds nuw %HulkInstance_Sub, ptr %0, i32 0, i32 0
  store double %5, ptr %6, align 8
  %7 = load ptr, ptr %2, align 8
  %8 = getelementptr inbounds nuw %HulkInstance_Sub, ptr %7, i32 0, i32 0
  %9 = load double, ptr %8, align 8
  %10 = getelementptr inbounds nuw %HulkInstance_Sub, ptr %0, i32 0, i32 0
  store double %9, ptr %10, align 8
  %11 = alloca ptr, align 8
  store ptr %0, ptr %11, align 8
  %12 = alloca ptr, align 8
  store ptr %0, ptr %12, align 8
  %13 = load ptr, ptr %12, align 8
  %14 = call ptr @hulk_meth_Sub_twice_0(ptr %13)
  call void @hulk_print_boxed(ptr %14)
  %expr_tmp = alloca double, align 8
  store double 0.000000e+00, ptr %expr_tmp, align 8
  ret i32 0
}

define ptr @hulk_meth_Sub_twice_0(ptr %0) {
entry:
  %1 = alloca ptr, align 8
  store ptr %0, ptr %1, align 8
  %2 = load ptr, ptr %1, align 8
  %3 = getelementptr inbounds nuw %HulkInstance_Sub, ptr %2, i32 0, i32 0
  %4 = load double, ptr %3, align 8
  %mul = fmul double %4, 2.000000e+00
  %5 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%BoxedValue, ptr null, i32 1) to i64))
  %6 = getelementptr inbounds nuw %BoxedValue, ptr %5, i32 0, i32 0
  store i32 1, ptr %6, align 4
  %7 = getelementptr inbounds nuw %BoxedValue, ptr %5, i32 0, i32 1
  store double %mul, ptr %7, align 8
  ret ptr %5
}
