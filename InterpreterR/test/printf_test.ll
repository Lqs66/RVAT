; LLVM IR test with printf to show output
; Tests basic arithmetic and function calls with visible output

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Declare external printf function
declare i32 @printf(i8*, ...)

; Global string constants
@.str_result = private unnamed_addr constant [23 x i8] c"Factorial of %d is %d\0A\00", align 1
@.str_add = private unnamed_addr constant [14 x i8] c"%d + %d = %d\0A\00", align 1
@.str_test = private unnamed_addr constant [28 x i8] c"Starting InterpreterR test\0A\00", align 1

; Simple add function
define i32 @add(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}

; Factorial function
define i32 @factorial(i32 %n) {
entry:
  %cmp = icmp sle i32 %n, 1
  br i1 %cmp, label %base_case, label %recursive_case

base_case:
  ret i32 1

recursive_case:
  %n_minus_1 = sub i32 %n, 1
  %fact_n_minus_1 = call i32 @factorial(i32 %n_minus_1)
  %result = mul i32 %n, %fact_n_minus_1
  ret i32 %result
}

; Main function with printf output
define i32 @main() {
entry:
  ; Print test start message
  %start_msg = getelementptr inbounds [25 x i8], [25 x i8]* @.str_test, i64 0, i64 0
  call i32 (i8*, ...) @printf(i8* %start_msg)
  
  ; Test addition: 10 + 20 = 30
  %sum = call i32 @add(i32 10, i32 20)
  %add_fmt = getelementptr inbounds [20 x i8], [20 x i8]* @.str_add, i64 0, i64 0
  call i32 (i8*, ...) @printf(i8* %add_fmt, i32 10, i32 20, i32 %sum)
  
  ; Test factorial(5) = 120
  %fact5 = call i32 @factorial(i32 5)
  %fact_fmt = getelementptr inbounds [30 x i8], [30 x i8]* @.str_result, i64 0, i64 0
  call i32 (i8*, ...) @printf(i8* %fact_fmt, i32 5, i32 %fact5)
  
  ; Test factorial(6) = 720
  %fact6 = call i32 @factorial(i32 6)
  call i32 (i8*, ...) @printf(i8* %fact_fmt, i32 6, i32 %fact6)
  
  ; Test factorial(7) = 5040
  %fact7 = call i32 @factorial(i32 7)
  call i32 (i8*, ...) @printf(i8* %fact_fmt, i32 7, i32 %fact7)
  
  ; Return success
  ret i32 0
}
