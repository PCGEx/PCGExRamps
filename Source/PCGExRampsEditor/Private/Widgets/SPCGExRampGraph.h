// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Curves/KeyHandle.h"
#include "Widgets/SLeafWidget.h"

class FPCGExRampEditController;

/**
 * Ramp graph: paints the evaluated curve and one draggable point per key. The X/Y axes auto-frame to
 * the controller's time/value frames (X always spans at least [0,1]); this widget only maps geometry.
 *
 * Interaction:
 *   - left-click a point   : select (and begin a 2D drag)
 *   - drag a point         : move it freely in X and value; dragging past a neighbour reorders keys
 *   - Ctrl + drag a point  : lock X (value-only drag)
 *   - Shift + click        : add a key at the cursor (X and value)
 *   - double-click empty   : add a key at the evaluated value there
 *   - Alt + click a point  : delete it (down to a one-key minimum)
 *   - middle-click a point : delete it (down to a one-key minimum)
 *   - Del / Backspace      : delete the selected key
 *   - right-click a point  : select only (non-destructive)
 *
 * All mutation goes through the shared FPCGExRampEditController.
 */
class SPCGExRampGraph : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGExRampGraph) {}
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
	float ValueToLocalY(float Value, const FVector2D& Size) const;
	float LocalXToTime(float LocalX, const FVector2D& Size) const;
	float LocalYToValue(float LocalY, const FVector2D& Size) const;
	FVector2D KeyToLocal(int32 Index, const FVector2D& Size) const;

	/** Index of the closest key within the hit radius of LocalPos, or INDEX_NONE. */
	int32 HitTestKeyIndex(const FVector2D& LocalPos, const FVector2D& Size) const;

	TSharedPtr<FPCGExRampEditController> Controller;

	bool bDragging = false;
	bool bDidDrag = false;
	/** Key being dragged, tracked by handle so a reorder mid-drag can't point us at the wrong key. */
	FKeyHandle DragHandle = FKeyHandle::Invalid();

	static constexpr float Padding = 8.0f;
	static constexpr float HandleSize = 9.0f;
	static constexpr float HandleHitRadius = 11.0f;
	static constexpr float DesiredWidth = 320.0f;
	static constexpr float DesiredHeight = 110.0f;
	static constexpr int32 CurveSamples = 128;
};
