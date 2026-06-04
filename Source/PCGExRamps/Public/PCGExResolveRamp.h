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
		return true;
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
