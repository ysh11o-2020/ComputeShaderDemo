// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyCSBlueprintLibrary.generated.h"

/**
 * 
 */
UCLASS()
class COMPUTESHADERDEMO_API UMyCSBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void FillRenderTarget(
		UTextureRenderTarget2D* RenderTarget,
		FLinearColor Color
	);
};
