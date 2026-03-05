#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"

// ========================================
// Compute Shader 参数结构体
// ========================================
// 这个结构体定义了 C++ 和 HLSL 之间共享的参数
// 每个 SHADER_PARAMETER 宏对应 .usf 中的一个变量

BEGIN_SHADER_PARAMETER_STRUCT(FMyCSParameters, )
	// 对应 HLSL 中的: RWTexture2D<float4> OutputTexture;
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
	// 对应 HLSL 中的: float4 FillColor;
	SHADER_PARAMETER(FVector4f, FillColor)
	// 对应 HLSL 中的: int2 TextureSize;
	SHADER_PARAMETER(FIntPoint, TextureSize)
END_SHADER_PARAMETER_STRUCT()


// ========================================
// Compute Shader 类
// ========================================
class FMyComputeShaderCS : public FGlobalShader
{
public:
	// 声明为全局着色器
	DECLARE_GLOBAL_SHADER(FMyComputeShaderCS);

	// 使用上面定义的参数结构体
	using FParameters = FMyCSParameters;
	SHADER_USE_PARAMETER_STRUCT(FMyComputeShaderCS, FGlobalShader);

	// 编译条件：只在支持 SM5 的平台上编译
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	// 修改编译环境：传入线程组大小等宏定义
	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// 这些宏会在 .usf 中以 #define 形式出现
		OutEnvironment.SetDefine(TEXT("THREADS_X"), 8);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), 8);
	}
};