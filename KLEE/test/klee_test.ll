; ModuleID = 'klee_test.bc'
source_filename = "klee_test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.A = type { i32, i32, ptr }

@heap = dso_local global [16 x i8] zeroinitializer, align 16, !dbg !0
@a = dso_local global %struct.A zeroinitializer, align 8, !dbg !17
@symbolic_b = dso_local global i8 0, align 1, !dbg !24
@symbolic_c = dso_local global i32 0, align 4, !dbg !27
@ptr = dso_local global ptr null, align 8, !dbg !29
@.str = private unnamed_addr constant [11 x i8] c"symbolic_b\00", align 1, !dbg !31
@.str.1 = private unnamed_addr constant [11 x i8] c"symbolic_c\00", align 1, !dbg !38
@.str.2 = private unnamed_addr constant [5 x i8] c"heap\00", align 1, !dbg !40
@.str.3 = private unnamed_addr constant [11 x i8] c"symbolic_a\00", align 1, !dbg !45

; Function Attrs: mustprogress noinline norecurse optnone uwtable
define dso_local noundef i32 @main() #0 !dbg !58 {
  call void @klee_make_symbolic(ptr noundef @symbolic_b, i64 noundef 1, ptr noundef @.str), !dbg !62
  call void @klee_make_symbolic(ptr noundef @symbolic_c, i64 noundef 4, ptr noundef @.str.1), !dbg !63
  call void @klee_make_symbolic(ptr noundef @heap, i64 noundef 16, ptr noundef @.str.2), !dbg !64
  call void @klee_make_symbolic(ptr noundef @a, i64 noundef 16, ptr noundef @.str.3), !dbg !65
  store ptr getelementptr inbounds (i8, ptr @a, i64 4), ptr @ptr, align 8, !dbg !66
  store ptr getelementptr inbounds (i8, ptr @a, i64 4), ptr getelementptr inbounds (i8, ptr @a, i64 8), align 8, !dbg !67
  store ptr getelementptr inbounds (i8, ptr @a, i64 4), ptr @heap, align 16, !dbg !68
  store ptr getelementptr inbounds (i8, ptr @heap, i64 8), ptr getelementptr inbounds (i8, ptr @heap, i64 8), align 8, !dbg !69
  ret i32 0, !dbg !70
}

declare void @klee_make_symbolic(ptr noundef, i64 noundef, ptr noundef) #1

attributes #0 = { mustprogress noinline norecurse optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!50, !51, !52, !53, !54, !55, !56}
!llvm.ident = !{!57}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "heap", scope: !2, file: !3, line: 5, type: !47, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 16.0.4", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !4, globals: !16, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "klee_test.c", directory: "/home/lqs66/Desktop/modelCheckingFlightControl/KLEE", checksumkind: CSK_MD5, checksum: "f36913cdd02843cdc748b5cb784d5e5c")
!4 = !{!5, !7, !13, !14, !15}
!5 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !6, size: 64)
!6 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!7 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !8, size: 64)
!8 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint8_t", file: !9, line: 24, baseType: !10)
!9 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/stdint-uintn.h", directory: "", checksumkind: CSK_MD5, checksum: "2bf2ae53c58c01b1a1b9383b5195125c")
!10 = !DIDerivedType(tag: DW_TAG_typedef, name: "__uint8_t", file: !11, line: 38, baseType: !12)
!11 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types.h", directory: "", checksumkind: CSK_MD5, checksum: "d108b5f93a74c50510d7d9bc0ab36df9")
!12 = !DIBasicType(name: "unsigned char", size: 8, encoding: DW_ATE_unsigned_char)
!13 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!14 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !13, size: 64)
!15 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !7, size: 64)
!16 = !{!0, !17, !24, !27, !29, !31, !38, !40, !45}
!17 = !DIGlobalVariableExpression(var: !18, expr: !DIExpression())
!18 = distinct !DIGlobalVariable(name: "a", scope: !2, file: !3, line: 12, type: !19, isLocal: false, isDefinition: true)
!19 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "A", file: !3, line: 7, size: 128, flags: DIFlagTypePassByValue, elements: !20, identifier: "_ZTS1A")
!20 = !{!21, !22, !23}
!21 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !19, file: !3, line: 8, baseType: !6, size: 32)
!22 = !DIDerivedType(tag: DW_TAG_member, name: "b", scope: !19, file: !3, line: 9, baseType: !6, size: 32, offset: 32)
!23 = !DIDerivedType(tag: DW_TAG_member, name: "p", scope: !19, file: !3, line: 10, baseType: !5, size: 64, offset: 64)
!24 = !DIGlobalVariableExpression(var: !25, expr: !DIExpression())
!25 = distinct !DIGlobalVariable(name: "symbolic_b", scope: !2, file: !3, line: 13, type: !26, isLocal: false, isDefinition: true)
!26 = !DIBasicType(name: "bool", size: 8, encoding: DW_ATE_boolean)
!27 = !DIGlobalVariableExpression(var: !28, expr: !DIExpression())
!28 = distinct !DIGlobalVariable(name: "symbolic_c", scope: !2, file: !3, line: 14, type: !6, isLocal: false, isDefinition: true)
!29 = !DIGlobalVariableExpression(var: !30, expr: !DIExpression())
!30 = distinct !DIGlobalVariable(name: "ptr", scope: !2, file: !3, line: 15, type: !5, isLocal: false, isDefinition: true)
!31 = !DIGlobalVariableExpression(var: !32, expr: !DIExpression())
!32 = distinct !DIGlobalVariable(scope: null, file: !3, line: 18, type: !33, isLocal: true, isDefinition: true)
!33 = !DICompositeType(tag: DW_TAG_array_type, baseType: !34, size: 88, elements: !36)
!34 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !35)
!35 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!36 = !{!37}
!37 = !DISubrange(count: 11)
!38 = !DIGlobalVariableExpression(var: !39, expr: !DIExpression())
!39 = distinct !DIGlobalVariable(scope: null, file: !3, line: 20, type: !33, isLocal: true, isDefinition: true)
!40 = !DIGlobalVariableExpression(var: !41, expr: !DIExpression())
!41 = distinct !DIGlobalVariable(scope: null, file: !3, line: 22, type: !42, isLocal: true, isDefinition: true)
!42 = !DICompositeType(tag: DW_TAG_array_type, baseType: !34, size: 40, elements: !43)
!43 = !{!44}
!44 = !DISubrange(count: 5)
!45 = !DIGlobalVariableExpression(var: !46, expr: !DIExpression())
!46 = distinct !DIGlobalVariable(scope: null, file: !3, line: 25, type: !33, isLocal: true, isDefinition: true)
!47 = !DICompositeType(tag: DW_TAG_array_type, baseType: !8, size: 128, elements: !48)
!48 = !{!49}
!49 = !DISubrange(count: 16)
!50 = !{i32 7, !"Dwarf Version", i32 5}
!51 = !{i32 2, !"Debug Info Version", i32 3}
!52 = !{i32 1, !"wchar_size", i32 4}
!53 = !{i32 8, !"PIC Level", i32 2}
!54 = !{i32 7, !"PIE Level", i32 2}
!55 = !{i32 7, !"uwtable", i32 2}
!56 = !{i32 7, !"frame-pointer", i32 2}
!57 = !{!"clang version 16.0.4"}
!58 = distinct !DISubprogram(name: "main", scope: !3, file: !3, line: 16, type: !59, scopeLine: 16, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !2, retainedNodes: !61)
!59 = !DISubroutineType(types: !60)
!60 = !{!6}
!61 = !{}
!62 = !DILocation(line: 18, column: 5, scope: !58)
!63 = !DILocation(line: 20, column: 5, scope: !58)
!64 = !DILocation(line: 22, column: 5, scope: !58)
!65 = !DILocation(line: 25, column: 5, scope: !58)
!66 = !DILocation(line: 27, column: 9, scope: !58)
!67 = !DILocation(line: 29, column: 33, scope: !58)
!68 = !DILocation(line: 31, column: 23, scope: !58)
!69 = !DILocation(line: 32, column: 29, scope: !58)
!70 = !DILocation(line: 47, column: 1, scope: !58)
