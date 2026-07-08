// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "SPCGExRampKeyStrip.h"

#include "PCGExRampEditController.h"
#include "PCGExRampsEditorStyle.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace PCGExRampStripColors
{
	static constexpr FLinearColor Background(0.0f, 0.0f, 0.0f, 0.2f);
	static constexpr FLinearColor Track(1.0f, 1.0f, 1.0f, 0.1f);
	static constexpr FLinearColor GemNormal(0.8f, 0.8f, 0.85f, 1.0f);
	static constexpr FLinearColor GemSelected(1.0f, 0.6f, 0.1f, 1.0f);
}

void SPCGExRampKeyStrip::Construct(const FArguments& InArgs, const TSharedRef<FPCGExRampEditController>& InController)
{
	Init(InController);
}

FVector2D SPCGExRampKeyStrip::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(DesiredWidth, DesiredHeight);
}

#pragma region Hit-test

int32 SPCGExRampKeyStrip::HitTestKey(const FVector2D& Local, const FVector2D& Size) const
{
	int32 Best = INDEX_NONE;
	float BestDist = GemHitHalfWidth;

	const int32 Num = Controller->NumKeys();
	for (int32 i = 0; i < Num; ++i)
	{
		const float Dist = FMath::Abs(Local.X - TimeToLocalX(Controller->GetKeyTimeAt(i), Size));
		if (Dist <= BestDist)
		{
			BestDist = Dist;
			Best = i;
		}
	}
	return Best;
}

#pragma endregion

#pragma region Painting

int32 SPCGExRampKeyStrip::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	if (!Controller.IsValid())
	{
		return LayerId;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetDefaultBrush();

	// Background.
	FSlateDrawElement::MakeBox(
		OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(),
		WhiteBrush, ESlateDrawEffect::None, PCGExRampStripColors::Background);

	// Centre track line.
	{
		const float Y = Size.Y * 0.5f;
		TArray<FVector2D> Line;
		Line.Add(FVector2D(Padding, Y));
		Line.Add(FVector2D(Size.X - Padding, Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, PCGExRampStripColors::Track, true, 1.0f);
	}

	// Gems. White SVG when supplied (tinted per selection below), else the built-in rectangle.
	const FSlateBrush* SliderBrush = FPCGExRampsEditorStyle::GetSliderBrush();
	const FSlateBrush* GemDrawBrush = SliderBrush ? SliderBrush : WhiteBrush;
	const int32 GemLayer = LayerId + 2;
	const float GemHeight = FMath::Max(Size.Y - 6.0f, 4.0f);
	const float GemTop = (Size.Y - GemHeight) * 0.5f;

	const int32 SelIdx = Controller->GetSelectedIndex();
	const int32 Num = Controller->NumKeys();
	for (int32 i = 0; i < Num; ++i)
	{
		const float CenterX = TimeToLocalX(Controller->GetKeyTimeAt(i), Size);

		const bool bSelected = (i == SelIdx);

		const FLinearColor GemColor = bSelected ? PCGExRampStripColors::GemSelected : PCGExRampStripColors::GemNormal;

		FSlateDrawElement::MakeBox(
			OutDrawElements, GemLayer,
			AllottedGeometry.ToPaintGeometry(FVector2D(GemWidth, GemHeight), FSlateLayoutTransform(FVector2D(CenterX - GemWidth * 0.5f, GemTop))),
			GemDrawBrush, ESlateDrawEffect::None, GemColor);
	}

	return GemLayer + 1;
}

#pragma endregion

#pragma region Drag / add hooks

void SPCGExRampKeyStrip::ApplyDrag(const FVector2D& Local, const FVector2D& Size, bool /*bCtrlDown*/)
{
	// X only: keep the current value (there is no vertical axis and no Ctrl axis-lock on the strip).
	Controller->MoveKey(DragHandle, LocalXToTime(Local.X, Size), Controller->GetKeyValue(DragHandle), /*bInteractive=*/true);
}

void SPCGExRampKeyStrip::AddKeyAtCursor(const FVector2D& Local, const FVector2D& Size)
{
	// The strip has no Y, so Shift+click adds at the cursor time with the value sampled from the curve.
	Controller->AddKeyAtTime(LocalXToTime(Local.X, Size));
}

#pragma endregion
