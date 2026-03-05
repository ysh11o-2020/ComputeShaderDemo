// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MyComputeShader : ModuleRules
{
	public MyComputeShader(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RenderCore",    // AddShaderSourceDirectoryMapping, FGlobalShader
				"RHI",           // FRHICommandList, GPU 资源类型
				"Projects"       // IPluginManager（用于定位插件路径）
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Renderer"       // 如果需要访问场景纹理等渲染管线内部类型
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
