// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Curves/CurveOwnerInterface.h"
#include "Curves/RichCurve.h"

class IPropertyHandle;
class IPropertyUtilities;
class SCurveEditor;
class SWidget;
class FPCGExRampEditController;
struct FPropertyChangedEvent;

/**
 * Detail customization for FPCGExRamp.
 *
 * Parses the single Data string into a transient FRichCurve and edits it through one of two views:
 *   - the inline PCGEx ramp editor (default): SPCGExRampEditor, a Houdini-style normalized editor
 *   - the classic SCurveEditor (FCurveOwnerInterface), when bUseLegacyCurveEditor is set
 *
 * Either view mutates the same shared FRichCurve; edits are re-serialized back to the Data string.
 * The raw string is also shown as a child row for inspection / copy-paste. The editor choice
 * (UPCGExRampsEditorSettings) is honoured per panel and live-refreshes when toggled.
 */
class FPCGExRampCustomization : public IPropertyTypeCustomization, public FCurveOwnerInterface
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual ~FPCGExRampCustomization();

	//~ Begin IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization interface

	//~ Begin FCurveOwnerInterface interface
	// The TArray overload is deprecated (5.6) but still pure; the editor uses the TAdderReserverRef one.
	UE_DEPRECATED(5.6, "Use version taking a TAdderReserverRef")
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual void GetCurves(TAdderReserverRef<FRichCurveEditInfoConst> Curves) const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;
	virtual void ModifyOwner() override;
	virtual TArray<const UObject*> GetOwners() const override;
	virtual void MakeTransactional() override;
	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;
	virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) override;
	//~ End FCurveOwnerInterface interface

private:
	/** Data string -> CurveData. */
	void PullFromProperty();

	/** CurveData -> Data string (writes the property handle). bInteractive coalesces the edit (mid-drag). */
	void PushToProperty(bool bInteractive = false);

	/** Build the inline PCGEx ramp editor over the shared curve. */
	TSharedRef<SWidget> BuildRampEditor();

	/** Build the classic SCurveEditor over the shared curve (via FCurveOwnerInterface). */
	TSharedRef<SWidget> BuildLegacyEditor();

	/** Controller change callback -> re-serialize. */
	void HandleRampChanged(bool bInteractive);

	/** Editor-settings change callback -> rebuild this panel with the now-selected editor. */
	void HandleSettingsChanged(UObject* Object, FPropertyChangedEvent& Event);

	/** Handle to the inner FPCGExRamp::Data FString property. */
	TSharedPtr<IPropertyHandle> DataHandle;

	/** For ForceRefresh when the editor-choice setting changes. */
	TSharedPtr<IPropertyUtilities> PropertyUtilities;

	/** String last loaded from the property; preserved verbatim while the working curve still matches it. */
	FString SourceData;

	/** Transient working curve edited by either view (shared with the controller). */
	TSharedPtr<FRichCurve> CurveData = MakeShared<FRichCurve>();

	/** Drives the inline ramp editor; null when the legacy editor is active. */
	TSharedPtr<FPCGExRampEditController> Controller;

	/** The classic curve editor widget; null when the inline editor is active. */
	TSharedPtr<SCurveEditor> CurveWidget;

	/** Subscription to UPCGExRampsEditorSettings::OnSettingChanged. */
	FDelegateHandle SettingsChangedHandle;
};
