// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Curves/CurveOwnerInterface.h"
#include "Curves/RichCurve.h"

class IPropertyHandle;
class SCurveEditor;

/**
 * Detail customization for FPCGExRamp.
 *
 * Parses the single Data string into a transient FRichCurve, exposes it through an inline
 * SCurveEditor (via FCurveOwnerInterface), and re-serializes back to the Data string on edit.
 * The raw string is also shown as a child row for inspection / copy-paste.
 */
class FPCGExRampCustomization : public IPropertyTypeCustomization, public FCurveOwnerInterface
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

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

	/** CurveData -> Data string (writes the property handle). */
	void PushToProperty();

	/** Handle to the inner FPCGExRamp::Data FString property. */
	TSharedPtr<IPropertyHandle> DataHandle;

	/** String last loaded from the property; preserved verbatim while the working curve still matches it. */
	FString SourceData;

	/** Transient working curve edited by the widget. */
	FRichCurve CurveData;

	/** The inline curve editor widget. */
	TSharedPtr<SCurveEditor> CurveWidget;
};
