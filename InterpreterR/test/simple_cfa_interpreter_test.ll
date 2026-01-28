; ModuleID = '/app/example.cpp'
source_filename = "/app/example.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: mustprogress noinline norecurse nounwind optnone uwtable
define dso_local noundef i32 @main() #0 !dbg !10 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  call void @llvm.dbg.declare(metadata ptr %1, metadata !16, metadata !DIExpression()), !dbg !17
  call void @llvm.dbg.declare(metadata ptr %2, metadata !18, metadata !DIExpression()), !dbg !19
  store i32 1, ptr %1, align 4, !dbg !20
  %3 = load i32, ptr %1, align 4, !dbg !21
  store i32 %3, ptr %2, align 4, !dbg !22
  %4 = load i32, ptr %1, align 4, !dbg !23
  %5 = load i32, ptr %2, align 4, !dbg !24
  %6 = add nsw i32 %4, %5, !dbg !25
  store i32 %6, ptr %1, align 4, !dbg !26
  %7 = load i32, ptr %1, align 4, !dbg !27
  %8 = load i32, ptr %1, align 4, !dbg !28
  %9 = mul nsw i32 %7, %8, !dbg !29
  store i32 %9, ptr %2, align 4, !dbg !30
  %10 = load i32, ptr %2, align 4, !dbg !31
  %11 = load i32, ptr %1, align 4, !dbg !32
  %12 = load i32, ptr %2, align 4, !dbg !33
  %13 = sub nsw i32 %11, %12, !dbg !34
  %14 = sdiv i32 %10, %13, !dbg !35
  store i32 %14, ptr %1, align 4, !dbg !36
  ret i32 0, !dbg !37
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = { mustprogress noinline norecurse nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 16.0.0 (https://github.com/llvm/llvm-project.git 08d094a0e457360ad8b94b017d2dc277e697ca76)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/app/example.cpp", directory: "/app", checksumkind: CSK_MD5, checksum: "aab5d73343be10ac47f8d565dcab7fd4")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 8, !"PIC Level", i32 2}
!6 = !{i32 7, !"PIE Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 2}
!8 = !{i32 7, !"frame-pointer", i32 2}
!9 = !{!"clang version 16.0.0 (https://github.com/llvm/llvm-project.git 08d094a0e457360ad8b94b017d2dc277e697ca76)"}
!10 = distinct !DISubprogram(name: "main", scope: !11, file: !11, line: 2, type: !12, scopeLine: 2, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !15)
!11 = !DIFile(filename: "example.cpp", directory: "/app", checksumkind: CSK_MD5, checksum: "aab5d73343be10ac47f8d565dcab7fd4")
!12 = !DISubroutineType(types: !13)
!13 = !{!14}
!14 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!15 = !{}
!16 = !DILocalVariable(name: "a", scope: !10, file: !11, line: 3, type: !14)
!17 = !DILocation(line: 3, column: 9, scope: !10)
!18 = !DILocalVariable(name: "b", scope: !10, file: !11, line: 3, type: !14)
!19 = !DILocation(line: 3, column: 11, scope: !10)
!20 = !DILocation(line: 4, column: 7, scope: !10)
!21 = !DILocation(line: 5, column: 9, scope: !10)
!22 = !DILocation(line: 5, column: 7, scope: !10)
!23 = !DILocation(line: 6, column: 9, scope: !10)
!24 = !DILocation(line: 6, column: 13, scope: !10)
!25 = !DILocation(line: 6, column: 11, scope: !10)
!26 = !DILocation(line: 6, column: 7, scope: !10)
!27 = !DILocation(line: 7, column: 9, scope: !10)
!28 = !DILocation(line: 7, column: 13, scope: !10)
!29 = !DILocation(line: 7, column: 11, scope: !10)
!30 = !DILocation(line: 7, column: 7, scope: !10)
!31 = !DILocation(line: 8, column: 9, scope: !10)
!32 = !DILocation(line: 8, column: 14, scope: !10)
!33 = !DILocation(line: 8, column: 18, scope: !10)
!34 = !DILocation(line: 8, column: 16, scope: !10)
!35 = !DILocation(line: 8, column: 11, scope: !10)
!36 = !DILocation(line: 8, column: 7, scope: !10)
!37 = !DILocation(line: 9, column: 1, scope: !10)