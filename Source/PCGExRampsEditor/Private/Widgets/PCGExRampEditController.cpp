// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExRampEditController.h"

FPCGExRampEditController::FPCGExRampEditController(const TSharedRef<FRichCurve>& InCurve)
	: Curve(InCurve)
{
	RefitValueFrame();
}

#pragma region Index <-> handle

FKeyHandle FPCGExRampEditController::GetKeyHandle(int32 Index) const
{
	if (!Curve->Keys.IsValidIndex(Index))
	{
		return FKeyHandle::Invalid();
	}

	// GetKeyHandleIterator yields handles in key (index) order.
	int32 i = 0;
	for (auto It = Curve->GetKeyHandleIterator(); It; ++It, ++i)
	{
		if (i == Index)
		{
			return *It;
		}
	}
	return FKeyHandle::Invalid();
}

int32 FPCGExRampEditController::GetKeyIndex(FKeyHandle Handle) const
{
	return Curve->IsKeyHandleValid(Handle) ? Curve->GetIndexSafe(Handle) : INDEX_NONE;
}

#pragma endregion

#pragma region Reads

float FPCGExRampEditController::GetKeyTimeAt(int32 Index) const
{
	return Curve->Keys.IsValidIndex(Index) ? Curve->Keys[Index].Time : 0.0f;
}

float FPCGExRampEditController::GetKeyValueAt(int32 Index) const
{
	return Curve->Keys.IsValidIndex(Index) ? Curve->Keys[Index].Value : 0.0f;
}

float FPCGExRampEditController::GetKeyTime(FKeyHandle Handle) const
{
	return Curve->IsKeyHandleValid(Handle) ? Curve->GetKeyTime(Handle) : 0.0f;
}

float FPCGExRampEditController::GetKeyValue(FKeyHandle Handle) const
{
	return Curve->IsKeyHandleValid(Handle) ? Curve->GetKeyValue(Handle) : 0.0f;
}

ERichCurveInterpMode FPCGExRampEditController::GetKeyInterp(FKeyHandle Handle) const
{
	return Curve->IsKeyHandleValid(Handle) ? Curve->GetKey(Handle).InterpMode.GetValue() : RCIM_Linear;
}

bool FPCGExRampEditController::IsValidKey(FKeyHandle Handle) const
{
	return Curve->IsKeyHandleValid(Handle);
}

void FPCGExRampEditController::GetTimeBounds(FKeyHandle Handle, float& OutMin, float& OutMax) const
{
	const int32 Idx = GetKeyIndex(Handle);
	const int32 Num = NumKeys();

	if (Idx == INDEX_NONE)
	{
		OutMin = 0.0f;
		OutMax = 1.0f;
		return;
	}

	// Every key (including the first/last) is editable in X within the [0,1] domain. Clamping between
	// neighbours keeps the keys ordered, so the first/last key always remains the lowest/highest
	// position -- the endpoints are dynamic, not pinned.
	OutMin = (Idx > 0) ? Curve->Keys[Idx - 1].Time + TimeEpsilon : 0.0f;
	OutMax = (Idx < Num - 1) ? Curve->Keys[Idx + 1].Time - TimeEpsilon : 1.0f;
	if (OutMin > OutMax)
	{
		OutMin = OutMax = Curve->Keys[Idx].Time;
	}
}

#pragma endregion

#pragma region Selection

void FPCGExRampEditController::SetSelectedKey(FKeyHandle Handle)
{
	if (!IsValidKey(Handle) || SelectedKey == Handle)
	{
		return;
	}
	SelectedKey = Handle;
	OnSelectionChanged.Broadcast();
}

void FPCGExRampEditController::SetSelectedKeyByIndex(int32 Index)
{
	SetSelectedKey(GetKeyHandle(Index));
}

void FPCGExRampEditController::ClearSelection()
{
	if (!SelectedKey.IsValid())
	{
		return;
	}
	SelectedKey = FKeyHandle::Invalid();
	OnSelectionChanged.Broadcast();
}

#pragma endregion

#pragma region Edits

void FPCGExRampEditController::AddKeyAtTime(float Time)
{
	// Clamp inside the domain so a new key never lands coincident with a pinned endpoint.
	const float T = FMath::Clamp(Time, TimeEpsilon, 1.0f - TimeEpsilon);
	const float V = Curve->Eval(T);

	const FKeyHandle Handle = Curve->AddKey(T, V);

	// Inherit interpolation from the left neighbour (fallback to the right, else linear).
	const int32 Idx = GetKeyIndex(Handle);
	ERichCurveInterpMode Interp = RCIM_Linear;
	if (Idx != INDEX_NONE)
	{
		if (Idx > 0)
		{
			Interp = Curve->Keys[Idx - 1].InterpMode.GetValue();
		}
		else if (Idx + 1 < NumKeys())
		{
			Interp = Curve->Keys[Idx + 1].InterpMode.GetValue();
		}
	}
	Curve->GetKey(Handle).InterpMode = Interp;
	Curve->AutoSetTangents();

	SelectedKey = Handle;
	OnSelectionChanged.Broadcast();

	RefitValueFrame();
	NotifyChanged(/*bInteractive=*/false);
}

void FPCGExRampEditController::DeleteKeyByIndex(int32 Index)
{
	// Always keep at least two keys, and never delete an endpoint.
	if (NumKeys() <= 2 || Index <= 0 || Index >= NumKeys() - 1)
	{
		return;
	}

	const FKeyHandle Handle = GetKeyHandle(Index);
	if (!IsValidKey(Handle))
	{
		return;
	}

	const bool bWasSelected = SelectedKey.IsValid() && (SelectedKey == Handle);

	Curve->DeleteKey(Handle);
	Curve->AutoSetTangents();

	if (bWasSelected)
	{
		// Select the neighbour that now occupies (or sits just before) the freed index.
		const int32 NewSel = FMath::Clamp(Index, 0, NumKeys() - 1);
		SelectedKey = GetKeyHandle(NewSel);
		OnSelectionChanged.Broadcast();
	}

	RefitValueFrame();
	NotifyChanged(/*bInteractive=*/false);
}

void FPCGExRampEditController::DeleteSelectedKey()
{
	if (HasSelection())
	{
		DeleteKeyByIndex(GetSelectedIndex());
	}
}

void FPCGExRampEditController::MoveKeyByIndex(int32 Index, float NewTime, float NewValue, bool bInteractive)
{
	MoveKey(GetKeyHandle(Index), NewTime, NewValue, bInteractive);
}

void FPCGExRampEditController::MoveKey(FKeyHandle Handle, float NewTime, float NewValue, bool bInteractive)
{
	if (!IsValidKey(Handle))
	{
		return;
	}

	float MinT, MaxT;
	GetTimeBounds(Handle, MinT, MaxT);
	const float ClampedT = FMath::Clamp(NewTime, MinT, MaxT);

	Curve->SetKeyTime(Handle, ClampedT);
	Curve->SetKeyValue(Handle, NewValue);
	Curve->AutoSetTangents();

	// Frame is frozen during an interactive drag (only refit on commit) -- this is what stops the
	// value/frame feedback loop that otherwise blows values up to astronomical numbers.
	if (!bInteractive)
	{
		RefitValueFrame();
	}
	NotifyChanged(bInteractive);
}

void FPCGExRampEditController::SetKeyValue(FKeyHandle Handle, float NewValue, bool bInteractive)
{
	if (!IsValidKey(Handle))
	{
		return;
	}
	Curve->SetKeyValue(Handle, NewValue);
	Curve->AutoSetTangents();

	// Frame is frozen during an interactive drag (only refit on commit) -- this is what stops the
	// value/frame feedback loop that otherwise blows values up to astronomical numbers.
	if (!bInteractive)
	{
		RefitValueFrame();
	}
	NotifyChanged(bInteractive);
}

void FPCGExRampEditController::SetKeyInterp(FKeyHandle Handle, ERichCurveInterpMode Mode)
{
	if (!IsValidKey(Handle))
	{
		return;
	}
	Curve->GetKey(Handle).InterpMode = Mode;
	Curve->AutoSetTangents();

	RefitValueFrame();
	NotifyChanged(/*bInteractive=*/false);
}

void FPCGExRampEditController::CommitInteractive()
{
	RefitValueFrame();
	NotifyChanged(/*bInteractive=*/false);
}

#pragma endregion

#pragma region Framing

void FPCGExRampEditController::RefitValueFrame()
{
	float MinV = 0.0f;
	float MaxV = 1.0f;

	if (Curve->Keys.Num() > 0)
	{
		MinV = MaxV = Curve->Keys[0].Value;
		for (const FRichCurveKey& Key : Curve->Keys)
		{
			MinV = FMath::Min(MinV, Key.Value);
			MaxV = FMath::Max(MaxV, Key.Value);
		}
	}

	float Span = MaxV - MinV;
	if (Span < KINDA_SMALL_NUMBER)
	{
		// Flat ramp: nothing to fit. Centre a unit window on the value.
		const float Center = (Curve->Keys.Num() > 0) ? Curve->Keys[0].Value : 0.5f;
		MinV = Center - 0.5f;
		MaxV = Center + 0.5f;
		Span = 1.0f;
	}

	const float Pad = Span * FramePadding;
	FrameMin = MinV - Pad;
	FrameMax = MaxV + Pad;
}

float FPCGExRampEditController::ValueToFrameAlpha(float Value) const
{
	const float Span = FrameMax - FrameMin;
	return (Span > KINDA_SMALL_NUMBER) ? (Value - FrameMin) / Span : 0.5f;
}

float FPCGExRampEditController::FrameAlphaToValue(float Alpha) const
{
	return FrameMin + Alpha * (FrameMax - FrameMin);
}

#pragma endregion
