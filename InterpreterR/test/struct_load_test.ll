; ModuleID = 'struct_load_test.c'
source_filename = "struct_load_test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Vector3 = type { float, float, float }

@global_vec = dso_local global %struct.Vector3 { float 1.000000e+00, float 2.000000e+00, float 3.000000e+00 }, align 4
@.str = private unnamed_addr constant [39 x i8] c"Global vector: x=%.1f, y=%.1f, z=%.1f\0A\00", align 1
@.str.1 = private unnamed_addr constant [28 x i8] c"Sum of global vector: %.1f\0A\00", align 1
@__const.main.v2 = private unnamed_addr constant %struct.Vector3 { float 4.000000e+00, float 5.000000e+00, float 6.000000e+00 }, align 4
@.str.2 = private unnamed_addr constant [38 x i8] c"Local vector: x=%.1f, y=%.1f, z=%.1f\0A\00", align 1
@.str.3 = private unnamed_addr constant [27 x i8] c"Sum of local vector: %.1f\0A\00", align 1
@.str.4 = private unnamed_addr constant [44 x i8] c"Loaded via pointer: x=%.1f, y=%.1f, z=%.1f\0A\00", align 1
@.str.5 = private unnamed_addr constant [44 x i8] c"After modification: x=%.1f, y=%.1f, z=%.1f\0A\00", align 1
@.str.6 = private unnamed_addr constant [30 x i8] c"Sum after modification: %.1f\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local float @sum_vector(<2 x float> %0, float %1) #0 {
  %3 = alloca %struct.Vector3, align 4
  %4 = alloca { <2 x float>, float }, align 4
  %5 = getelementptr inbounds { <2 x float>, float }, ptr %4, i32 0, i32 0
  store <2 x float> %0, ptr %5, align 4
  %6 = getelementptr inbounds { <2 x float>, float }, ptr %4, i32 0, i32 1
  store float %1, ptr %6, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %3, ptr align 4 %4, i64 12, i1 false)
  %7 = getelementptr inbounds %struct.Vector3, ptr %3, i32 0, i32 0
  %8 = load float, ptr %7, align 4
  %9 = getelementptr inbounds %struct.Vector3, ptr %3, i32 0, i32 1
  %10 = load float, ptr %9, align 4
  %11 = fadd float %8, %10
  %12 = getelementptr inbounds %struct.Vector3, ptr %3, i32 0, i32 2
  %13 = load float, ptr %12, align 4
  %14 = fadd float %11, %13
  ret float %14
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca %struct.Vector3, align 4
  %3 = alloca { <2 x float>, float }, align 4
  %4 = alloca %struct.Vector3, align 4
  %5 = alloca { <2 x float>, float }, align 4
  %6 = alloca ptr, align 8
  %7 = alloca %struct.Vector3, align 4
  %8 = alloca { <2 x float>, float }, align 4
  store i32 0, ptr %1, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %2, ptr align 4 @global_vec, i64 12, i1 false)
  %9 = getelementptr inbounds %struct.Vector3, ptr %2, i32 0, i32 0
  %10 = load float, ptr %9, align 4
  %11 = fpext float %10 to double
  %12 = getelementptr inbounds %struct.Vector3, ptr %2, i32 0, i32 1
  %13 = load float, ptr %12, align 4
  %14 = fpext float %13 to double
  %15 = getelementptr inbounds %struct.Vector3, ptr %2, i32 0, i32 2
  %16 = load float, ptr %15, align 4
  %17 = fpext float %16 to double
  %18 = call i32 (ptr, ...) @printf(ptr noundef @.str, double noundef %11, double noundef %14, double noundef %17)
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %3, ptr align 4 %2, i64 12, i1 false)
  %19 = getelementptr inbounds { <2 x float>, float }, ptr %3, i32 0, i32 0
  %20 = load <2 x float>, ptr %19, align 4
  %21 = getelementptr inbounds { <2 x float>, float }, ptr %3, i32 0, i32 1
  %22 = load float, ptr %21, align 4
  %23 = call float @sum_vector(<2 x float> %20, float %22)
  %24 = fpext float %23 to double
  %25 = call i32 (ptr, ...) @printf(ptr noundef @.str.1, double noundef %24)
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %4, ptr align 4 @__const.main.v2, i64 12, i1 false)
  %26 = getelementptr inbounds %struct.Vector3, ptr %4, i32 0, i32 0
  %27 = load float, ptr %26, align 4
  %28 = fpext float %27 to double
  %29 = getelementptr inbounds %struct.Vector3, ptr %4, i32 0, i32 1
  %30 = load float, ptr %29, align 4
  %31 = fpext float %30 to double
  %32 = getelementptr inbounds %struct.Vector3, ptr %4, i32 0, i32 2
  %33 = load float, ptr %32, align 4
  %34 = fpext float %33 to double
  %35 = call i32 (ptr, ...) @printf(ptr noundef @.str.2, double noundef %28, double noundef %31, double noundef %34)
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %5, ptr align 4 %4, i64 12, i1 false)
  %36 = getelementptr inbounds { <2 x float>, float }, ptr %5, i32 0, i32 0
  %37 = load <2 x float>, ptr %36, align 4
  %38 = getelementptr inbounds { <2 x float>, float }, ptr %5, i32 0, i32 1
  %39 = load float, ptr %38, align 4
  %40 = call float @sum_vector(<2 x float> %37, float %39)
  %41 = fpext float %40 to double
  %42 = call i32 (ptr, ...) @printf(ptr noundef @.str.3, double noundef %41)
  store ptr %4, ptr %6, align 8
  %43 = load ptr, ptr %6, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %7, ptr align 4 %43, i64 12, i1 false)
  %44 = getelementptr inbounds %struct.Vector3, ptr %7, i32 0, i32 0
  %45 = load float, ptr %44, align 4
  %46 = fpext float %45 to double
  %47 = getelementptr inbounds %struct.Vector3, ptr %7, i32 0, i32 1
  %48 = load float, ptr %47, align 4
  %49 = fpext float %48 to double
  %50 = getelementptr inbounds %struct.Vector3, ptr %7, i32 0, i32 2
  %51 = load float, ptr %50, align 4
  %52 = fpext float %51 to double
  %53 = call i32 (ptr, ...) @printf(ptr noundef @.str.4, double noundef %46, double noundef %49, double noundef %52)
  %54 = getelementptr inbounds %struct.Vector3, ptr %4, i32 0, i32 0
  store float 1.000000e+01, ptr %54, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %7, ptr align 4 %4, i64 12, i1 false)
  %55 = getelementptr inbounds %struct.Vector3, ptr %7, i32 0, i32 0
  %56 = load float, ptr %55, align 4
  %57 = fpext float %56 to double
  %58 = getelementptr inbounds %struct.Vector3, ptr %7, i32 0, i32 1
  %59 = load float, ptr %58, align 4
  %60 = fpext float %59 to double
  %61 = getelementptr inbounds %struct.Vector3, ptr %7, i32 0, i32 2
  %62 = load float, ptr %61, align 4
  %63 = fpext float %62 to double
  %64 = call i32 (ptr, ...) @printf(ptr noundef @.str.5, double noundef %57, double noundef %60, double noundef %63)
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %8, ptr align 4 %7, i64 12, i1 false)
  %65 = getelementptr inbounds { <2 x float>, float }, ptr %8, i32 0, i32 0
  %66 = load <2 x float>, ptr %65, align 4
  %67 = getelementptr inbounds { <2 x float>, float }, ptr %8, i32 0, i32 1
  %68 = load float, ptr %67, align 4
  %69 = call float @sum_vector(<2 x float> %66, float %68)
  %70 = fpext float %69 to double
  %71 = call i32 (ptr, ...) @printf(ptr noundef @.str.6, double noundef %70)
  ret i32 0
}

declare i32 @printf(ptr noundef, ...) #2

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="64" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 16.0.4 (https://github.com/Lqs66/llvm-16.0.4.git eff546031917ab4b92384daf019b83e0007d6862)"}
