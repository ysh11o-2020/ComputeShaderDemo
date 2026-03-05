// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCSBlueprintLibrary.h"
#include "MyCSDispatcher.h"

void UMyCSBlueprintLibrary::FillRenderTarget(UTextureRenderTarget2D* RenderTarget, FLinearColor Color)
{
	FMyCSDispatcher::Dispatch(RenderTarget, Color);
}
