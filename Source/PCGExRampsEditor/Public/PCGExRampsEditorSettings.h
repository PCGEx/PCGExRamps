// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "PCGExRampsEditorSettings.generated.h"

/**
 * Per-user editor preferences for PCGEx Ramps.
 * Access via Editor Preferences > Plugins > "PCGEx | Ramps".
 */
UCLASS(Config=EditorUser, DefaultConfig, meta=(DisplayName="PCGEx | Ramps", Description="PCGEx Editor Ramps Settings"))
class PCGEXRAMPSEDITOR_API UPCGExRampsEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Singleton accessor (the class default object holds the config-backed values). */
	static const UPCGExRampsEditorSettings* Get();

	//~ Begin UDeveloperSettings interface
	virtual FName GetContainerName() const override { return "Editor"; }
	virtual FName GetCategoryName() const override { return "Plugins"; }
	virtual FName GetSectionName() const override { return "PCGEx | Ramps"; }
	//~ End UDeveloperSettings interface

	/**
	 * Use Unreal's classic curve editor for FPCGExRamp instead of the inline PCGEx ramp editor.
	 *
	 * The PCGEx editor (default) shows Houdini-style normalized keys with a draggable key strip;
	 * the classic editor offers full tangent-handle control. Toggling this live-refreshes any
	 * open detail panels showing a ramp.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Ramp Editor")
	bool bUseLegacyCurveEditor = false;
};
