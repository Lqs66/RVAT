; 最小化的 llvm.type.test 测试代码

; 声明 llvm.type.test 内部函数
declare i1 @llvm.type.test(i8*, metadata) nounwind readnone
declare void @llvm.assume(i1) nounwind

; 主函数
define i32 @main() {
entry:
  ; 创建一个简单的指针
  %ptr = inttoptr i64 0 to i8*
  
  ; 调用 llvm.type.test
  %result = call i1 @llvm.type.test(i8* %ptr, metadata !"test_type")
  call void @llvm.assume(i1 %result)
  ; 将 i1 转换为 i32 作为返回值
  %result_i32 = zext i1 1 to i32
  
  ret i32 %result_i32
}

; 类型元数据定义
!0 = !{!"test_type"}
