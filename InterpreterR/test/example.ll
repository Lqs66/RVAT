; 声明外部函数
declare i64 @sysconf(i32) nounwind readnone  ; nounwind 表示不抛异常，readnone 表示纯函数
declare i32 @printf(i8*, ...) nounwind        ; 声明 printf 函数用于输出

; 定义格式化字符串常量
@.str = private unnamed_addr constant [22 x i8] c"CPU cores count: %ld\0A\00", align 1

define i64 @test_sysconf() {
  %name = add i32 0, 84  ; _SC_NPROCESSORS_ONLN = 84，获取在线 CPU 核心数
  %result = call i64 @sysconf(i32 %name)
  %fmt = getelementptr [22 x i8], [22 x i8]* @.str, i32 0, i32 0
  call i32 (i8*, ...) @printf(i8* %fmt, i64 %result)
  ret i64 %result
}

define i32 @main() {
  ; 调用 test_sysconf 获取 CPU 核心数
  %cpu = call i64 @test_sysconf()
  ; %cpu = add i64 0, 4
  
  ; 使用 printf 输出结果
  %fmt = getelementptr [22 x i8], [22 x i8]* @.str, i32 0, i32 0
  call i32 (i8*, ...) @printf(i8* %fmt, i64 %cpu)
  
  ret i32 0
}