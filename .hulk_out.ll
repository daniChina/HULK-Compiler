; ModuleID = 'hulk_program'
source_filename = "hulk_program"
target triple = "x86_64-pc-linux-gnu"

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
  %0 = alloca double, align 8
  store double 1.000000e+00, ptr %0, align 8
  %1 = load double, ptr %0, align 8
  call void @hulk_print_double(double %1)
  %expr_tmp = alloca double, align 8
  store double 0.000000e+00, ptr %expr_tmp, align 8
  ret i32 0
}
