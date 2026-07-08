// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExRampCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "SCurveEditor.h"
#include "Widgets/Layout/SBox.h"

#include "PCGExRampFormat.h"
#include "PCGExRampTypes.h"
#include "PCGExRampsEditorSettings.h"
#include "Widgets/PCGExRampEditController.h"
#include "Widgets/SPCGExRampEditor.h"

#define LOCTEXT_NAMESPACE "PCGExRampCustomization"

TSharedRef<IPropertyTypeCustomization> FPCGExRampCustomization::MakeInstance()
{
	return MakeShared<FPCGExRampCustomization>();
}

FPCGExRampCustomization::~FPCGExRampCustomization()
{
	if (SettingsChangedHandle.IsValid() && UObjectInitialized())
	{
		GetMutableDefault<UPCGExRampsEditorSettings>()->OnSettingChanged().Remove(SettingsChangedHandle);
	}
}

void FPCGExRampCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	DataHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExRamp, Data));
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	PullFromProperty();

	// Rebuild this panel if the editor-choice setting is toggled while it is open.
	SettingsChangedHandle = GetMutableDefault<UPCGExRampsEditorSettings>()->OnSettingChanged().AddSP(
		this, &FPCGExRampCustomization::HandleSettingsChanged);

	// The editor is a full-width child row (WholeRowContent, built in CustomizeChildren). ShouldAutoExpand
	// forces the struct open so the editor is always visible without a manual expand, while still getting
	// the full row width a struct header's content-sized value column can't provide.
	HeaderRow
		.ShouldAutoExpand(true)
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		];
}

void FPCGExRampCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Full-width inline editor as the first child row. WholeRowContent gives the widget the entire row
	// width, so the editor spans and adapts to the panel (no content-sized value column).
	const bool bUseLegacy = UPCGExRampsEditorSettings::Get()->bUseLegacyCurveEditor;
	const TSharedRef<SWidget> ValueWidget = bUseLegacy ? BuildLegacyEditor() : BuildRampEditor();

	ChildBuilder.AddCustomRow(LOCTEXT("RampEditorRow", "Ramp Editor"))
		.WholeRowContent()
		[
			ValueWidget
		];

	// Expose the raw string for inspection / copy-paste (editing it here doesn't live-refresh the
	// curve widget until the panel rebuilds).
	if (DataHandle.IsValid())
	{
		ChildBuilder.AddProperty(DataHandle.ToSharedRef());
	}
}

TSharedRef<SWidget> FPCGExRampCustomization::BuildRampEditor()
{
	Controller = MakeShared<FPCGExRampEditController>(CurveData.ToSharedRef());
	Controller->OnChanged.AddSP(this, &FPCGExRampCustomization::HandleRampChanged);
	return SNew(SPCGExRampEditor, Controller.ToSharedRef());
}

TSharedRef<SWidget> FPCGExRampCustomization::BuildLegacyEditor()
{
	// To swap to the modern Curve Editor panel (FCurveEditor / SCurveEditorPanel), this is the place.
	TSharedRef<SCurveEditor> Editor = SNew(SCurveEditor)
		.ViewMinInput(0.0f)
		.ViewMaxInput(1.0f)
		.ViewMinOutput(0.0f)
		.ViewMaxOutput(1.0f)
		.HideUI(false)
		.DesiredSize(FVector2D(320.0f, 140.0f));

	CurveWidget = Editor;
	Editor->SetCurveOwner(this);

	return SNew(SBox)
		.HeightOverride(140.0f)
		[
			Editor
		];
}

void FPCGExRampCustomization::HandleRampChanged(bool bInteractive)
{
	PushToProperty(bInteractive);
}

void FPCGExRampCustomization::HandleSettingsChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	// ForceRefresh() rebuilds the detail layout and destroys this customization synchronously. If an
	// interactive edit is still open, finalize it first (while DataHandle is still valid) so the property
	// transaction closes -- otherwise it strands, breaking undo and leaving PCG mid-regeneration.
	if (bInteractiveEditActive)
	{
		PushToProperty(/*bInteractive=*/false);
	}

	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->ForceRefresh();
	}
}

void FPCGExRampCustomization::PullFromProperty()
{
	FString Data;
	if (DataHandle.IsValid()
		&& DataHandle->GetValue(Data) == FPropertyAccess::Success
		&& PCGExRampFormat::Parse(Data, *CurveData)
		&& CurveData->Keys.Num() >= 1)
	{
		// Positions and values are both free (the editor auto-frames both axes), so keys are kept verbatim.
		// The only unusable payload is a non-finite one (NaN / Inf) -- a finite value, however large, is a
		// valid ramp and must not be silently replaced with the default.
		bool bSane = true;
		for (const FRichCurveKey& Key : CurveData->Keys)
		{
			if (!FMath::IsFinite(Key.Time) || !FMath::IsFinite(Key.Value))
			{
				bSane = false;
			}
		}

		if (bSane)
		{
			CurveData->AutoSetTangents();
			SourceData = Data;
			return;
		}
	}

	// Empty / unreadable / malformed / corrupt payload: fall back to the default linear 0->1 ramp so the
	// editor always shows a valid ramp instead of a blank graph. SourceData stays empty so the first real
	// edit writes the corrected payload through.
	CurveData->Reset();
	const FKeyHandle First = CurveData->AddKey(0.0f, 0.0f);
	const FKeyHandle Last = CurveData->AddKey(1.0f, 1.0f);
	CurveData->GetKey(First).InterpMode = RCIM_Linear;
	CurveData->GetKey(Last).InterpMode = RCIM_Linear;
	CurveData->AutoSetTangents();
	SourceData.Reset();
}

void FPCGExRampCustomization::PushToProperty(bool bInteractive)
{
	if (!DataHandle.IsValid())
	{
		return;
	}

	const FString NewData = PCGExRampFormat::SerializeCurve(*CurveData);

	// The non-interactive push that ends a drag MUST write, even if NewData is unchanged: it is the
	// finalizing SetValue that closes the property transaction the first interactive push opened (the
	// engine begins one on the first InteractiveChange and only ends it on the finalizing call). Skipping
	// it leaves the transaction open forever -- undo stops working and the PCG component never settles out
	// of interactive regeneration, so generation appears to hang.
	const bool bFinalizingInteractive = !bInteractive && bInteractiveEditActive;

	if (!bFinalizingInteractive)
	{
		// No-op guard: while the working curve still matches what we loaded, keep the original string
		// verbatim -- avoids formatting churn ("1" -> "1.0") and silently rewriting a LUT source into
		// curve mode on a no-op edit. A genuine edit serializes differently and writes through as curve mode.
		FRichCurve SourceCurve;
		if (!SourceData.IsEmpty()
			&& PCGExRampFormat::Parse(SourceData, SourceCurve)
			&& PCGExRampFormat::SerializeCurve(SourceCurve) == NewData)
		{
			return;
		}
	}

	DataHandle->SetValue(NewData, bInteractive ? EPropertyValueSetFlags::InteractiveChange : EPropertyValueSetFlags::DefaultFlags);
	SourceData = NewData;
	bInteractiveEditActive = bInteractive;
}

#pragma region FCurveOwnerInterface

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TArray<FRichCurveEditInfoConst> FPCGExRampCustomization::GetCurves() const
{
	TArray<FRichCurveEditInfoConst> Curves;
	Curves.Add(FRichCurveEditInfoConst(CurveData.Get(), FName(TEXT("Ramp"))));
	return Curves;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FPCGExRampCustomization::GetCurves(TAdderReserverRef<FRichCurveEditInfoConst> Curves) const
{
	Curves.Add(FRichCurveEditInfoConst(CurveData.Get(), FName(TEXT("Ramp"))));
}

TArray<FRichCurveEditInfo> FPCGExRampCustomization::GetCurves()
{
	TArray<FRichCurveEditInfo> Curves;
	Curves.Add(FRichCurveEditInfo(CurveData.Get(), FName(TEXT("Ramp"))));
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
	return CurveInfo.CurveToEdit == CurveData.Get();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
