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

    // 获取 RenderTarget 的资源引用
    FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!RTResource) return;

    // 获取尺寸
    FIntPoint Size = FIntPoint(RenderTarget->SizeX, RenderTarget->SizeY);
    FVector4f FillColor = FVector4f(Color.R, Color.G, Color.B, Color.A);

    ENQUEUE_RENDER_COMMAND(MyComputeShaderCommand)(
        [RTResource, Size, FillColor](FRHICommandListImmediate& RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList);

            // 获取 RHI 纹理
            FRHITexture* RHITexture = RTResource->GetRenderTargetTexture();
            if (!RHITexture) return;

            // 用 FRDGTextureDesc 创建一个 RDG 纹理，然后拷贝回去
            FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
                Size,
                RHITexture->GetFormat(),
                FClearValueBinding::Black,
                TexCreate_ShaderResource | TexCreate_UAV
            );

            FRDGTextureRef RDGTexture = GraphBuilder.CreateTexture(Desc, TEXT("MyCS_Output"));
            FRDGTextureUAVRef TextureUAV = GraphBuilder.CreateUAV(RDGTexture);

            // 设置参数
            FMyCSParameters* Parameters = GraphBuilder.AllocParameters<FMyCSParameters>();
            Parameters->OutputTexture = TextureUAV;
            Parameters->FillColor = FillColor;
            Parameters->TextureSize = Size;

            TShaderMapRef<FMyComputeShaderCS> ComputeShader(
                GetGlobalShaderMap(GMaxRHIFeatureLevel)
            );

            FIntVector GroupCount = FIntVector(
                FMath::DivideAndRoundUp(Size.X, 8),
                FMath::DivideAndRoundUp(Size.Y, 8),
                1
            );

            FComputeShaderUtils::AddPass(
                GraphBuilder,
                RDG_EVENT_NAME("MyComputeShaderPass"),
                ComputeShader,
                Parameters,
                GroupCount
            );

            // 把 RDG 纹理的结果拷贝回 RenderTarget
            AddCopyTexturePass(GraphBuilder, RDGTexture, 
                GraphBuilder.RegisterExternalTexture(
                    CreateRenderTarget(RHITexture, TEXT("MyCS_RT"))
                )
            );

            GraphBuilder.Execute();
        }
    );
}
