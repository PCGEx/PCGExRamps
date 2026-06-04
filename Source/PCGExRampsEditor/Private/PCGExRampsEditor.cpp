// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExRampsEditor.h"

#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

#include "PCGExRampCustomization.h"
#include "PCGExRampTypes.h"

#define LOCTEXT_NAMESPACE "FPCGExRampsEditorModule"

void FPCGExRampsEditorModule::StartupModule()
{
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
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExRampsEditorModule, PCGExRampsEditor)
