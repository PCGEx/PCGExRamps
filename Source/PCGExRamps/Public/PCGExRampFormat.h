// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Curves/RichCurve.h"

/**
 * PCGExRamps compact-text format (v1). One string per ramp so it fits a single PCG attribute value.
 *
 *   Curve mode:  R1|C|pre,post|key;key;...     LUT mode:  R1|L|min,max|v0,v1,...
 *   key arity:   t,v  |  t,v,i  |  t,v,i,tm,at,lt,wm,aw,lw
 *                (i interp, tm tangent mode, at/lt arrive/leave tangent, wm weight mode, aw/lw weights)
 *   LUT min,max defaults to 0,1; samples bake to evenly spaced linear keys.
 *
 * Codes are our own stable contract, translated to/from engine enums at parse/serialize so engine
 * reshuffles can't corrupt a saved string:
 *   Interp 0 Linear, 1 Constant, 2 Cubic       Tangent 0 Auto, 1 User, 2 Break
 *   Weight 0 None, 1 Arrive, 2 Leave, 3 Both    Extrap 0 Constant, 1 Cycle, 2 CycleWithOffset, 3 Oscillate, 4 Linear
 */
namespace PCGExRampFormat
{
	/** Current format version, written as the first field ("R1"). */
	inline constexpr int32 Version = 1;

	/** Parse compact text into an FRichCurve (LUT baked to linear keys). False on malformed input; OutCurve is reset. */
	PCGEXRAMPS_API bool Parse(const FString& In, FRichCurve& OutCurve);

	/** Serialize an FRichCurve to compact text (curve mode), using minimal field arity per key. */
	PCGEXRAMPS_API FString SerializeCurve(const FRichCurve& InCurve);

	/** Serialize a sampled LUT (domain + values) to LUT-mode text. No in-plugin UI authors LUTs, but Parse accepts them. */
	PCGEXRAMPS_API FString SerializeLUT(float InMin, float InMax, TArrayView<const float> InSamples);

	/** Cheap structural validation (magic + mode) without building a curve. */
	PCGEXRAMPS_API bool IsValid(const FString& In);
}
