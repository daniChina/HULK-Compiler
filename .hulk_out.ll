; ModuleID = 'hulk_program'
source_filename = "hulk_program"

%BoxedValue = type { i32, [8 x i8] }

@PI = constant double 0x400921FB54442D18, align 8
@E = constant double 0x4005BF0A8B145769, align 8

declare i64 @time(i64*)

declare void @srand(i32)

declare i8* @malloc(i64)

declare double @pow(double, double)

declare double @fmod(double, double)

declare void @hulk_print_double(double)

declare void @hulk_print_bool(i32)

declare void @hulk_print_null()

declare void @hulk_print_newline()

declare void @hulk_print_boxed(%BoxedValue*)

declare %BoxedValue* @hulk_string_concat(%BoxedValue*, %BoxedValue*)

declare %BoxedValue* @hulk_string_concat_ws(%BoxedValue*, %BoxedValue*)

define i32 @main() {
entry:
  call void @hulk_print_newline()
  %expr_tmp = alloca double, align 8
  store double 0.000000e+00, double* %expr_tmp, align 8
  ret i32 0
}
