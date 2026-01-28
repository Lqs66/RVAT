; ModuleID = 'phi_example.c'
source_filename = "phi_example.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define dso_local i32 @main() #0 {
entry:
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  store i32 10, ptr %x, align 4
  
  %cmp = load i32, ptr %x, align 4
  %cond = icmp sgt i32 %cmp, 5
  br i1 %cond, label %then, label %else

then:
  store i32 1, ptr %y, align 4
  br label %merge

else:
  store i32 2, ptr %y, align 4
  br label %merge

merge:
  %result = phi i32 [ 1, %then ], [ 2, %else ]
  %final = load i32, ptr %y, align 4
  ret i32 %result
}

attributes #0 = { "frame-pointer"="all" }
