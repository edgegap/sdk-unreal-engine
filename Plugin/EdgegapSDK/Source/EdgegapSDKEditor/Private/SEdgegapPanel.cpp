// Edgegap SDK editor: Tools > Edgegap panel.

#include "SEdgegapPanel.h"
#include "EdgegapBuildSettings.h"
#include "EdgegapEditorActions.h"
#include "EdgegapPipeline.h"
#include "EdgegapSettings.h"
#include "IDetailsView.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "EdgegapPanel"

namespace
{
	TSharedRef<SWidget> Button(const FText& Label, const FText& Tooltip, FOnClicked OnClicked, TAttribute<bool> Enabled)
	{
		return SNew(SButton)
			.Text(Label)
			.ToolTipText(Tooltip)
			.OnClicked(OnClicked)
			.IsEnabled(Enabled);
	}

	TSharedRef<SWidget> Heading(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11));
	}
}

void SEdgegapPanel::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bAllowSearch = false;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	TSharedRef<IDetailsView> Details = PropertyEditor.CreateDetailView(Args);
	Details->SetObject(GetMutableDefault<UEdgegapBuildSettings>());
	Details->OnFinishedChangingProperties().AddLambda([](const FPropertyChangedEvent&)
	{
		GetMutableDefault<UEdgegapBuildSettings>()->TryUpdateDefaultConfigFile();
	});

	LogHandle = FEdgegapPipeline::OnLogLine.AddSP(this, &SEdgegapPanel::AppendLog);
	const TAttribute<bool> Idle = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SEdgegapPanel::IsIdle));

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(8)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Intro",
					"Secrets (API token, registry login) are per developer: Project Settings > Plugins > Edgegap (my secrets). "
					"Matchmaker and server browser URLs: Project Settings > Plugins > Edgegap."))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					Button(LOCTEXT("Verify", "Verify API Token"), LOCTEXT("VerifyTip", "Lists your applications with the API token."),
						FOnClicked::CreateLambda([]() { EdgegapEditorActions::VerifyToken(); return FReply::Handled(); }), true)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					Button(LOCTEXT("Secrets", "Open Settings"), LOCTEXT("SecretsTip", "Project Settings > Plugins > Edgegap."),
						FOnClicked::CreateLambda([]()
						{
							if (ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
							{
								Settings->ShowViewer("Project", "Plugins", "EdgegapSettings");
							}
							return FReply::Handled();
						}), true)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					Button(LOCTEXT("CopyToken", "Copy API Token To Packaged Builds..."),
						LOCTEXT("CopyTokenTip", "Puts the org API token in DefaultGame.ini so packaged builds can call the Edgegap API. Anyone with the build gets full control of your organization: internal builds only."),
						FOnClicked::CreateLambda([]() { EdgegapEditorActions::CopyApiTokenToPackagedBuilds(); return FReply::Handled(); }), true)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 4) [ Heading(LOCTEXT("BuildHeading", "Build, push and deploy")) ]
			+ SVerticalBox::Slot().AutoHeight() [ Details ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					Button(LOCTEXT("BuildPush", "Build and Push"), LOCTEXT("BuildPushTip", "Package the Linux server, build and push the image, create the app (if needed) and the version."),
						FOnClicked::CreateLambda([this]() { return Run(EdgegapEditorActions::BuildAndPush(false)); }), Idle)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					Button(LOCTEXT("VersionOnly", "Create Version Only"), LOCTEXT("VersionOnlyTip", "Register an image you already pushed (tag = Version Name)."),
						FOnClicked::CreateLambda([this]() { return Run(EdgegapEditorActions::BuildAndPush(true)); }), Idle)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					Button(LOCTEXT("Deploy", "Deploy Test Server"), LOCTEXT("DeployTip", "Deploy the version near this machine and print the address to open."),
						FOnClicked::CreateLambda([this]() { return Run(EdgegapEditorActions::DeployTestServer()); }), Idle)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("Cancel", "Cancel"))
						.IsEnabled_Lambda([this]() { return !IsIdle(); })
						.OnClicked_Lambda([this]() { if (Running.IsValid()) { Running->Cancel(); } return FReply::Handled(); })
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center) [ Heading(LOCTEXT("DeploymentsHeading", "Running deployments")) ]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("Refresh", "Refresh")).OnClicked(this, &SEdgegapPanel::RefreshDeployments)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().MaxHeight(220)
			[
				SAssignNew(DeploymentList, SListView<FRow>)
					.ListItemsSource(&Deployments)
					.OnGenerateRow(this, &SEdgegapPanel::MakeRow)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 4) [ Heading(LOCTEXT("LogHeading", "Log")) ]
			+ SVerticalBox::Slot().AutoHeight().MaxHeight(320)
			[
				SAssignNew(LogBox, SMultiLineEditableTextBox).IsReadOnly(true).AutoWrapText(true)
			]
		]
	];
}

SEdgegapPanel::~SEdgegapPanel()
{
	FEdgegapPipeline::OnLogLine.Remove(LogHandle);
}

bool SEdgegapPanel::IsIdle() const
{
	return !Running.IsValid() || !Running->IsRunning();
}

FReply SEdgegapPanel::Run(TSharedRef<FEdgegapPipeline> Pipeline)
{
	Running = Pipeline;
	TWeakPtr<SEdgegapPanel> WeakThis = SharedThis(this);
	Pipeline->Run([WeakThis](bool)
	{
		if (TSharedPtr<SEdgegapPanel> Self = WeakThis.Pin()) { Self->RefreshDeployments(); }
	});
	return FReply::Handled();
}

FReply SEdgegapPanel::RefreshDeployments()
{
	TWeakPtr<SEdgegapPanel> WeakThis = SharedThis(this);
	EdgegapEditorActions::ListDeployments([WeakThis](const TArray<FEdgegapAPIDeploymentListData>& Data)
	{
		TSharedPtr<SEdgegapPanel> Self = WeakThis.Pin();
		if (!Self) { return; }
		Self->Deployments.Reset();
		for (const FEdgegapAPIDeploymentListData& D : Data) { Self->Deployments.Add(MakeShared<FEdgegapAPIDeploymentListData>(D)); }
		Self->DeploymentList->RequestListRefresh();
	});
	return FReply::Handled();
}

TSharedRef<ITableRow> SEdgegapPanel::MakeRow(FRow Row, const TSharedRef<STableViewBase>& Owner)
{
	TArray<FString> Ports;
	for (const TPair<FString, FEdgegapAPIPortMapping>& Pair : Row->Ports)
	{
		Ports.Add(FString::Printf(TEXT("%s %s:%d"), *Pair.Key, *Row->PublicIp, Pair.Value.External));
	}
	const FString Label = FString::Printf(TEXT("%s  %s  %s  %s"), *Row->RequestId, Row->bReady ? TEXT("ready") : TEXT("starting"),
		*FString::Join(Ports, TEXT(", ")), *FString::Join(Row->Tags, TEXT(",")));
	const FString RequestId = Row->RequestId;
	TWeakPtr<SEdgegapPanel> WeakThis = SharedThis(this);

	return SNew(STableRow<FRow>, Owner)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(FText::FromString(Label)) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).Text(LOCTEXT("Stop", "Stop")).OnClicked_Lambda([RequestId, WeakThis]()
			{
				EdgegapEditorActions::StopDeployment(RequestId, [WeakThis](bool)
				{
					if (TSharedPtr<SEdgegapPanel> Self = WeakThis.Pin()) { Self->RefreshDeployments(); }
				});
				return FReply::Handled();
			})
		]
	];
}

void SEdgegapPanel::AppendLog(const FString& Line)
{
	LogText += Line + TEXT("\n");
	if (LogText.Len() > 60000) { LogText.RightChopInline(LogText.Len() - 50000); }
	if (LogBox.IsValid())
	{
		LogBox->SetText(FText::FromString(LogText));
		LogBox->ScrollTo(FTextLocation(TNumericLimits<int32>::Max()));
	}
}

#undef LOCTEXT_NAMESPACE
