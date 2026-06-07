// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "SPCGExRampGraph.h"

#include "PCGExRampEditController.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace PCGExRampGraphColors
{
	static constexpr FLinearColor Background(0.0f, 0.0f, 0.0f, 0.25f);
	static constexpr FLinearColor Border(1.0f, 1.0f, 1.0f, 0.15f);
	static constexpr FLinearColor Grid(1.0f, 1.0f, 1.0f, 0.06f);
	static constexpr FLinearColor Curve(0.9f, 0.9f, 0.95f, 1.0f);
	static constexpr FLinearColor KeyNormal(0.8f, 0.8f, 0.85f, 1.0f);
	static constexpr FLinearColor KeySelected(1.0f, 0.6f, 0.1f, 1.0f);
}

void SPCGExRampGraph::Construct(const FArguments& InArgs, const TSharedRef<FPCGExRampEditController>& InController)
{
	Controller = InController;
	Controller->OnChanged.AddSP(this, &SPCGExRampGraph::HandleControllerChanged);
	Controller->OnSelectionChanged.AddSP(this, &SPCGExRampGraph::HandleSelectionChanged);
}

FVector2D SPCGExRampGraph::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(DesiredWidth, DesiredHeight);
}

void SPCGExRampGraph::HandleControllerChanged(bool bInteractive)
{
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SPCGExRampGraph::HandleSelectionChanged()
{
	Invalidate(EInvalidateWidgetReason::Paint);
}

#pragma region Coordinate mapping

float SPCGExRampGraph::TimeToLocalX(float Time, const FVector2D& Size) const
{
	const float Left = Padding;
	const float Right = Size.X - Padding;
	return Left + FMath::Clamp(Time, 0.0f, 1.0f) * FMath::Max(Right - Left, 1.0f);
}

float SPCGExRampGraph::ValueToLocalY(float Value, const FVector2D& Size) const
{
	const float Top = Padding;
	const float Bottom = Size.Y - Padding;
	const float Alpha = Controller->ValueToFrameAlpha(Value);
	return Bottom - Alpha * FMath::Max(Bottom - Top, 1.0f);
}

float SPCGExRampGraph::LocalXToTime(float LocalX, const FVector2D& Size) const
{
	const float Left = Padding;
	const float Right = Size.X - Padding;
	return FMath::Clamp((LocalX - Left) / FMath::Max(Right - Left, 1.0f), 0.0f, 1.0f);
}

float SPCGExRampGraph::LocalYToValue(float LocalY, const FVector2D& Size) const
{
	const float Top = Padding;
	const float Bottom = Size.Y - Padding;
	const float Alpha = (Bottom - LocalY) / FMath::Max(Bottom - Top, 1.0f);
	// Clamp to [0,1] of the (drag-frozen) frame: a vertical drag can't exceed the value range shown at
	// drag start, so dragging past the top/bottom edge can't run the value away. Use the numeric Value
	// field to set values beyond the visible range.
	return Controller->FrameAlphaToValue(FMath::Clamp(Alpha, 0.0f, 1.0f));
}

FVector2D SPCGExRampGraph::KeyToLocal(int32 Index, const FVector2D& Size) const
{
	return FVector2D(TimeToLocalX(Controller->GetKeyTimeAt(Index), Size), ValueToLocalY(Controller->GetKeyValueAt(Index), Size));
}

int32 SPCGExRampGraph::HitTestKeyIndex(const FVector2D& LocalPos, const FVector2D& Size) const
{
	int32 Best = INDEX_NONE;
	float BestDistSq = HandleHitRadius * HandleHitRadius;

	const int32 Num = Controller->NumKeys();
	for (int32 i = 0; i < Num; ++i)
	{
		const float DistSq = FVector2D::DistSquared(LocalPos, KeyToLocal(i, Size));
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = i;
		}
	}
	return Best;
}

#pragma endregion

#pragma region Painting

int32 SPCGExRampGraph::OnPaint(
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

	const float Left = Padding;
	const float Right = Size.X - Padding;
	const float Top = Padding;
	const float Bottom = Size.Y - Padding;

	// Background panel.
	FSlateDrawElement::MakeBox(
		OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(),
		WhiteBrush, ESlateDrawEffect::None, PCGExRampGraphColors::Background);

	// Grid lines (vertical at quarters, horizontal at frame min / mid / max).
	const int32 GridLayer = LayerId + 1;
	for (int32 i = 0; i <= 4; ++i)
	{
		const float X = TimeToLocalX(i * 0.25f, Size);
		TArray<FVector2D> Line;
		Line.Add(FVector2D(X, Top));
		Line.Add(FVector2D(X, Bottom));
		FSlateDrawElement::MakeLines(OutDrawElements, GridLayer, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, PCGExRampGraphColors::Grid, true, 1.0f);
	}
	for (int32 i = 0; i <= 2; ++i)
	{
		const float Y = Bottom - (i * 0.5f) * FMath::Max(Bottom - Top, 1.0f);
		TArray<FVector2D> Line;
		Line.Add(FVector2D(Left, Y));
		Line.Add(FVector2D(Right, Y));
		FSlateDrawElement::MakeLines(OutDrawElements, GridLayer, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, PCGExRampGraphColors::Grid, true, 1.0f);
	}

	// Border.
	{
		TArray<FVector2D> Box;
		Box.Add(FVector2D(Left, Top));
		Box.Add(FVector2D(Right, Top));
		Box.Add(FVector2D(Right, Bottom));
		Box.Add(FVector2D(Left, Bottom));
		Box.Add(FVector2D(Left, Top));
		FSlateDrawElement::MakeLines(OutDrawElements, GridLayer, AllottedGeometry.ToPaintGeometry(), Box, ESlateDrawEffect::None, PCGExRampGraphColors::Border, true, 1.0f);
	}

	// Evaluated curve.
	const FRichCurve& RichCurve = Controller->GetCurve();
	{
		TArray<FVector2D> CurvePoints;
		CurvePoints.Reserve(CurveSamples + 1);
		for (int32 i = 0; i <= CurveSamples; ++i)
		{
			const float T = static_cast<float>(i) / static_cast<float>(CurveSamples);
			const float V = RichCurve.Eval(T);
			CurvePoints.Add(FVector2D(TimeToLocalX(T, Size), ValueToLocalY(V, Size)));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), CurvePoints, ESlateDrawEffect::None, PCGExRampGraphColors::Curve, true, 1.5f);
	}

	// Key markers.
	const int32 KeyLayer = LayerId + 3;
	const int32 SelIdx = Controller->GetSelectedIndex();
	const int32 Num = Controller->NumKeys();
	for (int32 i = 0; i < Num; ++i)
	{
		const FVector2D Pos = KeyToLocal(i, Size);

		const bool bSelected = (i == SelIdx);

		const float MarkerSize = bSelected ? HandleSize + 2.0f : HandleSize;
		const FLinearColor MarkerColor = bSelected ? PCGExRampGraphColors::KeySelected : PCGExRampGraphColors::KeyNormal;

		FSlateDrawElement::MakeBox(
			OutDrawElements, KeyLayer,
			AllottedGeometry.ToPaintGeometry(FVector2D(MarkerSize, MarkerSize), FSlateLayoutTransform(FVector2D(Pos.X - MarkerSize * 0.5f, Pos.Y - MarkerSize * 0.5f))),
			WhiteBrush, ESlateDrawEffect::None, MarkerColor);
	}

	return KeyLayer + 1;
}

#pragma endregion

#pragma region Input

FReply SPCGExRampGraph::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!Controller.IsValid())
	{
		return FReply::Unhandled();
	}

	const FVector2D Size = MyGeometry.GetLocalSize();
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const int32 HitIdx = HitTestKeyIndex(Local, Size);

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
		if (HitIdx != INDEX_NONE)
		{
			Controller->SetSelectedKeyByIndex(HitIdx);
			bDragging = true;
			bDidDrag = false;
			DragIndex = HitIdx;
			return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
		}

		Controller->ClearSelection();
		return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// Non-destructive: select only.
		if (HitIdx != INDEX_NONE)
		{
			Controller->SetSelectedKeyByIndex(HitIdx);
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SPCGExRampGraph::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bDragging && HasMouseCapture() && Controller.IsValid())
	{
		const FVector2D Size = MyGeometry.GetLocalSize();
		const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		bDidDrag = true;
		Controller->MoveKeyByIndex(DragIndex, LocalXToTime(Local.X, Size), LocalYToValue(Local.Y, Size), /*bInteractive=*/true);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SPCGExRampGraph::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDragging)
	{
		bDragging = false;
		if (bDidDrag && Controller.IsValid())
		{
			Controller->CommitInteractive();
		}
		bDidDrag = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SPCGExRampGraph::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Controller.IsValid())
	{
		const FVector2D Size = MyGeometry.GetLocalSize();
		const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		if (HitTestKeyIndex(Local, Size) == INDEX_NONE)
		{
			Controller->AddKeyAtTime(LocalXToTime(Local.X, Size));
			return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
		}
	}
	return FReply::Unhandled();
}

FReply SPCGExRampGraph::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (Controller.IsValid() && (InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace))
	{
		Controller->DeleteSelectedKey();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

#pragma endregion
