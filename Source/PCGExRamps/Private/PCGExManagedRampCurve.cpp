// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExManagedRampCurve.h"

#include "Curves/CurveFloat.h"

bool UPCGExManagedRampCurve::Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	// Honor the soft/hard release contract so the curve survives a regeneration and can be reused.
	// PCG's start-of-generation cleanup calls Release(bHardRelease=false); a reusable resource must
	// KEEP its state and return false there (the base marks it unused so the Resolve Ramp node can
	// MarkAsReused it). Only a hard release -- the end-of-gen sweep of an un-reused resource via
	// ReleaseIfUnused, or teardown -- drops the curve. No actors to delete either way.
	if (bHardRelease)
	{
		Curve = nullptr;
		Config.Reset();
	}
	return Super::Release(bHardRelease, OutActorsToDelete);
}
