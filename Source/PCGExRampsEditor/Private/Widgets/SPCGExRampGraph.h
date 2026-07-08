// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "SPCGExRampWidgetBase.h"

class FPCGExRampEditController;

/**
 * Ramp graph: paints the evaluated curve and one draggable point per key. The X/Y axes auto-frame to
 * the controller's time/value frames (X always spans at least [0,1]); this widget only maps geometry.
 * The gesture grammar (select/drag/add/delete, Ctrl axis-lock, capture-loss handling) lives in the
 * shared SPCGExRampWidgetBase; this class supplies painting, the 2D hit-test, and the 2D drag/add.
 */
class SPCGExRampGraph : public SPCGExRampWidgetBase
{
public:
	SLATE_BEGIN_ARGS(SPCGExRampGraph) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FPCGExRampEditController>& InController);

	//~ Begin SWidget interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	//~ End SWidget interface

protected:
	//~ Begin SPCGExRampWidgetBase interface
	virtual int32 HitTestKey(const FVector2D& Local, const FVector2D& Size) const override;
	virtual void ApplyDrag(const FVector2D& Local, const FVector2D& Size, bool bCtrlDown) override;
	virtual void AddKeyAtCursor(const FVector2D& Local, const FVector2D& Size) override;
	//~ End SPCGExRampWidgetBase interface

private:
	float ValueToLocalY(float Value, const FVector2D& Size) const;
	float LocalYToValue(float LocalY, const FVector2D& Size) const;
	FVector2D KeyToLocal(int32 Index, const FVector2D& Size) const;

	static constexpr float HandleSize = 9.0f;
	static constexpr float HandleHitRadius = 11.0f;
	static constexpr float DesiredWidth = 320.0f;
	static constexpr float DesiredHeight = 110.0f;
	static constexpr int32 CurveSamples = 128;
};
