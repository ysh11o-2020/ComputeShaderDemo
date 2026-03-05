#include "MyComputeShaderCS.h"

// ========================================
// 将 C++ 类绑定到 .usf 文件
// ========================================
// 参数说明：
//   FMyComputeShaderCS          - C++ 类名
//   "/MyComputeShader/Private/MyComputeShader.usf"  - 虚拟路径（对应磁盘上的 .usf 文件）
//   "MainCS"                    - .usf 中的入口函数名
//   SF_Compute                  - 着色器类型（Compute Shader）

IMPLEMENT_GLOBAL_SHADER(
	FMyComputeShaderCS,
	"/MyComputeShader/Private/MyComputeShader.usf",
	"MainCS",
	SF_Compute
);