// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"
#include "PCGExResolveRamp.generated.h"

class UPCGPin;

/** How the Resolve Ramp node interprets its input attribute set. Exposed as palette variants. */
UENUM(BlueprintType)
enum class EPCGExRampSource : uint8
{
	/** Read the first String-typed attribute's value on the first row as a compact-text ramp. */
	String = 0 UMETA(DisplayName = "String"),
	/** Read the set as a key table: one row per key, with float Position + Value attributes. */
	AttributeSet = 1 UMETA(DisplayName = "Attribute Set"),
};

/** How the optional Min/Max Value pins derive their extremes. */
UENUM(BlueprintType)
enum class EPCGExRampValueRange : uint8
{
	/** Min/max of the key values. Exact for linear/constant ramps; ignores cubic overshoot between keys. */
	FromKeys = 0 UMETA(DisplayName = "From Keys"),
	/** Sample the evaluated curve across its domain to capture cubic overshoot/undershoot between keys. */
	Sampled = 1 UMETA(DisplayName = "Sampled"),
};

/**
 * Resolve Ramp. Pure consumer: reads one input attribute set, materializes a transient UCurveFloat,
 * and outputs its soft object path so native PCG nodes taking a TSoftObjectPtr<UCurveFloat> can use
 * it without a saved asset.
 *
 * Source modes (palette variants): String = first String attribute, first row; AttributeSet =
 * Position + Value float attributes, one row per key. Hand-authored ramps: author an FPCGExRamp and
 * feed its Data string into String mode via an attribute (see PCGExRampTypes.h).
 *
 * Both canonicalize to the same compact text, so identical curves dedup to one managed resource (CRC).
 * Non-cacheable + main-thread-only; each curve is a UPCGExManagedRampCurve on the component.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGExResolveRampSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override
	{
		return FName(TEXT("ResolveRamp"));
	}

	virtual FText GetDefaultNodeTitle() const override
	{
		return NSLOCTEXT("PCGExRamps", "ResolveRampTitle", "PCGEx | Resolve Ramp");
	}

	virtual FText GetNodeTooltipText() const override
	{
		return NSLOCTEXT("PCGExRamps", "ResolveRampTooltip", "Builds a transient curve from an input attribute set (string or key table) and outputs its soft object path for downstream curve inputs.");
	}

	virtual EPCGSettingsType GetType() const override
	{
		return EPCGSettingsType::Generic;
	}

	virtual bool ShouldDrawNodeCompact() const override
	{
		return !bOutputMinValue && !bOutputMaxValue && !bOutputMinPos && !bOutputMaxPos;
	}
	
	virtual bool GetCompactNodeIcon(FName& OutCompactNodeIcon) const override;

	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;
#endif

	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;

	/** How to interpret the input attribute set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EPCGExRampSource Source = EPCGExRampSource::String;

	/** Attribute Set mode: float attribute giving each key's position (time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "Source == EPCGExRampSource::AttributeSet", EditConditionHides))
	FName PositionAttributeName = FName(TEXT("Position"));

	/** Attribute Set mode: float attribute giving each key's value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "Source == EPCGExRampSource::AttributeSet", EditConditionHides))
	FName ValueAttributeName = FName(TEXT("Value"));

	/** Name of the output FSoftObjectPath attribute carrying the resolved curve path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FName OutputAttributeName = FName(TEXT("Curve"));

	// Opt-in bounds outputs. Each enabled toggle adds its own double-typed output pin describing the
	// resolved curve's bounding box: value pins = Y range, position pins = X (time) range.

	/** Output the curve's minimum value (Y) on a dedicated Min Value pin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (InlineEditConditionToggle))
	bool bOutputMinValue = false;

	/** Attribute name carrying the minimum value on the Min Value pin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOutputMinValue"))
	FName MinValueAttributeName = FName(TEXT("MinValue"));

	/** Output the curve's maximum value (Y) on a dedicated Max Value pin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (InlineEditConditionToggle))
	bool bOutputMaxValue = false;

	/** Attribute name carrying the maximum value on the Max Value pin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOutputMaxValue"))
	FName MaxValueAttributeName = FName(TEXT("MaxValue"));

	/** Output the curve's minimum position (X / time) on a dedicated Min Pos pin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (InlineEditConditionToggle))
	bool bOutputMinPos = false;

	/** Attribute name carrying the minimum position on the Min Pos pin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOutputMinPos"))
	FName MinPosAttributeName = FName(TEXT("MinPos"));

	/** Output the curve's maximum position (X / time) on a dedicated Max Pos pin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (InlineEditConditionToggle))
	bool bOutputMaxPos = false;

	/** Attribute name carrying the maximum position on the Max Pos pin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOutputMaxPos"))
	FName MaxPosAttributeName = FName(TEXT("MaxPos"));

	/** How the Min/Max Value pins derive their extremes (position pins always use the key time range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOutputMinValue || bOutputMaxValue", EditConditionHides))
	EPCGExRampValueRange ValueRangeMode = EPCGExRampValueRange::FromKeys;

	/** Sampled mode: number of evaluation samples across the curve domain (more = finer overshoot capture). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "(bOutputMinValue || bOutputMaxValue) && ValueRangeMode == EPCGExRampValueRange::Sampled", EditConditionHides, ClampMin = "2"))
	int32 ValueRangeSamples = 256;

	//~ Begin UPCGSettings interface
	// Non-cacheable so the managed-resource reuse/mark-used pass runs every generation.
	virtual bool IsCacheable() const override
	{
		return false;
	}

	virtual FPCGDataTypeIdentifier GetCurrentPinTypesID(const UPCGPin* InPin) const override;
	//~ End UPCGSettings interface

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class FPCGExResolveRampElement : public IPCGElement
{
public:
	// Creates a UObject and mutates the component's managed-resource set -- both game-thread only.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override
	{
		return true;
	}

	virtual bool IsCacheable(const UPCGSettings* InSettings) const override
	{
		return false;
	}

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
