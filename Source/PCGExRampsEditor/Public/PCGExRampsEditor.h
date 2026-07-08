// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FPCGExRampsEditorStyle;

class FPCGExRampsEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Owns the ramp editor's Slate style (SVG key/slider brushes) for the module's lifetime. */
	TSharedPtr<FPCGExRampsEditorStyle> Style;
};
