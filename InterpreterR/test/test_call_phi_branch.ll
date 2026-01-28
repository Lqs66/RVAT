; ModuleID = 'test_call_phi_branch.c'
source_filename = "test_call_phi_branch.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [31 x i8] c"Branch 1: Adding %d + %d = %d\0A\00", align 1
@.str.1 = private unnamed_addr constant [36 x i8] c"Branch 2: Multiplying %d * %d = %d\0A\00", align 1
@.str.2 = private unnamed_addr constant [38 x i8] c"Branch 3: Computing value of %d = %d\0A\00", align 1
@.str.3 = private unnamed_addr constant [28 x i8] c"Large result: %d - 10 = %d\0A\00", align 1
@.str.4 = private unnamed_addr constant [28 x i8] c"Small result: %d + 20 = %d\0A\00", align 1
@.str.5 = private unnamed_addr constant [27 x i8] c"Final result: %d, Sum: %d\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @add_numbers(i32 noundef %0, i32 noundef %1) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  store i32 %1, ptr %4, align 4
  %5 = load i32, ptr %3, align 4
  %6 = load i32, ptr %4, align 4
  %7 = add nsw i32 %5, %6
  ret i32 %7
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @multiply_numbers(i32 noundef %0, i32 noundef %1) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  store i32 %1, ptr %4, align 4
  %5 = load i32, ptr %3, align 4
  %6 = load i32, ptr %4, align 4
  %7 = mul nsw i32 %5, %6
  ret i32 %7
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @compute_value(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  %4 = load i32, ptr %3, align 4
  %5 = icmp sgt i32 %4, 10
  br i1 %5, label %6, label %9

6:                                                ; preds = %1
  %7 = load i32, ptr %3, align 4
  %8 = mul nsw i32 %7, 2
  store i32 %8, ptr %2, align 4
  br label %12

9:                                                ; preds = %1
  %10 = load i32, ptr %3, align 4
  %11 = add nsw i32 %10, 5
  store i32 %11, ptr %2, align 4
  br label %12

12:                                               ; preds = %9, %6
  %13 = load i32, ptr %2, align 4
  ret i32 %13
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  %7 = alloca i32, align 4
  %8 = alloca i32, align 4
  store i32 0, ptr %1, align 4
  store i32 15, ptr %2, align 4
  store i32 8, ptr %3, align 4
  %9 = load i32, ptr %2, align 4
  %10 = srem i32 %9, 3
  store i32 %10, ptr %5, align 4
  %11 = load i32, ptr %5, align 4
  %12 = icmp eq i32 %11, 0
  br i1 %12, label %13, label %21

13:                                               ; preds = %0
  %14 = load i32, ptr %2, align 4
  %15 = load i32, ptr %3, align 4
  %16 = call i32 @add_numbers(i32 noundef %14, i32 noundef %15)
  store i32 %16, ptr %4, align 4
  %17 = load i32, ptr %2, align 4
  %18 = load i32, ptr %3, align 4
  %19 = load i32, ptr %4, align 4
  %20 = call i32 (ptr, ...) @printf(ptr noundef @.str, i32 noundef %17, i32 noundef %18, i32 noundef %19)
  br label %39

21:                                               ; preds = %0
  %22 = load i32, ptr %5, align 4
  %23 = icmp eq i32 %22, 1
  br i1 %23, label %24, label %32

24:                                               ; preds = %21
  %25 = load i32, ptr %2, align 4
  %26 = load i32, ptr %3, align 4
  %27 = call i32 @multiply_numbers(i32 noundef %25, i32 noundef %26)
  store i32 %27, ptr %4, align 4
  %28 = load i32, ptr %2, align 4
  %29 = load i32, ptr %3, align 4
  %30 = load i32, ptr %4, align 4
  %31 = call i32 (ptr, ...) @printf(ptr noundef @.str.1, i32 noundef %28, i32 noundef %29, i32 noundef %30)
  br label %38

32:                                               ; preds = %21
  %33 = load i32, ptr %2, align 4
  %34 = call i32 @compute_value(i32 noundef %33)
  store i32 %34, ptr %4, align 4
  %35 = load i32, ptr %2, align 4
  %36 = load i32, ptr %4, align 4
  %37 = call i32 (ptr, ...) @printf(ptr noundef @.str.2, i32 noundef %35, i32 noundef %36)
  br label %38

38:                                               ; preds = %32, %24
  br label %39

39:                                               ; preds = %38, %13
  %40 = load i32, ptr %4, align 4
  %41 = icmp sgt i32 %40, 50
  br i1 %41, label %42, label %48

42:                                               ; preds = %39
  %43 = load i32, ptr %4, align 4
  %44 = sub nsw i32 %43, 10
  store i32 %44, ptr %6, align 4
  %45 = load i32, ptr %4, align 4
  %46 = load i32, ptr %6, align 4
  %47 = call i32 (ptr, ...) @printf(ptr noundef @.str.3, i32 noundef %45, i32 noundef %46)
  br label %54

48:                                               ; preds = %39
  %49 = load i32, ptr %4, align 4
  %50 = add nsw i32 %49, 20
  store i32 %50, ptr %6, align 4
  %51 = load i32, ptr %4, align 4
  %52 = load i32, ptr %6, align 4
  %53 = call i32 (ptr, ...) @printf(ptr noundef @.str.4, i32 noundef %51, i32 noundef %52)
  br label %54

54:                                               ; preds = %48, %42
  store i32 0, ptr %7, align 4
  store i32 0, ptr %8, align 4
  br label %55

55:                                               ; preds = %62, %54
  %56 = load i32, ptr %8, align 4
  %57 = icmp slt i32 %56, 5
  br i1 %57, label %58, label %65

58:                                               ; preds = %55
  %59 = load i32, ptr %7, align 4
  %60 = load i32, ptr %8, align 4
  %61 = call i32 @add_numbers(i32 noundef %59, i32 noundef %60)
  store i32 %61, ptr %7, align 4
  br label %62

62:                                               ; preds = %58
  %63 = load i32, ptr %8, align 4
  %64 = add nsw i32 %63, 1
  store i32 %64, ptr %8, align 4
  br label %55, !llvm.loop !6

65:                                               ; preds = %55
  %66 = load i32, ptr %6, align 4
  %67 = load i32, ptr %7, align 4
  %68 = call i32 (ptr, ...) @printf(ptr noundef @.str.5, i32 noundef %66, i32 noundef %67)
  ret i32 0
}

declare i32 @printf(ptr noundef, ...) #1

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 16.0.4 (https://github.com/Lqs66/llvm-16.0.4.git a88e13dcefccbf25550e9c344fe27a9feefb4abc)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
