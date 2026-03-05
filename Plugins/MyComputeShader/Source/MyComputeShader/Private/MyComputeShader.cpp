// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyComputeShader.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FMyComputeShaderModule"

void FMyComputeShaderModule::StartupModule()
{
	// 定位插件的 Shaders 目录
	FString PluginShaderDir = FPaths::Combine(
		IPluginManager::Get().FindPlugin(TEXT("MyComputeShader"))->GetBaseDir(),
		TEXT("Shaders")
	);

	// 将磁盘路径映射到虚拟路径
	// 虚拟路径必须以 / 开头，不能以 / 结尾
	AddShaderSourceDirectoryMapping(TEXT("/MyComputeShader"), PluginShaderDir);
}

void FMyComputeShaderModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMyComputeShaderModule, MyComputeShader)