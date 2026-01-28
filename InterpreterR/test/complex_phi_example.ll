; ModuleID = 'complex_phi_example.c'
source_filename = "complex_phi_example.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define dso_local i32 @complex_phi_example(i32 %X) #0 {
A:
  %cond = icmp sgt i32 %X, 0
  br i1 %cond, label %B, label %C

B:
  ; PHI指令合并来自A的X和来自D的X.redef
  %X.new = phi i32 [ %X, %A ], [ %X.redef, %D ]
  
  ; B块中的其他指令
  %val_from_B = add nsw i32 %X.new, 10
  br label %E
  
C:
  ; C块中的其他指令
  %val_from_C = sub nsw i32 %X, 5
  br label %E

D:
  ; D是B的另一个前驱块
  %X.redef = mul nsw i32 %X, 2
  br label %B

E:
  ; 合并B和C的结果
  %result = phi i32 [ %val_from_B, %B ], [ %val_from_C, %C ]
  ret i32 %result
}

attributes #0 = { "frame-pointer"="all" }
