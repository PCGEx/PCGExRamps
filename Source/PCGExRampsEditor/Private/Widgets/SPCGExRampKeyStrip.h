// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Curves/KeyHandle.h"
#include "Widgets/SLeafWidget.h"

class FPCGExRampEditController;

/**
 * Houdini-style key strip: a horizontal bar of draggable gems, one per key, positioned by their time
 * within the shared X frame. Shares the graph's horizontal padding so a gem sits directly under its
 * point.
 *
 * Interaction (X only -- value is edited on the graph or in the inspector):
 *   - left-click a gem     : select (and begin an X drag)
 *   - drag a gem           : move it in X; dragging past a neighbour reorders keys
 *   - Shift + click        : add a key at the cursor time (value sampled from the curve)
 *   - double-click empty   : add a key there
 *   - Alt + click a gem    : delete it (down to a one-key minimum)
 *   - middle-click a gem   : delete it (down to a one-key minimum)
 *   - Del / Backspace      : delete the selected key
 *   - right-click a gem    : select only (non-destructive)
 */
class SPCGExRampKeyStrip : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGExRampKeyStrip) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FPCGExRampEditController>& InController);

	//~ Begin SWidget interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget interface

private:
	void HandleControllerChanged(bool bInteractive);
	void HandleSelectionChanged();

	float TimeToLocalX(float Time, const FVector2D& Size) const;
	float LocalXToTime(float LocalX, const FVector2D& Size) const;

	/** Index of the gem whose centre is within the hit width of LocalX, or INDEX_NONE. */
	int32 HitTestKeyIndex(float LocalX, const FVector2D& Size) const;

	TSharedPtr<FPCGExRampEditController> Controller;

	bool bDragging = false;
	bool bDidDrag = false;
	/** Key being dragged, tracked by handle so a reorder mid-drag can't point us at the wrong key. */
	FKeyHandle DragHandle = FKeyHandle::Invalid();

	static constexpr float Padding = 8.0f;
	static constexpr float GemWidth = 10.0f;
	static constexpr float GemHitHalfWidth = 7.0f;
	static constexpr float DesiredWidth = 320.0f;
	static constexpr float DesiredHeight = 18.0f;
};
