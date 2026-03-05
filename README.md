# UE5.3 Compute Shader 完整教程（从零开始）

本教程将在 UE5.3 中从零创建一个 Compute Shader 插件，实现一个简单的功能：用 GPU 将一张 Render Target 填充为指定颜色。涵盖项目结构、模块配置、C++ 代码和 HLSL 着色器的全部步骤。

> **本教程已经过实际编译验证，修复了常见的坑点。**

---

## 第一步：创建插件

1. 打开 UE5.3 编辑器，点击 **Edit → Plugins → Add（左下角 + 按钮）**
2. 选择 **Blank** 模板
3. 插件名称填 `MyComputeShader`
4. 点击 Create Plugin
5. **关闭编辑器**（后续需要手动修改文件）

---

## 第二步：目录结构

创建完成后，你的插件目录应该如下组织：

```
项目根目录/
└── Plugins/
    └── MyComputeShader/
        ├── MyComputeShader.uplugin
        ├── Shaders/                          ← 手动创建
        │   └── Private/                      ← 手动创建
        │       └── MyComputeShader.usf       ← 手动创建
        └── Source/
            └── MyComputeShader/
                ├── MyComputeShader.Build.cs
                ├── Public/
                │   ├── MyComputeShader.h      (模块头文件)
                │   ├── MyComputeShaderCS.h    ← 手动创建（Shader 声明）
                │   ├── MyCSDispatcher.h       ← 手动创建（调度器声明）
                │   └── MyCSBlueprintLibrary.h ← 手动创建（蓝图接口）
                └── Private/
                    ├── MyComputeShader.cpp     (模块实现)
                    ├── MyComputeShaderCS.cpp   ← 手动创建（Shader 实现）
                    ├── MyCSDispatcher.cpp      ← 手动创建（调度器实现）
                    └── MyCSBlueprintLibrary.cpp← 手动创建（蓝图接口实现）
```

**手动创建 `Shaders/Private/` 目录**，这是放 `.usf` 文件的地方。

> ⚠️ **重要**：Shader 相关的调度代码（Dispatcher）应该放在**插件模块**内，而不是游戏模块内。
> 如果放在游戏模块中，游戏模块的 Build.cs 也必须添加 `RenderCore`、`RHI`、`Renderer` 依赖，否则会产生大量 LNK2019 链接错误。

---

## 第三步：修改 `.uplugin` 文件

打开 `MyComputeShader.uplugin`，**关键修改是 LoadingPhase**：

```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "1.0",
    "FriendlyName": "MyComputeShader",
    "Description": "A simple compute shader plugin",
    "Category": "Rendering",
    "CreatedBy": "YourName",
    "CanContainContent": false,
    "IsBetaVersion": false,
    "Installed": false,
    "Modules": [
        {
            "Name": "MyComputeShader",
            "Type": "Runtime",
            "LoadingPhase": "PostConfigInit"
        }
    ]
}
```

> **为什么要 `PostConfigInit`？**
> 因为 `AddShaderSourceDirectoryMapping` 必须在引擎编译着色器之前调用。
> `PostConfigInit` 是配置系统初始化之后、引擎完全启动之前的阶段。
> 如果使用默认的 `Default` 加载阶段，调用时机太晚，着色器目录映射不会生效。

---

## 第四步：修改 Build.cs

打开 `Source/MyComputeShader/MyComputeShader.Build.cs`：

```csharp
using UnrealBuildTool;

public class MyComputeShader : ModuleRules
{
    public MyComputeShader(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",    // FRDGBuilder, FGlobalShader, AddShaderSourceDirectoryMapping
            "RHI",           // FRHICommandList, GMaxRHIFeatureLevel, GPU 资源类型
            "Projects"       // IPluginManager（用于定位插件路径）
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Renderer"       // SetComputePipelineState, SetShaderParameters, GetGlobalShaderMap
        });
    }
}
```

> ⚠️ **三个渲染模块缺一不可：**
>
> | 模块 | 提供的关键符号 |
> |------|---------------|
> | `RenderCore` | `FRDGBuilder`、`FRDGPass`、`FRDGEventName`、`CreateRenderTarget`、`FShaderParametersMetadata` |
> | `RHI` | `GMaxRHIFeatureLevel`、`GRHICommandList`、`FRHICommandDispatchComputeShader`、`GIsThreadedRendering` |
> | `Renderer` | `SetComputePipelineState`、`SetShaderParameters`、`ValidateShaderParameters`、`GetGlobalShaderMap` |
>
> 缺少任何一个都会导致大量 LNK2019 链接错误。

---

## 第五步：模块头文件

**`Public/MyComputeShader.h`：**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMyComputeShaderModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
```

---

## 第六步：模块实现

这是最关键的文件之一，负责注册着色器目录映射。

**`Private/MyComputeShader.cpp`：**

```cpp
#include "MyComputeShader.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"

void FMyComputeShaderModule::StartupModule()
{
    // 方案 A：通过 IPluginManager 定位路径（推荐）
    // 注意：极少数情况下 FindPlugin 可能在 PostConfigInit 阶段返回空指针导致崩溃
    // 如果遇到 Exception 0x80000003 崩溃，请改用方案 B
    
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
}

// 注册模块（名称必须和 Build.cs 中的模块名一致）
IMPLEMENT_MODULE(FMyComputeShaderModule, MyComputeShader)
```

> **如果 `FindPlugin` 导致运行时崩溃（Exception 0x80000003）**，使用方案 B：
> ```cpp
> void FMyComputeShaderModule::StartupModule()
> {
>     // 方案 B：直接拼接路径（更稳妥）
>     FString PluginShaderDir = FPaths::Combine(
>         FPaths::ProjectPluginsDir(),
>         TEXT("MyComputeShader"),
>         TEXT("Shaders")
>     );
>
>     AddShaderSourceDirectoryMapping(TEXT("/MyComputeShader"), PluginShaderDir);
> }
> ```

> **路径映射关系说明：**
>
> | 虚拟路径 | 磁盘路径 |
> |---------|---------|
> | `/MyComputeShader` | `.../Plugins/MyComputeShader/Shaders/` |
> | `/MyComputeShader/Private/MyComputeShader.usf` | `.../Shaders/Private/MyComputeShader.usf` |
>
> ⚠️ **常见错误**：把磁盘路径指向 `Shaders/Private` 而不是 `Shaders`。
> 如果映射到 `Shaders/Private`，引擎会把 `/MyComputeShader/Private/xxx.usf` 解析为
> `.../Shaders/Private/Private/xxx.usf`，多了一层 Private，找不到文件。

---

## 第七步：Shader 声明

**`Public/MyComputeShaderCS.h`：**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"

// ========================================
// Compute Shader 参数结构体
// ========================================
// 这个结构体定义了 C++ 和 HLSL 之间共享的参数
// 每个 SHADER_PARAMETER 宏对应 .usf 中的一个变量
// ⚠️ 名字必须和 .usf 中完全一致，否则绑定失败

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

        // ⚠️ 不要使用 SET_SHADER_DEFINE 宏，UE5 中不存在
        // 正确写法是 OutEnvironment.SetDefine
        OutEnvironment.SetDefine(TEXT("THREADS_X"), 8);
        OutEnvironment.SetDefine(TEXT("THREADS_Y"), 8);
    }
};
```

---

## 第八步：Shader 实现

**`Private/MyComputeShaderCS.cpp`：**

```cpp
#include "MyComputeShaderCS.h"

// ========================================
// 将 C++ 类绑定到 .usf 文件
// ========================================
// 参数说明：
//   FMyComputeShaderCS                              - C++ 类名
//   "/MyComputeShader/Private/MyComputeShader.usf"  - 虚拟路径
//   "MainCS"                                        - .usf 中的入口函数名
//   SF_Compute                                      - 着色器类型

IMPLEMENT_GLOBAL_SHADER(
    FMyComputeShaderCS,
    "/MyComputeShader/Private/MyComputeShader.usf",
    "MainCS",
    SF_Compute
);
```

---

## 第九步：HLSL 着色器

**`Shaders/Private/MyComputeShader.usf`：**

```hlsl
// ========================================
// MyComputeShader.usf
// ========================================
// THREADS_X 和 THREADS_Y 由 C++ 端的 ModifyCompilationEnvironment 定义

#include "/Engine/Public/Platform.ush"

// 参数：名字必须和 C++ 的 BEGIN_SHADER_PARAMETER_STRUCT 中完全一致
RWTexture2D<float4> OutputTexture;
float4 FillColor;
int2 TextureSize;

[numthreads(THREADS_X, THREADS_Y, 1)]
void MainCS(
    uint3 DispatchThreadId : SV_DispatchThreadID,
    uint3 GroupThreadId    : SV_GroupThreadID,
    uint3 GroupId          : SV_GroupID)
{
    // 边界检查：超出纹理范围的线程直接返回
    if (DispatchThreadId.x >= (uint)TextureSize.x ||
        DispatchThreadId.y >= (uint)TextureSize.y)
    {
        return;
    }

    // 写入颜色到输出纹理
    OutputTexture[DispatchThreadId.xy] = FillColor;
}
```

---

## 第十步：调度 Compute Shader（使用 RDG）

### 10.1 调度器声明

**`Public/MyCSDispatcher.h`：**

```cpp
#pragma once

#include "CoreMinimal.h"

class UTextureRenderTarget2D;

class MYCOMPUTESHADER_API FMyCSDispatcher
{
public:
    static void Dispatch(
        UTextureRenderTarget2D* RenderTarget,
        FLinearColor Color
    );
};
```

### 10.2 调度器实现

**`Private/MyCSDispatcher.cpp`：**

```cpp
#include "MyCSDispatcher.h"
#include "MyComputeShaderCS.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GlobalShader.h"

void FMyCSDispatcher::Dispatch(
    UTextureRenderTarget2D* RenderTarget,
    FLinearColor Color)
{
    if (!RenderTarget) return;

    FTextureRenderTargetResource* RTResource =
        RenderTarget->GameThread_GetRenderTargetResource();
    if (!RTResource) return;

    FIntPoint Size = FIntPoint(RenderTarget->SizeX, RenderTarget->SizeY);
    FVector4f FillColor = FVector4f(Color.R, Color.G, Color.B, Color.A);

    // 将渲染命令提交到渲染线程
    ENQUEUE_RENDER_COMMAND(MyComputeShaderCommand)(
        [RTResource, Size, FillColor](FRHICommandListImmediate& RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList);

            // ========================================
            // ⚠️ 关键：不要直接对 RenderTarget 创建 UAV
            // ========================================
            // 很多 RenderTarget 创建时没有 TexCreate_UAV 标志，
            // 直接 RegisterExternalTexture 后调用 CreateUAV 会导致崩溃。
            // 
            // 正确做法：创建一个带 UAV 标志的临时纹理，
            // Compute Shader 写入临时纹理，最后拷贝回 RenderTarget。

            FRHITexture* RHITexture = RTResource->GetRenderTargetTexture();
            if (!RHITexture) return;

            // 创建带 UAV 标志的临时 RDG 纹理
            FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
                Size,
                RHITexture->GetFormat(),
                FClearValueBinding::Black,
                TexCreate_ShaderResource | TexCreate_UAV
            );
            FRDGTextureRef RDGTexture = GraphBuilder.CreateTexture(
                Desc, TEXT("MyCS_TempOutput")
            );
            FRDGTextureUAVRef TextureUAV = GraphBuilder.CreateUAV(RDGTexture);

            // 设置 Shader 参数
            FMyCSParameters* Parameters =
                GraphBuilder.AllocParameters<FMyCSParameters>();
            Parameters->OutputTexture = TextureUAV;
            Parameters->FillColor = FillColor;
            Parameters->TextureSize = Size;

            // 获取 Shader 引用
            TShaderMapRef<FMyComputeShaderCS> ComputeShader(
                GetGlobalShaderMap(GMaxRHIFeatureLevel)
            );

            // 计算调度的线程组数量
            // 每个线程组是 8x8（由 THREADS_X/Y 定义）
            FIntVector GroupCount = FIntVector(
                FMath::DivideAndRoundUp(Size.X, 8),
                FMath::DivideAndRoundUp(Size.Y, 8),
                1
            );

            // 添加 Compute Shader Pass
            FComputeShaderUtils::AddPass(
                GraphBuilder,
                RDG_EVENT_NAME("MyComputeShaderPass"),
                ComputeShader,
                Parameters,
                GroupCount
            );

            // 把临时纹理的结果拷贝回 RenderTarget
            FRDGTextureRef RTTexture = GraphBuilder.RegisterExternalTexture(
                CreateRenderTarget(RHITexture, TEXT("MyCS_RT"))
            );
            AddCopyTexturePass(GraphBuilder, RDGTexture, RTTexture);

            // 执行 RDG
            GraphBuilder.Execute();
        }
    );
}
```

### 10.3 Blueprint 接口

**`Public/MyCSBlueprintLibrary.h`：**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyCSBlueprintLibrary.generated.h"

UCLASS()
class MYCOMPUTESHADER_API UMyCSBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "ComputeShader")
    static void FillRenderTarget(
        UTextureRenderTarget2D* RenderTarget,
        FLinearColor Color
    );
};
```

**`Private/MyCSBlueprintLibrary.cpp`：**

```cpp
#include "MyCSBlueprintLibrary.h"
#include "MyCSDispatcher.h"

void UMyCSBlueprintLibrary::FillRenderTarget(
    UTextureRenderTarget2D* RenderTarget,
    FLinearColor Color)
{
    FMyCSDispatcher::Dispatch(RenderTarget, Color);
}
```

---

## 第十一步：在编辑器中测试

1. 编译项目
2. 在内容浏览器中右键 → **Miscellaneous → Render Target** → 创建 `RT_Test`
3. 设置尺寸为 **512×512**，格式为 **RTF RGBA8** 或 **RTF RGBA16f**
4. 创建一个 Actor Blueprint
5. 在 **BeginPlay** 中调用 **FillRenderTarget** 节点
6. 将 `RT_Test` 传入，设置想要的颜色
7. 创建一个材质，用 `TextureSampleParameter2D` 采样 `RT_Test`
8. 将材质应用到场景中的一个平面上
9. 运行，平面应该显示为你指定的颜色
<img width="1435" height="984" alt="image" src="https://github.com/user-attachments/assets/24a1d50d-cdba-4b13-b654-348133d4bb7d" />
<img width="1435" height="984" alt="image" src="https://github.com/user-attachments/assets/24a1d50d-cdba-4b13-b654-348133d4bb7d" />

---

## 常见错误与解决方案

### 编译阶段错误

| 错误 | 原因 | 解决 |
|------|------|------|
| `无法打开包括文件 "ShaderCore.h"` | 缺少模块依赖 | Build.cs 中添加 `"RenderCore"` |
| `LNK2019: unresolved external symbol`（大量，40个左右） | 缺少渲染模块依赖 | Build.cs 中添加 `"RenderCore"`、`"RHI"`、`"Renderer"` |
| `SET_SHADER_DEFINE` 未定义 | UE5 中此宏不存在 | 改用 `OutEnvironment.SetDefine(TEXT("NAME"), Value)` |
| `无法打开 "PostProcess/PostProcessInputs.h"` | 头文件在 UE5 中已移动 | 改用 `"PostProcess/DrawRectangle.h"` 或 `"ScreenPass.h"` |
| `StructuredBufffer` 编译错误 | 拼写错误（多了一个 f） | 改为 `StructuredBuffer` |
| `RWStructureBuffer` 编译错误 | 拼写错误（少了一个 d） | 改为 `RWStructuredBuffer` |

### 运行时错误

| 错误 | 原因 | 解决 |
|------|------|------|
| `Exception 0x80000003`（启动时） | `FindPlugin` 返回空指针 | 用 `FPaths::ProjectPluginsDir()` 拼接路径替代 |
| `Exception 0x80000003`（`AddShaderSourceDirectoryMapping`） | 磁盘路径不存在或虚拟路径格式错误 | 虚拟路径必须以 `/` 开头且不以 `/` 结尾；确认磁盘目录存在 |
| `CreateUAV` 处崩溃 | RenderTarget 没有 UAV 标志 | 用 `CreateTexture` 创建临时带 UAV 纹理，再 `AddCopyTexturePass` 拷回 |
| Shader 编译失败 | .usf 中变量名和 C++ 不匹配 | 确保 `SHADER_PARAMETER` 的名字和 HLSL 中完全一致 |
| 输出纹理全黑 | 线程组数量计算错误 | 确认 `DivideAndRoundUp` 参数正确，且 RenderTarget 格式兼容 |
| 编辑器启动时崩溃 | `LoadingPhase` 不是 `PostConfigInit` | 修改 `.uplugin` 文件中的 `LoadingPhase` |

### 着色器找不到

确认以下映射链条完整：

```
.uplugin: LoadingPhase = "PostConfigInit"
    ↓
StartupModule(): AddShaderSourceDirectoryMapping("/MyComputeShader", ".../Shaders")
                                                                          ↑
                                                         注意是 Shaders，不是 Shaders/Private
    ↓
IMPLEMENT_GLOBAL_SHADER(..., "/MyComputeShader/Private/MyComputeShader.usf", ...)
    ↓
磁盘文件: Plugins/MyComputeShader/Shaders/Private/MyComputeShader.usf
```

---

## 参数类型速查表

### C++ → HLSL 参数对应

| C++ 宏 | HLSL 类型 | 说明 |
|--------|-----------|------|
| `SHADER_PARAMETER(float, Name)` | `float Name;` | 标量 |
| `SHADER_PARAMETER(FVector4f, Name)` | `float4 Name;` | 四维向量 |
| `SHADER_PARAMETER(FIntPoint, Name)` | `int2 Name;` | 二维整数 |
| `SHADER_PARAMETER(FMatrix44f, Name)` | `float4x4 Name;` | 矩阵 |
| `SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Name)` | `RWTexture2D<float4> Name;` | 可读写纹理 |
| `SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, Name)` | `Texture2D<float4> Name;` | 只读纹理 |
| `SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<T>, Name)` | `StructuredBuffer<T> Name;` | 只读结构化缓冲 |
| `SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<T>, Name)` | `RWStructuredBuffer<T> Name;` | 可读写结构化缓冲 |
| `SHADER_PARAMETER_SAMPLER(SamplerState, Name)` | `SamplerState Name;` | 采样器 |

---

## 完整文件清单

| 文件路径 | 作用 |
|---------|------|
| `MyComputeShader.uplugin` | 插件描述，LoadingPhase 设为 PostConfigInit |
| `Source/.../MyComputeShader.Build.cs` | 模块依赖配置（RenderCore、RHI、Renderer） |
| `Source/.../Public/MyComputeShader.h` | 模块接口声明 |
| `Source/.../Private/MyComputeShader.cpp` | 模块启动，注册着色器目录映射 |
| `Source/.../Public/MyComputeShaderCS.h` | Shader 类和参数结构体声明 |
| `Source/.../Private/MyComputeShaderCS.cpp` | `IMPLEMENT_GLOBAL_SHADER` 绑定 |
| `Source/.../Public/MyCSDispatcher.h` | 调度器声明 |
| `Source/.../Private/MyCSDispatcher.cpp` | RDG 调度逻辑（核心） |
| `Source/.../Public/MyCSBlueprintLibrary.h` | Blueprint 函数库声明 |
| `Source/.../Private/MyCSBlueprintLibrary.cpp` | Blueprint 函数库实现 |
| `Shaders/Private/MyComputeShader.usf` | HLSL Compute Shader |

---

## 进阶：从这里开始做流体仿真

掌握了以上基础后，你可以把这个框架扩展为流体仿真：

1. **创建多张 Render Target**：分别存储速度场（PF_G16R16F）、密度场（PF_R16F）、压力场（PF_R16F）
2. **编写多个 Compute Shader**：Advection.usf、Diffusion.usf、Divergence.usf、Jacobi.usf、Project.usf
3. **每帧按顺序调度**：注入外力 → 对流 → 扩散 → 计算散度 → Jacobi 迭代（循环30次）→ 压力投影
4. **使用乒乓缓冲**：每一步读取 TextureA 写入 TextureB，然后交换
5. **加入涡度限制（Vorticity Confinement）**：减少数值耗散，让烟雾更有细节

---

## 推荐参考资源

| 资源 | 链接 |
|------|------|
| UE5.3 RDG Compute Shader Demo | https://github.com/brohaooo/ComputeShaderDemo |
| UE5 Niagara Compute Shader 集成 | https://github.com/Shadertech/UE5NiagaraComputeExample |
| Shadeup 官方教程 | https://unreal.shadeup.dev |
| Eyez_CG 博客教程 | https://eyezdomain.com/blog/161n/fast-and-clean-custom-compute-shader-and-raster-shader-in-ue5 |
| Epic 官方 Compute Shader 教程 | https://dev.epicgames.com/community/learning/tutorials/MP6x/unreal-engine-creating-a-compute-shader-c |
| Jos Stam 流体仿真论文 | http://graphics.cs.cmu.edu/nsp/course/15-464/Fall09/papers/StamFluidforGames.pdf |
| GPU Gems 第38章 GPU 流体 | https://developer.nvidia.com/gpugems/gpugems/part-vi-beyond-triangles/chapter-38-fast-fluid-dynamics-simulation-gpu |
