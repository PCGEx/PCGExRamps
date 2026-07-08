// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExRampsEditorStyle.h"

#include "Brushes/SlateImageBrush.h" // FSlateVectorImageBrush
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyleRegistry.h"

namespace PCGExRampsEditorStyleNames
{
	// Single source of truth for the brush ids, shared by registration and lookup. Function-local statics
	// (not namespace-scope FName globals) so there's no static-init-order dependency on FName's tables.
	inline FName Key() { static const FName Name(TEXT("PCGExRamps.Key")); return Name; }
	inline FName Slider() { static const FName Name(TEXT("PCGExRamps.Slider")); return Name; }
}

FName FPCGExRampsEditorStyle::StaticStyleSetName()
{
	static const FName StyleName(TEXT("PCGExRampsEditorStyle"));
	return StyleName;
}

FPCGExRampsEditorStyle::FPCGExRampsEditorStyle()
	: FSlateStyleSet(StaticStyleSetName())
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PCGExRamps"));
	if (Plugin.IsValid())
	{
		SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));

		// Nominal size only -- the widgets always draw the brush at an explicit size, so this is just the
		// vector's default. The art must be WHITE: colour comes from the draw-time tint (selection state).
		const FVector2D IconSize(16.0f, 16.0f);

		// Register a brush only when its .svg exists, so a missing file leaves the getter returning nullptr
		// and the widget keeps drawing its built-in shape instead of a broken/placeholder brush.
		const FString KeyPath = RootToContentDir(TEXT("RampKey"), TEXT(".svg"));
		if (FPaths::FileExists(KeyPath))
		{
			Set(PCGExRampsEditorStyleNames::Key(), new FSlateVectorImageBrush(KeyPath, IconSize));
		}

		const FString SliderPath = RootToContentDir(TEXT("RampSlider"), TEXT(".svg"));
		if (FPaths::FileExists(SliderPath))
		{
			Set(PCGExRampsEditorStyleNames::Slider(), new FSlateVectorImageBrush(SliderPath, IconSize));
		}
	}

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FPCGExRampsEditorStyle::~FPCGExRampsEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

const FSlateBrush* FPCGExRampsEditorStyle::FindBrush(const FName BrushName)
{
	const ISlateStyle* Style = FSlateStyleRegistry::FindSlateStyle(StaticStyleSetName());
	// Pass an explicit null default so a missing brush yields nullptr (not the "no brush" placeholder),
	// which is the "art not supplied yet -> draw a fallback" signal the widgets rely on.
	return Style ? Style->GetOptionalBrush(BrushName, nullptr, nullptr) : nullptr;
}

const FSlateBrush* FPCGExRampsEditorStyle::GetKeyBrush()
{
	return FindBrush(PCGExRampsEditorStyleNames::Key());
}

const FSlateBrush* FPCGExRampsEditorStyle::GetSliderBrush()
{
	return FindBrush(PCGExRampsEditorStyleNames::Slider());
}
