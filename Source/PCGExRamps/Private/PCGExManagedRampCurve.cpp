// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExManagedRampCurve.h"

#include "Curves/CurveFloat.h"

bool UPCGExManagedRampCurve::Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	// Transient curve: nothing to persist, no actors to delete. Drop the reference for GC and return
	// true so PCG removes this resource from the component's managed set.
	Curve = nullptr;
	Config.Reset();
	return true;
}
