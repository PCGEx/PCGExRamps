// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExResolveRamp.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Curves/CurveFloat.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Utils/PCGPreconfiguration.h"

#include "PCGExManagedRampCurve.h"
#include "PCGExRampFormat.h"

#define LOCTEXT_NAMESPACE "PCGExResolveRampElement"

namespace PCGExResolveRamp
{
	// Output pin labels for the opt-in bounds pins. Shared by OutputPinProperties (declaration),
	// GetCurrentPinTypesID (type tagging) and the element (data routing) -- keep them in one place.
	const FName MinValuePinLabel = FName(TEXT("Min Value"));
	const FName MaxValuePinLabel = FName(TEXT("Max Value"));
	const FName MinPosPinLabel = FName(TEXT("Min Pos"));
	const FName MaxPosPinLabel = FName(TEXT("Max Pos"));

	/**
	 * Min/max of the curve's evaluated values, sampling the domain to catch cubic overshoot/undershoot
	 * that key values alone miss. Seeded from the key range so coarse sampling can never report a range
	 * narrower than the keys themselves.
	 */
	void SampledValueRange(const FRichCurve& Curve, const int32 Samples, float& OutMin, float& OutMax)
	{
		Curve.GetValueRange(OutMin, OutMax);

		float MinTime = 0.0f;
		float MaxTime = 0.0f;
		Curve.GetTimeRange(MinTime, MaxTime);

		if (MaxTime <= MinTime || Samples < 2)
		{
			// Degenerate domain (single key or zero span): nothing to sample between -- keys are exact.
			return;
		}

		const float Span = MaxTime - MinTime;
		for (int32 i = 0; i < Samples; ++i)
		{
			const float Time = MinTime + Span * (static_cast<float>(i) / static_cast<float>(Samples - 1));
			const float Value = Curve.Eval(Time);
			OutMin = FMath::Min(OutMin, Value);
			OutMax = FMath::Max(OutMax, Value);
		}
	}

	/**
	 * Find a managed ramp curve matching both CRC and exact config string (the string check rules out a
	 * 32-bit CRC collision) and mark it reused so PCG keeps it this generation. Self-contained re-impl so
	 * the plugin needs no PCGEx dependency.
	 */
	UPCGExManagedRampCurve* TryReuseRampCurve(UPCGComponent* Component, const FPCGCrc& Crc, const FString& Config)
	{
		if (!Component || !Crc.IsValid())
		{
			return nullptr;
		}

		UPCGExManagedRampCurve* Found = nullptr;
		Component->ForEachManagedResource(
			[&Found, &Crc, &Config](UPCGManagedResource* Resource)
			{
				if (Found)
				{
					return;
				}

				UPCGExManagedRampCurve* Typed = Cast<UPCGExManagedRampCurve>(Resource);
				if (!Typed || !Typed->GetCrc().IsValid() || !(Typed->GetCrc() == Crc) || Typed->Config != Config)
				{
					return;
				}

				Typed->MarkAsReused();
				Found = Typed;
			});

		return Found;
	}

	/**
	 * Resolve one canonical config string to a live curve's soft object path, creating + registering a
	 * managed resource if no identical one already exists on the component. Game thread only.
	 */
	FSoftObjectPath EnsureCurve(FPCGContext* Context, UPCGComponent* Component, const FString& Config)
	{
		if (Config.IsEmpty())
		{
			return FSoftObjectPath();
		}

		// Parse once and honor the result -- don't silently emit an empty curve for an unparseable
		// (e.g. wrong-version) string the way a structural-only IsValid() check would.
		FRichCurve Curve;
		if (!PCGExRampFormat::Parse(Config, Curve))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context,
			           FText::Format(NSLOCTEXT("PCGExRamps", "BadConfig", "Resolve Ramp could not parse the ramp string '{0}'; no curve was created."), FText::FromString(Config)));
			return FSoftObjectPath();
		}

		// Canonicalize via the serializer so raw-string and key-table authoring of the same curve hash
		// identically and dedup ("1" and "1.0" are otherwise distinct strings).
		const FString Canonical = PCGExRampFormat::SerializeCurve(Curve);

		if (!Component)
		{
			// No component = nowhere to anchor the curve's lifetime, so the soft path would dangle.
			PCGE_LOG_C(Warning, GraphAndLog, Context,
			           NSLOCTEXT("PCGExRamps", "NoComponent", "Resolve Ramp needs an owning PCG component to manage curve lifetime; no curve was created."));
			return FSoftObjectPath();
		}

		const FPCGCrc Crc(FCrc::StrCrc32<TCHAR>(*Canonical));

		// Reuse an identical curve already materialized on this component (dedup + no rebuild churn).
		if (const UPCGExManagedRampCurve* Existing = TryReuseRampCurve(Component, Crc, Canonical))
		{
			if (Existing->Curve)
			{
				return FSoftObjectPath(Existing->Curve);
			}
		}

		check(IsInGameThread());

		// Outer the resource to the component (matches StagingLoadLevel); outer the curve to the
		// resource so its lifetime is unambiguous and its object path is unique + resolvable.
		UPCGExManagedRampCurve* Managed = NewObject<UPCGExManagedRampCurve>(Component);
		Managed->SetCrc(Crc);
		Managed->Config = Canonical;

		const FName CurveName = MakeUniqueObjectName(Managed, UCurveFloat::StaticClass(), FName(TEXT("RampCurve")));
		UCurveFloat* CurveAsset = NewObject<UCurveFloat>(Managed, CurveName, RF_Transient);
		CurveAsset->FloatCurve = MoveTemp(Curve);

		Managed->Curve = CurveAsset;
		Component->AddToManagedResources(Managed);

		return FSoftObjectPath(CurveAsset);
	}

	/** String mode: pull the first String-typed attribute's value at the first row. */
	bool ReadFirstStringValue(const UPCGMetadata* Metadata, FString& OutString)
	{
		TArray<FName> Names;
		TArray<EPCGMetadataTypes> Types;
		Metadata->GetAttributes(Names, Types);

		for (int32 i = 0; i < Names.Num(); ++i)
		{
			if (Types[i] != EPCGMetadataTypes::String)
			{
				continue;
			}

			if (const FPCGMetadataAttribute<FString>* Attr = Metadata->GetConstTypedAttribute<FString>(Names[i]))
			{
				OutString = Attr->GetValueFromItemKey(0);
				return true;
			}
		}

		return false;
	}

	/** Returns the attribute if it exists and is a scalar float/double; reports which via OutType. */
	const FPCGMetadataAttributeBase* GetNumericAttribute(const UPCGMetadata* Metadata, const FName Name, EPCGMetadataTypes& OutType)
	{
		const FPCGMetadataAttributeBase* Base = Metadata->GetConstAttribute(Name);
		if (!Base)
		{
			return nullptr;
		}

		OutType = static_cast<EPCGMetadataTypes>(Base->GetTypeId());
		return (OutType == EPCGMetadataTypes::Double || OutType == EPCGMetadataTypes::Float) ? Base : nullptr;
	}

	double ReadNumericValue(const FPCGMetadataAttributeBase* Base, const EPCGMetadataTypes Type, const PCGMetadataEntryKey Key)
	{
		if (Type == EPCGMetadataTypes::Double)
		{
			return static_cast<const FPCGMetadataAttribute<double>*>(Base)->GetValueFromItemKey(Key);
		}
		return static_cast<const FPCGMetadataAttribute<float>*>(Base)->GetValueFromItemKey(Key);
	}

	/** AttributeSet mode: build a curve from a key table (one row per key, sorted by position). */
	bool ReadKeyTableCurve(const UPCGMetadata* Metadata, const FName PositionName, const FName ValueName, FRichCurve& OutCurve)
	{
		EPCGMetadataTypes PositionType = EPCGMetadataTypes::Double;
		EPCGMetadataTypes ValueType = EPCGMetadataTypes::Double;
		const FPCGMetadataAttributeBase* PositionAttr = GetNumericAttribute(Metadata, PositionName, PositionType);
		const FPCGMetadataAttributeBase* ValueAttr = GetNumericAttribute(Metadata, ValueName, ValueType);
		if (!PositionAttr || !ValueAttr)
		{
			return false;
		}

		const int64 Count = Metadata->GetLocalItemCount();
		if (Count <= 0)
		{
			return false;
		}

		TArray<TPair<double, double>> Keys; // (position, value)
		Keys.Reserve(static_cast<int32>(Count));
		for (int64 Key = 0; Key < Count; ++Key)
		{
			Keys.Emplace(ReadNumericValue(PositionAttr, PositionType, Key), ReadNumericValue(ValueAttr, ValueType, Key));
		}

		// FRichCurve expects time-sorted keys; the attribute rows are not guaranteed ordered. Sorting
		// up front also keeps AddKey on its O(1) append fast-path in the loop below.
		Keys.Sort([](const TPair<double, double>& A, const TPair<double, double>& B) { return A.Key < B.Key; });

		OutCurve.Reset();
		for (const TPair<double, double>& Key : Keys)
		{
			const FKeyHandle Handle = OutCurve.AddKey(static_cast<float>(Key.Key), static_cast<float>(Key.Value));
			OutCurve.GetKey(Handle).InterpMode = RCIM_Linear;
		}
		// All keys linear -- tangents are unused, so no AutoSetTangents needed.

		return true;
	}
}

#pragma region UPCGExResolveRampSettings

#if WITH_EDITOR
bool UPCGExResolveRampSettings::GetCompactNodeIcon(FName& OutCompactNodeIcon) const
{
	OutCompactNodeIcon = PCGNodeConstants::Icons::CompactNodeConvert;
	return true;
}

TArray<FPCGPreConfiguredSettingsInfo> UPCGExResolveRampSettings::GetPreconfiguredInfo() const
{
	return FPCGPreConfiguredSettingsInfo::PopulateFromEnum<EPCGExRampSource>({}, INVTEXT("{0}"));
}
#endif

void UPCGExResolveRampSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo)
{
	Super::ApplyPreconfiguredSettings(PreconfigureInfo);

	if (const UEnum* EnumPtr = StaticEnum<EPCGExRampSource>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfigureInfo.PreconfiguredIndex))
		{
			Source = static_cast<EPCGExRampSource>(PreconfigureInfo.PreconfiguredIndex);
		}
	}
}

FPCGDataTypeIdentifier UPCGExResolveRampSettings::GetCurrentPinTypesID(const UPCGPin* InPin) const
{
	if (InPin && InPin->IsOutputPin())
	{
		const FName Label = InPin->Properties.Label;
		if (Label == PCGExResolveRamp::MinValuePinLabel || Label == PCGExResolveRamp::MaxValuePinLabel ||
			Label == PCGExResolveRamp::MinPosPinLabel || Label == PCGExResolveRamp::MaxPosPinLabel)
		{
			// Bounds pins each carry a single double.
			FPCGDataTypeIdentifier Id = FPCGDataTypeInfoParam::AsId();
			Id.CustomSubtype = static_cast<int32>(EPCGMetadataTypes::Double);
			return Id;
		}

		// The main output pin carries the resolved curve path.
		FPCGDataTypeIdentifier Id = FPCGDataTypeInfoParam::AsId();
		Id.CustomSubtype = static_cast<int32>(EPCGMetadataTypes::SoftObjectPath);
		return Id;
	}

	// Input: enforce String in String mode. AttributeSet mode carries a multi-float key table, so
	// there's no single attribute subtype to pin it to -- leave it a generic Param.
	if (Source == EPCGExRampSource::String)
	{
		FPCGDataTypeIdentifier Id = FPCGDataTypeInfoParam::AsId();
		Id.CustomSubtype = static_cast<int32>(EPCGMetadataTypes::String);
		return Id;
	}

	return FPCGDataTypeInfoParam::AsId();
}

TArray<FPCGPinProperties> UPCGExResolveRampSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	FPCGPinProperties& In = Pins.Emplace_GetRef(
		PCGPinConstants::DefaultInputLabel,
		EPCGDataType::Param,
		/*bInAllowMultipleConnections=*/false,
		/*bInAllowMultipleData=*/false);
	In.SetRequiredPin();
#if WITH_EDITOR
	In.Tooltip = NSLOCTEXT("PCGExRamps", "InPinTooltip", "One attribute set describing the ramp -- a String attribute (String mode) or Position/Value float key rows (Attribute Set mode).");
#endif
	return Pins;
}

TArray<FPCGPinProperties> UPCGExResolveRampSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	FPCGPinProperties& Out = Pins.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);
#if WITH_EDITOR
	Out.Tooltip = NSLOCTEXT("PCGExRamps", "OutPinTooltip", "Attribute set carrying the resolved curve soft path under OutputAttributeName.");
#endif

	// Opt-in bounds pins -- present only when their toggle is enabled.
	if (bOutputMinValue)
	{
		FPCGPinProperties& P = Pins.Emplace_GetRef(PCGExResolveRamp::MinValuePinLabel, EPCGDataType::Param);
#if WITH_EDITOR
		P.Tooltip = NSLOCTEXT("PCGExRamps", "MinValuePinTooltip", "The curve's minimum value (Y) as a double, under MinValueAttributeName.");
#endif
	}
	if (bOutputMaxValue)
	{
		FPCGPinProperties& P = Pins.Emplace_GetRef(PCGExResolveRamp::MaxValuePinLabel, EPCGDataType::Param);
#if WITH_EDITOR
		P.Tooltip = NSLOCTEXT("PCGExRamps", "MaxValuePinTooltip", "The curve's maximum value (Y) as a double, under MaxValueAttributeName.");
#endif
	}
	if (bOutputMinPos)
	{
		FPCGPinProperties& P = Pins.Emplace_GetRef(PCGExResolveRamp::MinPosPinLabel, EPCGDataType::Param);
#if WITH_EDITOR
		P.Tooltip = NSLOCTEXT("PCGExRamps", "MinPosPinTooltip", "The curve's minimum position (X / time) as a double, under MinPosAttributeName.");
#endif
	}
	if (bOutputMaxPos)
	{
		FPCGPinProperties& P = Pins.Emplace_GetRef(PCGExResolveRamp::MaxPosPinLabel, EPCGDataType::Param);
#if WITH_EDITOR
		P.Tooltip = NSLOCTEXT("PCGExRamps", "MaxPosPinTooltip", "The curve's maximum position (X / time) as a double, under MaxPosAttributeName.");
#endif
	}

	return Pins;
}

FPCGElementPtr UPCGExResolveRampSettings::CreateElement() const
{
	return MakeShared<FPCGExResolveRampElement>();
}

#pragma endregion

#pragma region FPCGExResolveRampElement

bool FPCGExResolveRampElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExResolveRampElement::Execute);

	// Guaranteed by CanExecuteOnlyOnMainThread; NewObject + managed-resource registration require it.
	check(IsInGameThread());

	const UPCGExResolveRampSettings* Settings = Context->GetInputSettings<UPCGExResolveRampSettings>();
	check(Settings);

	// ExecutionSource (PCG 5.6+) replaces the deprecated SourceComponent; GetObject() yields the backing
	// UPCGComponent.
	UPCGComponent* Component = Cast<UPCGComponent>(Context->ExecutionSource.GetObject());

	// Pure consumer: one input attribute set -> one curve. Use the first param data on the input pin.
	const UPCGParamData* InParam = nullptr;
	for (const FPCGTaggedData& Tagged : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		InParam = Cast<UPCGParamData>(Tagged.Data);
		if (InParam)
		{
			break;
		}
	}

	// Resolve the input into one compact-text config (both modes funnel through this).
	FString Config;
	if (InParam && InParam->ConstMetadata())
	{
		const UPCGMetadata* Metadata = InParam->ConstMetadata();

		if (Settings->Source == EPCGExRampSource::String)
		{
			if (!PCGExResolveRamp::ReadFirstStringValue(Metadata, Config))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
				           NSLOCTEXT("PCGExRamps", "NoStringAttr", "String mode: the input attribute set has no String-typed attribute."));
			}
		}
		else
		{
			FRichCurve Built;
			if (PCGExResolveRamp::ReadKeyTableCurve(Metadata, Settings->PositionAttributeName, Settings->ValueAttributeName, Built))
			{
				Config = PCGExRampFormat::SerializeCurve(Built);
			}
			else
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
				           FText::Format(NSLOCTEXT("PCGExRamps", "NoKeyAttrs", "Attribute Set mode: the input needs float attributes '{0}' (position) and '{1}' (value)."),
				                         FText::FromName(Settings->PositionAttributeName), FText::FromName(Settings->ValueAttributeName)));
			}
		}
	}
	else
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context,
		           NSLOCTEXT("PCGExRamps", "NoInputSet", "Resolve Ramp needs one input attribute set."));
	}

	// One output entry carrying the resolved curve path (empty path if the input was missing/invalid).
	// NewObject_AnyThread is PCG's sanctioned creation path -- it tracks the object on the context.
	UPCGParamData* OutData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
	check(OutData && OutData->Metadata);

	FPCGMetadataAttribute<FSoftObjectPath>* PathAttr = OutData->Metadata->CreateAttribute<FSoftObjectPath>(
		Settings->OutputAttributeName,
		FSoftObjectPath(),
		/*bAllowsInterpolation=*/false,
		/*bOverrideParent=*/false);

	const FSoftObjectPath Path = PCGExResolveRamp::EnsureCurve(Context, Component, Config);
	const int64 EntryKey = OutData->Metadata->AddEntry();
	if (PathAttr)
	{
		PathAttr->SetValue(EntryKey, Path);
	}

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = OutData;
	Output.Pin = PCGPinConstants::DefaultOutputLabel;

	// Opt-in bounds pins. Bounds describe the curve itself, so they're independent of the managed-resource
	// path above -- they're emitted whenever the config parses, even if no component anchored the curve.
	const bool bAnyBounds = Settings->bOutputMinValue || Settings->bOutputMaxValue ||
		Settings->bOutputMinPos || Settings->bOutputMaxPos;

	if (bAnyBounds && !Config.IsEmpty())
	{
		FRichCurve BoundsCurve;
		if (PCGExRampFormat::Parse(Config, BoundsCurve))
		{
			float MinTime = 0.0f;
			float MaxTime = 0.0f;
			BoundsCurve.GetTimeRange(MinTime, MaxTime);

			float MinValue = 0.0f;
			float MaxValue = 0.0f;
			if (Settings->ValueRangeMode == EPCGExRampValueRange::Sampled)
			{
				PCGExResolveRamp::SampledValueRange(BoundsCurve, Settings->ValueRangeSamples, MinValue, MaxValue);
			}
			else
			{
				BoundsCurve.GetValueRange(MinValue, MaxValue);
			}

			// Emit one single-entry double param on the named pin.
			auto EmitBound = [Context](const FName PinLabel, const FName AttrName, const double Value)
			{
				UPCGParamData* Data = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
				check(Data && Data->Metadata);

				FPCGMetadataAttribute<double>* Attr = Data->Metadata->CreateAttribute<double>(
					AttrName, 0.0, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/false);
				const int64 EntryKey = Data->Metadata->AddEntry();
				if (Attr)
				{
					Attr->SetValue(EntryKey, Value);
				}

				FPCGTaggedData& Tagged = Context->OutputData.TaggedData.Emplace_GetRef();
				Tagged.Data = Data;
				Tagged.Pin = PinLabel;
			};

			if (Settings->bOutputMinValue) { EmitBound(PCGExResolveRamp::MinValuePinLabel, Settings->MinValueAttributeName, MinValue); }
			if (Settings->bOutputMaxValue) { EmitBound(PCGExResolveRamp::MaxValuePinLabel, Settings->MaxValueAttributeName, MaxValue); }
			if (Settings->bOutputMinPos) { EmitBound(PCGExResolveRamp::MinPosPinLabel, Settings->MinPosAttributeName, MinTime); }
			if (Settings->bOutputMaxPos) { EmitBound(PCGExResolveRamp::MaxPosPinLabel, Settings->MaxPosAttributeName, MaxTime); }
		}
	}

	return true;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
