// Copyright 2026 Timothe Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "SPCGExRampEditor.h"

#include "PCGExRampEditController.h"
#include "SPCGExRampGraph.h"
#include "SPCGExRampKeyStrip.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPCGExRampEditor"

void SPCGExRampEditor::Construct(const FArguments& InArgs, const TSharedRef<FPCGExRampEditController>& InController)
{
	Controller = InController;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(GraphHeight)
			[
				SNew(SPCGExRampGraph, Controller.ToSharedRef())
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 3.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.HeightOverride(StripHeight)
			[
				SNew(SPCGExRampKeyStrip, Controller.ToSharedRef())
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			BuildInspector()
		]
	];
}

#pragma region Selection helpers

bool SPCGExRampEditor::HasSelection() const
{
	return Controller.IsValid() && Controller->HasSelection();
}

TOptional<float> SPCGExRampEditor::GetSelectedTime() const
{
	if (!HasSelection())
	{
		return TOptional<float>();
	}
	return TOptional<float>(Controller->GetKeyTime(Controller->GetSelectedKey()));
}

TOptional<float> SPCGExRampEditor::GetSelectedValue() const
{
	if (!HasSelection())
	{
		return TOptional<float>();
	}
	return TOptional<float>(Controller->GetKeyValue(Controller->GetSelectedKey()));
}

void SPCGExRampEditor::SetSelectedTime(float NewTime, bool bInteractive)
{
	if (HasSelection())
	{
		const FKeyHandle Sel = Controller->GetSelectedKey();
		Controller->MoveKey(Sel, NewTime, Controller->GetKeyValue(Sel), bInteractive);
	}
}

void SPCGExRampEditor::SetSelectedValue(float NewValue, bool bInteractive)
{
	if (HasSelection())
	{
		Controller->SetKeyValue(Controller->GetSelectedKey(), NewValue, bInteractive);
	}
}

#pragma endregion

#pragma region Interpolation

FText SPCGExRampEditor::GetSelectedInterpText() const
{
	if (!HasSelection())
	{
		return LOCTEXT("InterpNone", "--");
	}

	switch (Controller->GetKeyInterp(Controller->GetSelectedKey()))
	{
	case RCIM_Constant:
		return LOCTEXT("InterpConstant", "Constant");
	case RCIM_Cubic:
		return LOCTEXT("InterpCubic", "Cubic");
	default:
		return LOCTEXT("InterpLinear", "Linear");
	}
}

void SPCGExRampEditor::SetSelectedInterp(ERichCurveInterpMode Mode)
{
	if (HasSelection())
	{
		Controller->SetKeyInterp(Controller->GetSelectedKey(), Mode);
	}
}

TSharedRef<SWidget> SPCGExRampEditor::BuildInterpMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("InterpLinear", "Linear"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SPCGExRampEditor::SetSelectedInterp, RCIM_Linear)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("InterpConstant", "Constant"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SPCGExRampEditor::SetSelectedInterp, RCIM_Constant)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("InterpCubic", "Cubic"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SPCGExRampEditor::SetSelectedInterp, RCIM_Cubic)));

	return MenuBuilder.MakeWidget();
}

#pragma endregion

#pragma region Inspector

TSharedRef<SWidget> SPCGExRampEditor::BuildInspector()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("PosLabel", "Pos"))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(true)
			// No hard min/max: positions are free (incl. negative and > 1). The slider still spans the
			// canonical [0,1] for convenience; typing goes beyond it.
			.MinSliderValue(0.0f).MaxSliderValue(1.0f)
			.Delta(0.01f)
			.IsEnabled_Lambda([this]() { return HasSelection(); })
			.Value_Lambda([this]() { return GetSelectedTime(); })
			.OnValueChanged_Lambda([this](float V) { SetSelectedTime(V, /*bInteractive=*/true); })
			.OnValueCommitted_Lambda([this](float V, ETextCommit::Type) { SetSelectedTime(V, /*bInteractive=*/false); })
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("ValLabel", "Value"))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(true)
			.Delta(0.01f)
			.IsEnabled_Lambda([this]() { return HasSelection(); })
			.Value_Lambda([this]() { return GetSelectedValue(); })
			.OnValueChanged_Lambda([this](float V) { SetSelectedValue(V, /*bInteractive=*/true); })
			.OnValueCommitted_Lambda([this](float V, ETextCommit::Type) { SetSelectedValue(V, /*bInteractive=*/false); })
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("InterpLabel", "Interp"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.IsEnabled_Lambda([this]() { return HasSelection(); })
			.OnGetMenuContent(FOnGetContent::CreateSP(this, &SPCGExRampEditor::BuildInterpMenu))
			.ButtonContent()
			[
				SNew(STextBlock).Text_Lambda([this]() { return GetSelectedInterpText(); })
			]
		];
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
