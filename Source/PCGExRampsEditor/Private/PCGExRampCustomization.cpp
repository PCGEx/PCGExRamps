// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExRampCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "SCurveEditor.h"
#include "Widgets/Layout/SBox.h"

#include "PCGExRampFormat.h"
#include "PCGExRampTypes.h"

#define LOCTEXT_NAMESPACE "PCGExRampCustomization"

TSharedRef<IPropertyTypeCustomization> FPCGExRampCustomization::MakeInstance()
{
	return MakeShared<FPCGExRampCustomization>();
}

void FPCGExRampCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	DataHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExRamp, Data));
	PullFromProperty();

	// Owner is wired via SetCurveOwner() after construction. To swap to the modern Curve Editor panel
	// (FCurveEditor / SCurveEditorPanel), this is the single place.
	TSharedRef<SCurveEditor> Editor = SNew(SCurveEditor)
		.ViewMinInput(0.0f)
		.ViewMaxInput(1.0f)
		.ViewMinOutput(0.0f)
		.ViewMaxOutput(1.0f)
		.HideUI(false)
		.DesiredSize(FVector2D(320.0f, 140.0f));

	CurveWidget = Editor;
	Editor->SetCurveOwner(this);

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(320.0f)
		[
			SNew(SBox)
			.HeightOverride(140.0f)
			[
				Editor
			]
		];
}

void FPCGExRampCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Expose the raw string for inspection / copy-paste (editing it here doesn't live-refresh the
	// curve widget until the panel rebuilds).
	if (DataHandle.IsValid())
	{
		ChildBuilder.AddProperty(DataHandle.ToSharedRef());
	}
}

void FPCGExRampCustomization::PullFromProperty()
{
	FString Data;
	if (DataHandle.IsValid() && DataHandle->GetValue(Data) == FPropertyAccess::Success)
	{
		SourceData = Data;
		PCGExRampFormat::Parse(Data, CurveData);
	}
}

void FPCGExRampCustomization::PushToProperty()
{
	if (!DataHandle.IsValid())
	{
		return;
	}

	const FString NewData = PCGExRampFormat::SerializeCurve(CurveData);

	// If the working curve still matches what we loaded, keep the original string verbatim -- avoids
	// formatting churn ("1" -> "1.0") and silently rewriting a LUT source into curve mode on a no-op
	// edit. A genuine edit serializes differently and writes through as curve mode.
	FRichCurve SourceCurve;
	if (!SourceData.IsEmpty()
		&& PCGExRampFormat::Parse(SourceData, SourceCurve)
		&& PCGExRampFormat::SerializeCurve(SourceCurve) == NewData)
	{
		return;
	}

	DataHandle->SetValue(NewData);
	SourceData = NewData;
}

#pragma region FCurveOwnerInterface

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TArray<FRichCurveEditInfoConst> FPCGExRampCustomization::GetCurves() const
{
	TArray<FRichCurveEditInfoConst> Curves;
	Curves.Add(FRichCurveEditInfoConst(&CurveData, FName(TEXT("Ramp"))));
	return Curves;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FPCGExRampCustomization::GetCurves(TAdderReserverRef<FRichCurveEditInfoConst> Curves) const
{
	Curves.Add(FRichCurveEditInfoConst(&CurveData, FName(TEXT("Ramp"))));
}

TArray<FRichCurveEditInfo> FPCGExRampCustomization::GetCurves()
{
	TArray<FRichCurveEditInfo> Curves;
	Curves.Add(FRichCurveEditInfo(&CurveData, FName(TEXT("Ramp"))));
	return Curves;
}

void FPCGExRampCustomization::ModifyOwner()
{
	// No UObject owner / no transaction integration in v1.
}

TArray<const UObject*> FPCGExRampCustomization::GetOwners() const
{
	return TArray<const UObject*>();
}

void FPCGExRampCustomization::MakeTransactional()
{
	// No transaction integration in v1.
}

void FPCGExRampCustomization::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{
	PushToProperty();
}

bool FPCGExRampCustomization::IsValidCurve(FRichCurveEditInfo CurveInfo)
{
	return CurveInfo.CurveToEdit == &CurveData;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
