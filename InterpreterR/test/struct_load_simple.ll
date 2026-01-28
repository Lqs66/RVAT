; ModuleID = 'struct_load_simple.c'
source_filename = "struct_load_simple.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Vector3 = type { float, float, float }

@__const.main.v1 = private unnamed_addr constant %struct.Vector3 { float 1.000000e+00, float 2.000000e+00, float 3.000000e+00 }, align 4
@.str = private unnamed_addr constant [32 x i8] c"Loaded: x=%.1f, y=%.1f, z=%.1f\0A\00", align 1

; Function Attrs: noinline nounwind uwtable
define dso_local { <2 x float>, float } @get_vector(ptr noundef %0) #0 {
  %2 = alloca %struct.Vector3, align 4
  %3 = alloca ptr, align 8
  %4 = alloca { <2 x float>, float }, align 8
  store ptr %0, ptr %3, align 8
  %5 = load ptr, ptr %3, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %2, ptr align 4 %5, i64 12, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %4, ptr align 4 %2, i64 12, i1 false)
  %6 = load { <2 x float>, float }, ptr %4, align 8
  ret { <2 x float>, float } %6
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #2 {
  %1 = alloca i32, align 4
  %2 = alloca %struct.Vector3, align 4
  %3 = alloca %struct.Vector3, align 4
  %4 = alloca { <2 x float>, float }, align 8
  store i32 0, ptr %1, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %2, ptr align 4 @__const.main.v1, i64 12, i1 false)
  %5 = call { <2 x float>, float } @get_vector(ptr noundef %2)
  store { <2 x float>, float } %5, ptr %4, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %3, ptr align 8 %4, i64 12, i1 false)
  %6 = getelementptr inbounds %struct.Vector3, ptr %3, i32 0, i32 0
  %7 = load float, ptr %6, align 4
  %8 = fpext float %7 to double
  %9 = getelementptr inbounds %struct.Vector3, ptr %3, i32 0, i32 1
  %10 = load float, ptr %9, align 4
  %11 = fpext float %10 to double
  %12 = getelementptr inbounds %struct.Vector3, ptr %3, i32 0, i32 2
  %13 = load float, ptr %12, align 4
  %14 = fpext float %13 to double
  %15 = call i32 (ptr, ...) @printf(ptr noundef @.str, double noundef %8, double noundef %11, double noundef %14)
  ret i32 0
}

declare i32 @printf(ptr noundef, ...) #3

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="64" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 16.0.4 (https://github.com/Lqs66/llvm-16.0.4.git eff546031917ab4b92384daf019b83e0007d6862)"}
