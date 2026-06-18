; ModuleID = 'hulk_program'
source_filename = "hulk_program"

%BoxedValue = type { i32, [8 x i8] }
%HulkRange = type { double, double }

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

declare double @sin(double)

declare double @cos(double)

declare double @sqrt(double)

declare double @log(double)

declare double @exp(double)

declare i32 @rand()

declare %HulkRange* @hulk_range_create(double, double)

define i32 @main() {
entry:
  %0 = call %HulkRange* @hulk_range_create(double 0.000000e+00, double 3.000000e+00)
  %1 = getelementptr inbounds %HulkRange, %HulkRange* %0, i32 0, i32 0
  %2 = load double, double* %1, align 8
  %3 = getelementptr inbounds %HulkRange, %HulkRange* %0, i32 0, i32 1
  %4 = load double, double* %3, align 8
  %5 = alloca double, align 8
  store double %2, double* %5, align 8
  br label %for.cond

for.cond:                                         ; preds = %for.step, %entry
  %6 = load double, double* %5, align 8
  %7 = fcmp olt double %6, %4
  br i1 %7, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %8 = alloca double, align 8
  store double %6, double* %8, align 8
  %9 = load double, double* %8, align 8
  call void @hulk_print_double(double %9)
  br label %for.step

for.step:                                         ; preds = %for.body
  %10 = load double, double* %5, align 8
  %11 = fadd double %10, 1.000000e+00
  store double %11, double* %5, align 8
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %expr_tmp = alloca double, align 8
  store double 0.000000e+00, double* %expr_tmp, align 8
  ret i32 0
}
