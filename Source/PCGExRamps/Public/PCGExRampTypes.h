// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRampTypes.generated.h"

/**
 * Authoring container for a ramp. The whole ramp (keys, tangents, interp, extrapolation, or a sampled
 * LUT) encodes into one compact-text string (grammar in PCGExRampFormat.h) so it fits a single PCG
 * attribute value -- PCG attributes can't hold a nested struct. The editor module gives this an inline
 * curve editor over the Data string.
 *
 * FPCGExRamp is a standalone authoring container; the Resolve Ramp node consumes attribute sets, not
 * this struct. To drive the node, surface Data as a String attribute and feed it into String mode.
 */
USTRUCT(BlueprintType)
struct PCGEXRAMPS_API FPCGExRamp
{
	GENERATED_BODY()

	/** Serialized ramp payload in PCGExRamps compact-text format. Default is a linear 0->1 ramp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramp")
	FString Data = TEXT("R1|C|0,0|0,0;1,1");
};
