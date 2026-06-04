// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGManagedResource.h"
#include "PCGExManagedRampCurve.generated.h"

class UCurveFloat;

/**
 * PCG managed resource owning a transient UCurveFloat built by the Resolve Ramp node.
 *
 * Lifetime: the Curve UPROPERTY keeps the curve alive; the component's managed-resource set keeps
 * this alive. PCG marks all resources unused each (re)generation; the node re-marks the ones it still
 * needs (by CRC), and PCG Release()s the rest -- so stale configs drop and identical ones are reused.
 * The CRC lives on the base (SetCrc/GetCrc); Config holds the source string to rule out CRC collisions.
 */
UCLASS()
class PCGEXRAMPS_API UPCGExManagedRampCurve : public UPCGManagedResource
{
	GENERATED_BODY()

public:
	//~ Begin UPCGManagedResource interface
	virtual bool Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	//~ End UPCGManagedResource interface

	/** The transient curve, kept alive by this strong reference for the resource's lifetime. */
	UPROPERTY(Transient)
	TObjectPtr<UCurveFloat> Curve = nullptr;

	/** The exact config string this curve was built from -- used to rule out CRC collisions on reuse. */
	UPROPERTY(Transient)
	FString Config;
};
