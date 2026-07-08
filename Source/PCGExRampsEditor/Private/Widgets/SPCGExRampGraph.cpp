// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "SPCGExRampGraph.h"

#include "PCGExRampEditController.h"
#include "PCGExRampsEditorStyle.h"

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
	// Signed-area fill: the band between the curve and the value=0 baseline, so positive vs negative
	// regions read at a glance.
	static constexpr FLinearColor Fill(0.9f, 0.9f, 0.95f, 0.07f);
	// Vertical drop-lines linking a graph key to its strip gem: faint for unselected, the selection
	// colour (solid) for the selected key.
	static constexpr FLinearColor ConnectorNormal(1.0f, 1.0f, 1.0f, 0.12f);
	static constexpr FLinearColor ConnectorSelected(1.0f, 0.6f, 0.1f, 0.6f);
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
	// Map through the time frame (no clamp): a key dragged past the frozen frame's edge tracks the
	// cursor beyond the plot and pulls back into view when the frame refits on release.
	const float Alpha = Controller->TimeToFrameAlpha(Time);
	return Left + Alpha * FMath::Max(Right - Left, 1.0f);
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
	const float Alpha = (LocalX - Left) / FMath::Max(Right - Left, 1.0f);
	return Controller->FrameAlphaToTime(Alpha);
}

float SPCGExRampGraph::LocalYToValue(float LocalY, const FVector2D& Size) const
{
	const float Top = Padding;
	const float Bottom = Size.Y - Padding;
	const float Alpha = (Bottom - LocalY) / FMath::Max(Bottom - Top, 1.0f);
	// No clamp: the value follows the cursor unbounded. The frame is frozen mid-drag (stable mapping,
	// no feedback loop), so the point can be dragged past the top/bottom edge; the frame refits with
	// headroom on release. This is what lets the extremes (usually the end keys) leave the border.
	return Controller->FrameAlphaToValue(Alpha);
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

	// Paint order, back to front: background, grid/border, signed-area fill, key->gem connectors, curve,
	// key markers.
	const int32 GridLayer = LayerId + 1;
	const int32 FillLayer = LayerId + 2;
	const int32 ConnectorLayer = LayerId + 3;
	const int32 CurveLayer = LayerId + 4;
	const int32 KeyLayer = LayerId + 5;

	// Grid lines (vertical at quarters, horizontal at frame min / mid / max).
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

	// Zero baseline in local space, clamped to the plot so the fill still has a valid edge when value=0
	// falls outside the framed range.
	const float ZeroY = FMath::Clamp(ValueToLocalY(0.0f, Size), Top, Bottom);

	// Evaluated curve, sampled across the whole visible frame (which may be wider than [0,1]) so the
	// extrapolated tails and any out-of-range keys are drawn edge to edge.
	const FRichCurve& RichCurve = Controller->GetCurve();
	{
		const float FrameMinT = Controller->GetTimeFrameMin();
		const float FrameMaxT = Controller->GetTimeFrameMax();
		const float SpanT = FrameMaxT - FrameMinT;

		TArray<FVector2D> CurvePoints;
		CurvePoints.Reserve(CurveSamples + 1);
		for (int32 i = 0; i <= CurveSamples; ++i)
		{
			const float T = FrameMinT + SpanT * (static_cast<float>(i) / static_cast<float>(CurveSamples));
			const float V = RichCurve.Eval(T);
			CurvePoints.Add(FVector2D(TimeToLocalX(T, Size), ValueToLocalY(V, Size)));
		}

		// Signed-area fill: one thin quad per sample segment, spanning between the curve and the zero
		// baseline. Local-space boxes reuse the proven white-brush + tint path; a single MakeCustomVerts
		// mesh would be the smoother-but-fiddlier upgrade if this ever needs it.
		for (int32 i = 0; i < CurvePoints.Num() - 1; ++i)
		{
			const float X0 = CurvePoints[i].X;
			const float BoxW = CurvePoints[i + 1].X - X0;
			const float CurveY = (CurvePoints[i].Y + CurvePoints[i + 1].Y) * 0.5f;
			const float BoxTop = FMath::Min(CurveY, ZeroY);
			const float BoxH = FMath::Abs(CurveY - ZeroY);
			if (BoxW > 0.0f && BoxH > 0.0f)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements, FillLayer,
					AllottedGeometry.ToPaintGeometry(FVector2D(BoxW, BoxH), FSlateLayoutTransform(FVector2D(X0, BoxTop))),
					WhiteBrush, ESlateDrawEffect::None, PCGExRampGraphColors::Fill);
			}
		}

		FSlateDrawElement::MakeLines(OutDrawElements, CurveLayer, AllottedGeometry.ToPaintGeometry(), CurvePoints, ESlateDrawEffect::None, PCGExRampGraphColors::Curve, true, 1.5f);
	}

	// Key markers, each with a vertical drop-line down to its strip gem (dashed + faint when unselected,
	// solid in the selection colour for the selected key).
	// White SVG when supplied (tinted by the marker colour below), else the built-in square.
	const FSlateBrush* KeyBrush = FPCGExRampsEditorStyle::GetKeyBrush();
	const FSlateBrush* KeyMarkerBrush = KeyBrush ? KeyBrush : WhiteBrush;
	const int32 SelIdx = Controller->GetSelectedIndex();
	const int32 Num = Controller->NumKeys();
	for (int32 i = 0; i < Num; ++i)
	{
		const FVector2D Pos = KeyToLocal(i, Size);
		const bool bSelected = (i == SelIdx);

		// Connector: key point -> plot bottom edge (the strip gem sits directly beneath, same X).
		if (bSelected)
		{
			TArray<FVector2D> Line;
			Line.Add(FVector2D(Pos.X, Pos.Y));
			Line.Add(FVector2D(Pos.X, Bottom));
			FSlateDrawElement::MakeLines(OutDrawElements, ConnectorLayer, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, PCGExRampGraphColors::ConnectorSelected, true, 1.0f);
		}
		else
		{
			constexpr float Dash = 3.0f;
			constexpr float DashGap = 2.5f;
			for (float Y = Pos.Y; Y < Bottom; Y += Dash + DashGap)
			{
				TArray<FVector2D> Seg;
				Seg.Add(FVector2D(Pos.X, Y));
				Seg.Add(FVector2D(Pos.X, FMath::Min(Y + Dash, Bottom)));
				FSlateDrawElement::MakeLines(OutDrawElements, ConnectorLayer, AllottedGeometry.ToPaintGeometry(), Seg, ESlateDrawEffect::None, PCGExRampGraphColors::ConnectorNormal, true, 1.0f);
			}
		}

		const float MarkerSize = bSelected ? HandleSize + 2.0f : HandleSize;
		const FLinearColor MarkerColor = bSelected ? PCGExRampGraphColors::KeySelected : PCGExRampGraphColors::KeyNormal;

		FSlateDrawElement::MakeBox(
			OutDrawElements, KeyLayer,
			AllottedGeometry.ToPaintGeometry(FVector2D(MarkerSize, MarkerSize), FSlateLayoutTransform(FVector2D(Pos.X - MarkerSize * 0.5f, Pos.Y - MarkerSize * 0.5f))),
			KeyMarkerBrush, ESlateDrawEffect::None, MarkerColor);
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
		// Alt + click a point: delete it (alternative to middle-click).
		if (MouseEvent.IsAltDown())
		{
			if (HitIdx != INDEX_NONE)
			{
				Controller->DeleteKeyByIndex(HitIdx);
			}
			return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
		}

		// Shift + click: add a key at the cursor (both X and value), then select it.
		if (MouseEvent.IsShiftDown())
		{
			Controller->AddKeyAtTimeValue(LocalXToTime(Local.X, Size), LocalYToValue(Local.Y, Size));
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
		// Ctrl locks the horizontal axis: keep the key's current time and edit value only. Held live, so
		// releasing Ctrl mid-drag resumes free 2D movement.
		const float NewTime = MouseEvent.IsControlDown()
			                      ? Controller->GetKeyTime(DragHandle)
			                      : LocalXToTime(Local.X, Size);
		Controller->MoveKey(DragHandle, NewTime, LocalYToValue(Local.Y, Size), /*bInteractive=*/true);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SPCGExRampGraph::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
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
