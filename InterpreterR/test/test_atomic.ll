; test_atomic.ll
@shared = global i32 0, align 4
declare void @llvm.memset.p0i8.i64(i8*, i8, i64, i1)
declare i64 @sysconf(i32) nounwind readnone  ; nounwind 表示不抛异常，readnone 表示纯函数
declare i32 @printf(i8*, ...) nounwind        ; 声明 printf 函数用于输出

@.str = private unnamed_addr constant [22 x i8] c"CPU cores count: %ld\0A\00", align 1

define i32 @main() {
  %val = add i32 0, 1  ; %8 = 1
  %ptr = getelementptr i32, i32* @shared, i32 0
  %old = atomicrmw or i32* %ptr, i32 %val seq_cst, align 4
  %cpu = call i64 @sysconf(i32 84)
  call i32 @printf(i8* getelementptr inbounds ([22 x i8], [22 x i8]* @.str, i32 0, i32 0), i64 %cpu)
  ret i32 %old
}