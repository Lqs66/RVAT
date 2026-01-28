; ModuleID = '/app/example.cpp'
source_filename = "/app/example.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [20 x i8] c"current n is: %lld\0A\00", align 1, !dbg !0
@.str.1 = private unnamed_addr constant [22 x i8] c"%d \E7\9A\84\E9\98\B6\E4\B9\98\E6\98\AF %lld\0A\00", align 1, !dbg !8

; Function Attrs: mustprogress noinline optnone uwtable
define dso_local noundef i64 @_Z9factoriali(i32 noundef %0) #0 !dbg !24 {
  %2 = alloca i64, align 8
  %3 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  call void @llvm.dbg.declare(metadata ptr %3, metadata !30, metadata !DIExpression()), !dbg !31
  %4 = load i32, ptr %3, align 4, !dbg !32
  %5 = icmp eq i32 %4, 0, !dbg !34
  br i1 %5, label %6, label %7, !dbg !35

6:                                                ; preds = %1
  store i64 1, ptr %2, align 8, !dbg !36
  br label %16, !dbg !36

7:                                                ; preds = %1
  %8 = load i32, ptr %3, align 4, !dbg !38
  %9 = call i32 (ptr, ...) @printf(ptr noundef @.str, i32 noundef %8), !dbg !39
  %10 = load i32, ptr %3, align 4, !dbg !40
  %11 = sext i32 %10 to i64, !dbg !40
  %12 = load i32, ptr %3, align 4, !dbg !41
  %13 = sub nsw i32 %12, 1, !dbg !42
  %14 = call noundef i64 @_Z9factoriali(i32 noundef %13), !dbg !43
  %15 = mul nsw i64 %11, %14, !dbg !44
  store i64 %15, ptr %2, align 8, !dbg !45
  br label %16, !dbg !45

16:                                               ; preds = %7, %6
  %17 = load i64, ptr %2, align 8, !dbg !46
  ret i64 %17, !dbg !46
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

declare i32 @printf(ptr noundef, ...) #2

; Function Attrs: mustprogress noinline norecurse optnone uwtable
define dso_local noundef i32 @main() #3 !dbg !47 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i64, align 8
  store i32 0, ptr %1, align 4
  call void @llvm.dbg.declare(metadata ptr %2, metadata !50, metadata !DIExpression()), !dbg !51
  store i32 10, ptr %2, align 4, !dbg !51
  call void @llvm.dbg.declare(metadata ptr %3, metadata !52, metadata !DIExpression()), !dbg !53
  %4 = load i32, ptr %2, align 4, !dbg !54
  %5 = call noundef i64 @_Z9factoriali(i32 noundef %4), !dbg !55
  store i64 %5, ptr %3, align 8, !dbg !56
  %6 = load i32, ptr %2, align 4, !dbg !57
  %7 = load i64, ptr %3, align 8, !dbg !58
  %8 = call i32 (ptr, ...) @printf(ptr noundef @.str.1, i32 noundef %6, i64 noundef %7), !dbg !59
  ret i32 0, !dbg !60
}

attributes #0 = { mustprogress noinline optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #2 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { mustprogress noinline norecurse optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!13}
!llvm.module.flags = !{!16, !17, !18, !19, !20, !21, !22}
!llvm.ident = !{!23}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(scope: null, file: !2, line: 10, type: !3, isLocal: true, isDefinition: true)
!2 = !DIFile(filename: "example.cpp", directory: "/app", checksumkind: CSK_MD5, checksum: "4def05cccd1d30514954573c26c89a0b")
!3 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 160, elements: !6)
!4 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !5)
!5 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!6 = !{!7}
!7 = !DISubrange(count: 20)
!8 = !DIGlobalVariableExpression(var: !9, expr: !DIExpression())
!9 = distinct !DIGlobalVariable(scope: null, file: !2, line: 22, type: !10, isLocal: true, isDefinition: true)
!10 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 176, elements: !11)
!11 = !{!12}
!12 = !DISubrange(count: 22)
!13 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !14, producer: "clang version 16.0.0 (https://github.com/llvm/llvm-project.git 08d094a0e457360ad8b94b017d2dc277e697ca76)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, globals: !15, splitDebugInlining: false, nameTableKind: None)
!14 = !DIFile(filename: "/app/example.cpp", directory: "/app", checksumkind: CSK_MD5, checksum: "4def05cccd1d30514954573c26c89a0b")
!15 = !{!0, !8}
!16 = !{i32 7, !"Dwarf Version", i32 5}
!17 = !{i32 2, !"Debug Info Version", i32 3}
!18 = !{i32 1, !"wchar_size", i32 4}
!19 = !{i32 8, !"PIC Level", i32 2}
!20 = !{i32 7, !"PIE Level", i32 2}
!21 = !{i32 7, !"uwtable", i32 2}
!22 = !{i32 7, !"frame-pointer", i32 2}
!23 = !{!"clang version 16.0.0 (https://github.com/llvm/llvm-project.git 08d094a0e457360ad8b94b017d2dc277e697ca76)"}
!24 = distinct !DISubprogram(name: "factorial", linkageName: "_Z9factoriali", scope: !2, file: !2, line: 4, type: !25, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !13, retainedNodes: !29)
!25 = !DISubroutineType(types: !26)
!26 = !{!27, !28}
!27 = !DIBasicType(name: "long long", size: 64, encoding: DW_ATE_signed)
!28 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!29 = !{}
!30 = !DILocalVariable(name: "n", arg: 1, scope: !24, file: !2, line: 4, type: !28)
!31 = !DILocation(line: 4, column: 25, scope: !24)
!32 = !DILocation(line: 7, column: 9, scope: !33)
!33 = distinct !DILexicalBlock(scope: !24, file: !2, line: 7, column: 9)
!34 = !DILocation(line: 7, column: 11, scope: !33)
!35 = !DILocation(line: 7, column: 9, scope: !24)
!36 = !DILocation(line: 8, column: 9, scope: !37)
!37 = distinct !DILexicalBlock(scope: !33, file: !2, line: 7, column: 17)
!38 = !DILocation(line: 10, column: 36, scope: !24)
!39 = !DILocation(line: 10, column: 5, scope: !24)
!40 = !DILocation(line: 14, column: 12, scope: !24)
!41 = !DILocation(line: 14, column: 26, scope: !24)
!42 = !DILocation(line: 14, column: 28, scope: !24)
!43 = !DILocation(line: 14, column: 16, scope: !24)
!44 = !DILocation(line: 14, column: 14, scope: !24)
!45 = !DILocation(line: 14, column: 5, scope: !24)
!46 = !DILocation(line: 15, column: 1, scope: !24)
!47 = distinct !DISubprogram(name: "main", scope: !2, file: !2, line: 18, type: !48, scopeLine: 18, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !13, retainedNodes: !29)
!48 = !DISubroutineType(types: !49)
!49 = !{!28}
!50 = !DILocalVariable(name: "num", scope: !47, file: !2, line: 19, type: !28)
!51 = !DILocation(line: 19, column: 9, scope: !47)
!52 = !DILocalVariable(name: "result", scope: !47, file: !2, line: 20, type: !27)
!53 = !DILocation(line: 20, column: 15, scope: !47)
!54 = !DILocation(line: 21, column: 24, scope: !47)
!55 = !DILocation(line: 21, column: 14, scope: !47)
!56 = !DILocation(line: 21, column: 12, scope: !47)
!57 = !DILocation(line: 22, column: 38, scope: !47)
!58 = !DILocation(line: 22, column: 43, scope: !47)
!59 = !DILocation(line: 22, column: 5, scope: !47)
!60 = !DILocation(line: 24, column: 5, scope: !47)