// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "SPCGExRampKeyStrip.h"

#include "PCGExRampEditController.h"

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
	Controller = InController;
	Controller->OnChanged.AddSP(this, &SPCGExRampKeyStrip::HandleControllerChanged);
	Controller->OnSelectionChanged.AddSP(this, &SPCGExRampKeyStrip::HandleSelectionChanged);
}

FVector2D SPCGExRampKeyStrip::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(DesiredWidth, DesiredHeight);
}

void SPCGExRampKeyStrip::HandleControllerChanged(bool bInteractive)
{
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SPCGExRampKeyStrip::HandleSelectionChanged()
{
	Invalidate(EInvalidateWidgetReason::Paint);
}

#pragma region Coordinate mapping

float SPCGExRampKeyStrip::TimeToLocalX(float Time, const FVector2D& Size) const
{
	const float Left = Padding;
	const float Right = Size.X - Padding;
	// Map through the shared time frame (no clamp) so gems line up under the graph's points.
	const float Alpha = Controller->TimeToFrameAlpha(Time);
	return Left + Alpha * FMath::Max(Right - Left, 1.0f);
}

float SPCGExRampKeyStrip::LocalXToTime(float LocalX, const FVector2D& Size) const
{
	const float Left = Padding;
	const float Right = Size.X - Padding;
	const float Alpha = (LocalX - Left) / FMath::Max(Right - Left, 1.0f);
	return Controller->FrameAlphaToTime(Alpha);
}

int32 SPCGExRampKeyStrip::HitTestKeyIndex(float LocalX, const FVector2D& Size) const
{
	int32 Best = INDEX_NONE;
	float BestDist = GemHitHalfWidth;

	const int32 Num = Controller->NumKeys();
	for (int32 i = 0; i < Num; ++i)
	{
		const float Dist = FMath::Abs(LocalX - TimeToLocalX(Controller->GetKeyTimeAt(i), Size));
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

	// Gems.
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
			WhiteBrush, ESlateDrawEffect::None, GemColor);
	}

	return GemLayer + 1;
}

#pragma endregion

#pragma region Input

FReply SPCGExRampKeyStrip::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!Controller.IsValid())
	{
		return FReply::Unhandled();
	}

	const FVector2D Size = MyGeometry.GetLocalSize();
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const int32 HitIdx = HitTestKeyIndex(Local.X, Size);

	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		if (HitIdx != INDEX_NONE)
		{
			Controller->DeleteKeyByIndex(HitIdx);
		}
		return FReply::Handled();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Alt + click a gem: delete it (alternative to middle-click).
		if (MouseEvent.IsAltDown())
		{
			if (HitIdx != INDEX_NONE)
			{
				Controller->DeleteKeyByIndex(HitIdx);
			}
			return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
		}

		// Shift + click: add a key at the cursor time (value sampled from the curve -- the strip has no Y).
		if (MouseEvent.IsShiftDown())
		{
			Controller->AddKeyAtTime(LocalXToTime(Local.X, Size));
			return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
		}

		if (HitIdx != INDEX_NONE)
		{
			Controller->SetSelectedKeyByIndex(HitIdx);
			bDragging = true;
			bDidDrag = false;
			DragHandle = Controller->GetKeyHandleAt(HitIdx);
			return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
		}

		Controller->ClearSelection();
		return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (HitIdx != INDEX_NONE)
		{
			Controller->SetSelectedKeyByIndex(HitIdx);
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SPCGExRampKeyStrip::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bDragging && HasMouseCapture() && Controller.IsValid())
	{
		const FVector2D Size = MyGeometry.GetLocalSize();
		const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		bDidDrag = true;
		// X only: keep the current value.
		Controller->MoveKey(DragHandle, LocalXToTime(Local.X, Size), Controller->GetKeyValue(DragHandle), /*bInteractive=*/true);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SPCGExRampKeyStrip::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDragging)
	{
		bDragging = false;
		DragHandle = FKeyHandle::Invalid();
		if (bDidDrag && Controller.IsValid())
		{
			Controller->CommitInteractive();
		}
		bDidDrag = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SPCGExRampKeyStrip::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Controller.IsValid())
	{
		const FVector2D Size = MyGeometry.GetLocalSize();
		const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		if (HitTestKeyIndex(Local.X, Size) == INDEX_NONE)
		{
			Controller->AddKeyAtTime(LocalXToTime(Local.X, Size));
			return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
		}
	}
	return FReply::Unhandled();
}

FReply SPCGExRampKeyStrip::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (Controller.IsValid() && (InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace))
	{
		Controller->DeleteSelectedKey();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

#pragma endregion
