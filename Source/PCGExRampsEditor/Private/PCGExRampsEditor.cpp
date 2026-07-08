// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExRampsEditor.h"

#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

#include "PCGExRampCustomization.h"
#include "PCGExRampsEditorStyle.h"
#include "PCGExRampTypes.h"

#define LOCTEXT_NAMESPACE "FPCGExRampsEditorModule"

void FPCGExRampsEditorModule::StartupModule()
{
	// Style first: the property customization builds widgets that look its brushes up by style name.
	Style = MakeShared<FPCGExRampsEditorStyle>();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomPropertyTypeLayout(
		FPCGExRamp::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExRampCustomization::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();
}

void FPCGExRampsEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FPCGExRamp::StaticStruct()->GetFName());
	}

	// Destroying the style unregisters it from the Slate style registry.
	Style.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExRampsEditorModule, PCGExRampsEditor)
