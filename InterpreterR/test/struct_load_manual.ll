target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Vector3 = type { float, float, float }

@global_vec = global %struct.Vector3 { float 1.0, float 2.0, float 3.0 }, align 4

declare i32 @printf(i8*, ...)

@.str = private constant [34 x i8] c"x=%.1f, y=%.1f, z=%.1f, sum=%.1f\0A\00", align 1

define i32 @main() {
entry:
  %local_vec = alloca %struct.Vector3, align 4
  
  %loaded = load %struct.Vector3, %struct.Vector3* @global_vec, align 4
  
  store %struct.Vector3 %loaded, %struct.Vector3* %local_vec, align 4
  
  %x_ptr = getelementptr inbounds %struct.Vector3, %struct.Vector3* %local_vec, i32 0, i32 0
  %x = load float, float* %x_ptr, align 4
  
  %y_ptr = getelementptr inbounds %struct.Vector3, %struct.Vector3* %local_vec, i32 0, i32 1
  %y = load float, float* %y_ptr, align 4
  
  %z_ptr = getelementptr inbounds %struct.Vector3, %struct.Vector3* %local_vec, i32 0, i32 2
  %z = load float, float* %z_ptr, align 4
  
  %sum1 = fadd float %x, %y
  %sum = fadd float %sum1, %z
  
  %x_d = fpext float %x to double
  %y_d = fpext float %y to double
  %z_d = fpext float %z to double
  %sum_d = fpext float %sum to double
  
  %fmt = getelementptr [34 x i8], [34 x i8]* @.str, i32 0, i32 0
  call i32 (i8*, ...) @printf(i8* %fmt, double %x_d, double %y_d, double %z_d, double %sum_d)
  
  ret i32 0
}
