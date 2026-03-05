#pragma once

#include "CoreMinimal.h"

class UTextureRenderTarget2D;

class MYCOMPUTESHADER_API FMyCSDispatcher
{
public:
	// 将指定颜色填充到 RenderTarget
	static void Dispatch(
		UTextureRenderTarget2D* RenderTarget,
		FLinearColor Color
	);
};