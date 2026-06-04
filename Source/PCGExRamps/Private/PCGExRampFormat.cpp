// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExRampFormat.h"

// File-local helpers in a named namespace (not anonymous/static) so they're safe under Unity builds.
namespace PCGExRampFormat
{
	ERichCurveInterpMode CodeToInterp(int32 Code)
	{
		switch (Code)
		{
		case 1:
			return RCIM_Constant;
		case 2:
			return RCIM_Cubic;
		default:
			return RCIM_Linear;
		}
	}

	int32 InterpToCode(ERichCurveInterpMode Mode)
	{
		switch (Mode)
		{
		case RCIM_Constant:
			return 1;
		case RCIM_Cubic:
			return 2;
		default:
			return 0; // Linear (and None, treated as Linear)
		}
	}

	ERichCurveTangentMode CodeToTangentMode(int32 Code)
	{
		switch (Code)
		{
		case 1:
			return RCTM_User;
		case 2:
			return RCTM_Break;
		default:
			return RCTM_Auto;
		}
	}

	int32 TangentModeToCode(ERichCurveTangentMode Mode)
	{
		switch (Mode)
		{
		case RCTM_User:
			return 1;
		case RCTM_Break:
			return 2;
		default:
			return 0; // Auto (and None, treated as Auto)
		}
	}

	ERichCurveTangentWeightMode CodeToWeightMode(int32 Code)
	{
		switch (Code)
		{
		case 1:
			return RCTWM_WeightedArrive;
		case 2:
			return RCTWM_WeightedLeave;
		case 3:
			return RCTWM_WeightedBoth;
		default:
			return RCTWM_WeightedNone;
		}
	}

	int32 WeightModeToCode(ERichCurveTangentWeightMode Mode)
	{
		switch (Mode)
		{
		case RCTWM_WeightedArrive:
			return 1;
		case RCTWM_WeightedLeave:
			return 2;
		case RCTWM_WeightedBoth:
			return 3;
		default:
			return 0; // None
		}
	}

	ERichCurveExtrapolation CodeToExtrap(int32 Code)
	{
		switch (Code)
		{
		case 1:
			return RCCE_Cycle;
		case 2:
			return RCCE_CycleWithOffset;
		case 3:
			return RCCE_Oscillate;
		case 4:
			return RCCE_Linear;
		default:
			return RCCE_Constant;
		}
	}

	int32 ExtrapToCode(ERichCurveExtrapolation Mode)
	{
		switch (Mode)
		{
		case RCCE_Cycle:
			return 1;
		case RCCE_CycleWithOffset:
			return 2;
		case RCCE_Oscillate:
			return 3;
		case RCCE_Linear:
			return 4;
		default:
			return 0; // Constant (and None, treated as Constant)
		}
	}

	/** Parse-stable float token. */
	FString FloatToToken(float Value)
	{
		return FString::SanitizeFloat(Value);
	}

	bool Parse(const FString& In, FRichCurve& OutCurve)
	{
		OutCurve.Reset();

		TArray<FString> Parts;
		In.ParseIntoArray(Parts, TEXT("|"), /*InCullEmpty=*/false);
		if (Parts.Num() < 4)
		{
			return false;
		}

		// Magic + version, e.g. "R1".
		const FString& Tag = Parts[0];
		if (!Tag.StartsWith(TEXT("R")))
		{
			return false;
		}
		const int32 Ver = FCString::Atoi(*Tag.RightChop(1));
		if (Ver != Version)
		{
			return false;
		}

		const FString& Mode = Parts[1];

		if (Mode == TEXT("C"))
		{
			TArray<FString> Meta;
			Parts[2].ParseIntoArray(Meta, TEXT(","), /*InCullEmpty=*/false);
			if (Meta.Num() >= 2)
			{
				OutCurve.PreInfinityExtrap = CodeToExtrap(FCString::Atoi(*Meta[0]));
				OutCurve.PostInfinityExtrap = CodeToExtrap(FCString::Atoi(*Meta[1]));
			}

			TArray<FString> KeyTokens;
			Parts[3].ParseIntoArray(KeyTokens, TEXT(";"), /*InCullEmpty=*/true);
			for (const FString& KeyTok : KeyTokens)
			{
				TArray<FString> F;
				KeyTok.ParseIntoArray(F, TEXT(","), /*InCullEmpty=*/false);
				if (F.Num() < 2)
				{
					continue;
				}

				const float Time = FCString::Atof(*F[0]);
				const float Value = FCString::Atof(*F[1]);
				const FKeyHandle Handle = OutCurve.AddKey(Time, Value);
				FRichCurveKey& Key = OutCurve.GetKey(Handle);

				Key.InterpMode = (F.Num() >= 3) ? CodeToInterp(FCString::Atoi(*F[2])) : RCIM_Linear;

				if (F.Num() >= 9)
				{
					Key.TangentMode = CodeToTangentMode(FCString::Atoi(*F[3]));
					Key.ArriveTangent = FCString::Atof(*F[4]);
					Key.LeaveTangent = FCString::Atof(*F[5]);
					Key.TangentWeightMode = CodeToWeightMode(FCString::Atoi(*F[6]));
					Key.ArriveTangentWeight = FCString::Atof(*F[7]);
					Key.LeaveTangentWeight = FCString::Atof(*F[8]);
				}
				else
				{
					Key.TangentMode = RCTM_Auto;
				}
			}

			if (OutCurve.Keys.Num() == 0)
			{
				// "R1|C||" etc: valid header, no usable keys -- reject rather than return a 0-key curve.
				return false;
			}

			// Recompute tangents for Auto keys; User/Break keys keep their stored tangents.
			OutCurve.AutoSetTangents();
			return true;
		}

		if (Mode == TEXT("L"))
		{
			TArray<FString> Meta;
			Parts[2].ParseIntoArray(Meta, TEXT(","), /*InCullEmpty=*/false);
			const float Min = (Meta.Num() >= 1 && !Meta[0].IsEmpty()) ? FCString::Atof(*Meta[0]) : 0.0f;
			const float Max = (Meta.Num() >= 2 && !Meta[1].IsEmpty()) ? FCString::Atof(*Meta[1]) : 1.0f;

			TArray<FString> ValueTokens;
			Parts[3].ParseIntoArray(ValueTokens, TEXT(","), /*InCullEmpty=*/true);
			const int32 Num = ValueTokens.Num();
			if (Num == 0)
			{
				return false;
			}

			if (Num == 1)
			{
				OutCurve.AddKey(Min, FCString::Atof(*ValueTokens[0]));
			}
			else
			{
				const float Span = Max - Min;
				if (Span <= 0.0f)
				{
					// Multi-sample LUT needs an increasing domain; Max <= Min would stack/reverse keys.
					return false;
				}
				for (int32 i = 0; i < Num; ++i)
				{
					const float T = Min + Span * (static_cast<float>(i) / static_cast<float>(Num - 1));
					const FKeyHandle Handle = OutCurve.AddKey(T, FCString::Atof(*ValueTokens[i]));
					OutCurve.GetKey(Handle).InterpMode = RCIM_Linear;
				}
			}

			// A LUT carries no extrapolation info; leave the FRichCurve defaults (Constant).
			return true;
		}

		return false;
	}

	FString SerializeCurve(const FRichCurve& InCurve)
	{
		FString Out = FString::Printf(
			TEXT("R%d|C|%d,%d|"),
			Version,
			ExtrapToCode(InCurve.PreInfinityExtrap),
			ExtrapToCode(InCurve.PostInfinityExtrap));

		bool bFirst = true;
		for (const FRichCurveKey& Key : InCurve.Keys)
		{
			if (!bFirst)
			{
				Out += TEXT(";");
			}
			bFirst = false;

			Out += FloatToToken(Key.Time);
			Out += TEXT(",");
			Out += FloatToToken(Key.Value);

			const bool bAuto = (Key.TangentMode == RCTM_Auto || Key.TangentMode == RCTM_None);
			const bool bWeighted = (Key.TangentWeightMode != RCTWM_WeightedNone);

			if (Key.InterpMode == RCIM_Cubic && (!bAuto || bWeighted))
			{
				// Full arity -- the tangents/weights matter and cannot be recomputed from t,v alone.
				Out += FString::Printf(
					TEXT(",%d,%d,%s,%s,%d,%s,%s"),
					InterpToCode(Key.InterpMode),
					TangentModeToCode(Key.TangentMode),
					*FloatToToken(Key.ArriveTangent),
					*FloatToToken(Key.LeaveTangent),
					WeightModeToCode(Key.TangentWeightMode),
					*FloatToToken(Key.ArriveTangentWeight),
					*FloatToToken(Key.LeaveTangentWeight));
			}
			else if (Key.InterpMode != RCIM_Linear)
			{
				// Constant, or Cubic-with-auto-tangents: interp code is enough (tangents recomputed on parse).
				Out += FString::Printf(TEXT(",%d"), InterpToCode(Key.InterpMode));
			}
		}

		return Out;
	}

	FString SerializeLUT(float InMin, float InMax, TArrayView<const float> InSamples)
	{
		FString Out = FString::Printf(TEXT("R%d|L|%s,%s|"), Version, *FloatToToken(InMin), *FloatToToken(InMax));

		for (int32 i = 0; i < InSamples.Num(); ++i)
		{
			if (i > 0)
			{
				Out += TEXT(",");
			}
			Out += FloatToToken(InSamples[i]);
		}

		return Out;
	}

	bool IsValid(const FString& In)
	{
		TArray<FString> Parts;
		In.ParseIntoArray(Parts, TEXT("|"), /*InCullEmpty=*/false);
		if (Parts.Num() < 4 || !Parts[0].StartsWith(TEXT("R")))
		{
			return false;
		}
		return Parts[1] == TEXT("C") || Parts[1] == TEXT("L");
	}
}
