// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Curves/RichCurve.h"
#include "Curves/KeyHandle.h"

/** Broadcast after the working curve mutates. bInteractive = mid-drag (coalesce undo, only grow the value frame). */
DECLARE_MULTICAST_DELEGATE_OneParam(FPCGExOnRampChanged, bool /*bInteractive*/);

/** Broadcast when the selected key changes (or selection is cleared). */
DECLARE_MULTICAST_DELEGATE(FPCGExOnRampSelectionChanged);

/**
 * Shared edit state for the inline ramp widgets (graph + key strip).
 *
 * Wraps the transient working FRichCurve and is the single place that enforces the ramp invariants:
 *   - at least two keys, the first and last of which are "endpoints"
 *   - endpoints cannot be deleted and cannot be dragged in X (their time is pinned)
 *   - interior keys never reorder (X drag clamps between neighbours)
 *
 * Position (time) is a normalized [0,1] domain. Value (Y) is never clamped -- the value axis
 * auto-frames to the key range instead (frame only grows during an interactive drag, then refits on
 * commit). The views talk to this in key *indices*; index<->handle bridging stays here (only the
 * curve's public API is used).
 */
class FPCGExRampEditController
{
public:
	explicit FPCGExRampEditController(const TSharedRef<FRichCurve>& InCurve);

	FRichCurve& GetCurve() const { return *Curve; }
	int32 NumKeys() const { return Curve->Keys.Num(); }

	// --- Read by index (for painting / hit-testing) ---
	float GetKeyTimeAt(int32 Index) const;
	float GetKeyValueAt(int32 Index) const;

	// --- Read by handle (for the inspector's current selection) ---
	float GetKeyTime(FKeyHandle Handle) const;
	float GetKeyValue(FKeyHandle Handle) const;
	ERichCurveInterpMode GetKeyInterp(FKeyHandle Handle) const;
	bool IsValidKey(FKeyHandle Handle) const;

	// --- Selection ---
	FKeyHandle GetSelectedKey() const { return SelectedKey; }
	int32 GetSelectedIndex() const { return GetKeyIndex(SelectedKey); }
	bool HasSelection() const { return SelectedKey.IsValid() && IsValidKey(SelectedKey); }
	void SetSelectedKeyByIndex(int32 Index);
	void ClearSelection();

	// --- Edits (all notify OnChanged; bInteractive coalesces undo and only grows the value frame) ---
	void AddKeyAtTime(float Time);
	void DeleteKeyByIndex(int32 Index);
	void DeleteSelectedKey();
	void MoveKeyByIndex(int32 Index, float NewTime, float NewValue, bool bInteractive);
	void MoveKey(FKeyHandle Handle, float NewTime, float NewValue, bool bInteractive);
	void SetKeyValue(FKeyHandle Handle, float NewValue, bool bInteractive);
	void SetKeyInterp(FKeyHandle Handle, ERichCurveInterpMode Mode);

	/** Close an interactive gesture: refit the value frame tightly and emit a final (undoable) change. */
	void CommitInteractive();

	// --- Value-axis framing ---
	float GetFrameMin() const { return FrameMin; }
	float GetFrameMax() const { return FrameMax; }
	float ValueToFrameAlpha(float Value) const;
	float FrameAlphaToValue(float Alpha) const;

	FPCGExOnRampChanged OnChanged;
	FPCGExOnRampSelectionChanged OnSelectionChanged;

	/** Smallest gap kept between neighbouring keys when dragging in X. */
	static constexpr float TimeEpsilon = 1.e-4f;

private:
	void NotifyChanged(bool bInteractive) { OnChanged.Broadcast(bInteractive); }

	/** index -> handle via the curve's public ordered handle iterator. Invalid handle if out of range. */
	FKeyHandle GetKeyHandle(int32 Index) const;
	/** handle -> index (INDEX_NONE if stale / invalid). */
	int32 GetKeyIndex(FKeyHandle Handle) const;

	void SetSelectedKey(FKeyHandle Handle);

	/** X-range a key may occupy: endpoints return [time,time]; interior keys are clamped between neighbours. */
	void GetTimeBounds(FKeyHandle Handle, float& OutMin, float& OutMax) const;

	/** Recompute the value frame to tightly fit the current keys. Called on non-interactive edits only;
	 *  the frame stays frozen during a drag so the Y mapping has a stable reference (no feedback loop). */
	void RefitValueFrame();

	TSharedRef<FRichCurve> Curve;

	FKeyHandle SelectedKey = FKeyHandle::Invalid();

	float FrameMin = 0.0f;
	float FrameMax = 1.0f;

	/** Fraction of the value span added as headroom above/below so points sit inside the border. */
	static constexpr float FramePadding = 0.08f;
};
