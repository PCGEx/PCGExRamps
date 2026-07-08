// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExRampsEditor : ModuleRules
{
	public PCGExRampsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PCGExRamps",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",          // SCurveEditor + FCurveOwnerInterface live here
			"EditorWidgets",
			"PropertyEditor",    // RegisterCustomPropertyTypeLayout
			"DeveloperSettings", // UPCGExRampsEditorSettings (UDeveloperSettings)
			"Projects",          // IPluginManager -> plugin Resources dir for the Slate style
		});
	}
}
