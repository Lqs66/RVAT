; ModuleID = '/app/example.cpp'
source_filename = "/app/example.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [19 x i8] c"multiplyByTwo: %d\0A\00", align 1, !dbg !0
@.str.1 = private unnamed_addr constant [14 x i8] c"addByTwo: %d\0A\00", align 1, !dbg !8
@.str.2 = private unnamed_addr constant [10 x i8] c"main: %d\0A\00", align 1, !dbg !13

; Function Attrs: mustprogress noinline optnone uwtable
define dso_local noundef i32 @_Z13multiplyByTwoi(i32 noundef %0) #0 !dbg !29 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  call void @llvm.dbg.declare(metadata ptr %2, metadata !34, metadata !DIExpression()), !dbg !35
  %3 = load i32, ptr %2, align 4, !dbg !36
  %4 = mul nsw i32 %3, 2, !dbg !37
  %5 = call i32 (ptr, ...) @printf(ptr noundef @.str, i32 noundef %4), !dbg !38
  %6 = load i32, ptr %2, align 4, !dbg !39
  %7 = mul nsw i32 %6, 2, !dbg !40
  ret i32 %7, !dbg !41
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

declare i32 @printf(ptr noundef, ...) #2

; Function Attrs: mustprogress noinline optnone uwtable
define dso_local noundef i32 @_Z8addByTwoii(i32 noundef %0, i32 noundef %1) #0 !dbg !42 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  call void @llvm.dbg.declare(metadata ptr %3, metadata !45, metadata !DIExpression()), !dbg !46
  store i32 %1, ptr %4, align 4
  call void @llvm.dbg.declare(metadata ptr %4, metadata !47, metadata !DIExpression()), !dbg !48
  call void @llvm.dbg.declare(metadata ptr %5, metadata !49, metadata !DIExpression()), !dbg !50
  %6 = load i32, ptr %3, align 4, !dbg !51
  %7 = load i32, ptr %4, align 4, !dbg !52
  %8 = call noundef i32 @_Z13multiplyByTwoi(i32 noundef %7), !dbg !53
  %9 = add nsw i32 %6, %8, !dbg !54
  store i32 %9, ptr %5, align 4, !dbg !50
  %10 = load i32, ptr %5, align 4, !dbg !55
  %11 = call i32 (ptr, ...) @printf(ptr noundef @.str.1, i32 noundef %10), !dbg !56
  %12 = load i32, ptr %5, align 4, !dbg !57
  ret i32 %12, !dbg !58
}

; Function Attrs: mustprogress noinline norecurse optnone uwtable
define dso_local noundef i32 @main() #3 !dbg !59 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 0, ptr %1, align 4
  call void @llvm.dbg.declare(metadata ptr %2, metadata !62, metadata !DIExpression()), !dbg !63
  store i32 5, ptr %2, align 4, !dbg !63
  call void @llvm.dbg.declare(metadata ptr %3, metadata !64, metadata !DIExpression()), !dbg !65
  %4 = load i32, ptr %2, align 4, !dbg !66
  %5 = call noundef i32 @_Z8addByTwoii(i32 noundef %4, i32 noundef 10), !dbg !67
  store i32 %5, ptr %3, align 4, !dbg !65
  %6 = load i32, ptr %3, align 4, !dbg !68
  %7 = call i32 (ptr, ...) @printf(ptr noundef @.str.2, i32 noundef %6), !dbg !69
  ret i32 0, !dbg !70
}

attributes #0 = { mustprogress noinline optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #2 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { mustprogress noinline norecurse optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!18}
!llvm.module.flags = !{!21, !22, !23, !24, !25, !26, !27}
!llvm.ident = !{!28}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(scope: null, file: !2, line: 4, type: !3, isLocal: true, isDefinition: true)
!2 = !DIFile(filename: "example.cpp", directory: "/app", checksumkind: CSK_MD5, checksum: "87d6891d13711f74c70db6ef97a8efd2")
!3 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 152, elements: !6)
!4 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !5)
!5 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!6 = !{!7}
!7 = !DISubrange(count: 19)
!8 = !DIGlobalVariableExpression(var: !9, expr: !DIExpression())
!9 = distinct !DIGlobalVariable(scope: null, file: !2, line: 10, type: !10, isLocal: true, isDefinition: true)
!10 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 112, elements: !11)
!11 = !{!12}
!12 = !DISubrange(count: 14)
!13 = !DIGlobalVariableExpression(var: !14, expr: !DIExpression())
!14 = distinct !DIGlobalVariable(scope: null, file: !2, line: 19, type: !15, isLocal: true, isDefinition: true)
!15 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 80, elements: !16)
!16 = !{!17}
!17 = !DISubrange(count: 10)
!18 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !19, producer: "clang version 16.0.0 (https://github.com/llvm/llvm-project.git 08d094a0e457360ad8b94b017d2dc277e697ca76)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, globals: !20, splitDebugInlining: false, nameTableKind: None)
!19 = !DIFile(filename: "/app/example.cpp", directory: "/app", checksumkind: CSK_MD5, checksum: "87d6891d13711f74c70db6ef97a8efd2")
!20 = !{!0, !8, !13}
!21 = !{i32 7, !"Dwarf Version", i32 5}
!22 = !{i32 2, !"Debug Info Version", i32 3}
!23 = !{i32 1, !"wchar_size", i32 4}
!24 = !{i32 8, !"PIC Level", i32 2}
!25 = !{i32 7, !"PIE Level", i32 2}
!26 = !{i32 7, !"uwtable", i32 2}
!27 = !{i32 7, !"frame-pointer", i32 2}
!28 = !{!"clang version 16.0.0 (https://github.com/llvm/llvm-project.git 08d094a0e457360ad8b94b017d2dc277e697ca76)"}
!29 = distinct !DISubprogram(name: "multiplyByTwo", linkageName: "_Z13multiplyByTwoi", scope: !2, file: !2, line: 3, type: !30, scopeLine: 3, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !18, retainedNodes: !33)
!30 = !DISubroutineType(types: !31)
!31 = !{!32, !32}
!32 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!33 = !{}
!34 = !DILocalVariable(name: "number", arg: 1, scope: !29, file: !2, line: 3, type: !32)
!35 = !DILocation(line: 3, column: 23, scope: !29)
!36 = !DILocation(line: 4, column: 35, scope: !29)
!37 = !DILocation(line: 4, column: 42, scope: !29)
!38 = !DILocation(line: 4, column: 5, scope: !29)
!39 = !DILocation(line: 5, column: 12, scope: !29)
!40 = !DILocation(line: 5, column: 19, scope: !29)
!41 = !DILocation(line: 5, column: 5, scope: !29)
!42 = distinct !DISubprogram(name: "addByTwo", linkageName: "_Z8addByTwoii", scope: !2, file: !2, line: 8, type: !43, scopeLine: 8, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !18, retainedNodes: !33)
!43 = !DISubroutineType(types: !44)
!44 = !{!32, !32, !32}
!45 = !DILocalVariable(name: "a", arg: 1, scope: !42, file: !2, line: 8, type: !32)
!46 = !DILocation(line: 8, column: 18, scope: !42)
!47 = !DILocalVariable(name: "b", arg: 2, scope: !42, file: !2, line: 8, type: !32)
!48 = !DILocation(line: 8, column: 25, scope: !42)
!49 = !DILocalVariable(name: "ret", scope: !42, file: !2, line: 9, type: !32)
!50 = !DILocation(line: 9, column: 9, scope: !42)
!51 = !DILocation(line: 9, column: 15, scope: !42)
!52 = !DILocation(line: 9, column: 33, scope: !42)
!53 = !DILocation(line: 9, column: 19, scope: !42)
!54 = !DILocation(line: 9, column: 17, scope: !42)
!55 = !DILocation(line: 10, column: 30, scope: !42)
!56 = !DILocation(line: 10, column: 5, scope: !42)
!57 = !DILocation(line: 11, column: 12, scope: !42)
!58 = !DILocation(line: 11, column: 5, scope: !42)
!59 = distinct !DISubprogram(name: "main", scope: !2, file: !2, line: 14, type: !60, scopeLine: 14, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !18, retainedNodes: !33)
!60 = !DISubroutineType(types: !61)
!61 = !{!32}
!62 = !DILocalVariable(name: "inputNumber", scope: !59, file: !2, line: 15, type: !32)
!63 = !DILocation(line: 15, column: 9, scope: !59)
!64 = !DILocalVariable(name: "result", scope: !59, file: !2, line: 17, type: !32)
!65 = !DILocation(line: 17, column: 9, scope: !59)
!66 = !DILocation(line: 17, column: 27, scope: !59)
!67 = !DILocation(line: 17, column: 18, scope: !59)
!68 = !DILocation(line: 19, column: 26, scope: !59)
!69 = !DILocation(line: 19, column: 5, scope: !59)
!70 = !DILocation(line: 22, column: 5, scope: !59)