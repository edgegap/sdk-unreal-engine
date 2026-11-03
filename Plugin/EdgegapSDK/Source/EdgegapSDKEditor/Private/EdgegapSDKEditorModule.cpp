// Edgegap SDK editor: registers Tools > Edgegap.

#include "Modules/ModuleManager.h"
#include "EdgegapBuildSettings.h"
#include "EdgegapEditorActions.h"
#include "EdgegapPipeline.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/Parse.h"
#include "SEdgegapPanel.h"
#include "ToolMenus.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "EdgegapSDKEditor"

static const FName EdgegapTabName(TEXT("EdgegapPanel"));

class FEdgegapSDKEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(EdgegapTabName, FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab).TabRole(ETabRole::NomadTab)[ SNew(SEdgegapPanel) ];
		}))
		.SetDisplayName(LOCTEXT("TabTitle", "Edgegap"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Build, push and deploy your dedicated server on Edgegap."));

		RegisterConsoleCommands();

		MenuCallback = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped Owner(EdgegapTabName);
			UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
			// FindOrAddSection(Name, Label) does not exist before UE 5.1.
			FToolMenuSection* Existing = Menu->FindSection("Edgegap");
			FToolMenuSection& Section = Existing ? *Existing : Menu->AddSection("Edgegap", LOCTEXT("Section", "Edgegap"));
			Section.AddMenuEntry("OpenEdgegap", LOCTEXT("MenuEntry", "Edgegap"), LOCTEXT("MenuTooltip", "Build, push and deploy on Edgegap."),
				FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(EdgegapTabName); })));
		}));
	}

	virtual void ShutdownModule() override
	{
		for (IConsoleObject* Command : Commands) { IConsoleManager::Get().UnregisterConsoleObject(Command); }
		Commands.Reset();
		if (UObjectInitialized())
		{
			UToolMenus::UnRegisterStartupCallback(MenuCallback);
			UToolMenus::UnregisterOwner(EdgegapTabName);
		}
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(EdgegapTabName);
		}
	}

private:
	/**
	 * The panel's actions as console commands, for CI and headless use:
	 *   UnrealEditor-Cmd <project> -ExecCmds="Edgegap.BuildAndPush" -EdgegapQuitWhenDone -unattended -nullrhi
	 * App settings come from DefaultEditor.ini ([/Script/EdgegapSDKEditor.EdgegapBuildSettings]);
	 * -EdgegapApp= / -EdgegapVersion= override the app and version.
	 */
	void RegisterConsoleCommands()
	{
		UEdgegapBuildSettings* Settings = GetMutableDefault<UEdgegapBuildSettings>();
		FParse::Value(FCommandLine::Get(), TEXT("EdgegapApp="), Settings->AppName);
		FParse::Value(FCommandLine::Get(), TEXT("EdgegapVersion="), Settings->VersionName);

		auto Add = [this](const TCHAR* Name, const TCHAR* Help, TFunction<void(const TArray<FString>&)> Run)
		{
			Commands.Add(IConsoleManager::Get().RegisterConsoleCommand(Name, Help,
				FConsoleCommandWithArgsDelegate::CreateLambda([Run](const TArray<FString>& Args) { Run(Args); }), ECVF_Default));
		};
		Add(TEXT("Edgegap.VerifyToken"), TEXT("List applications with the API token."), [](const TArray<FString>&)
		{
			EdgegapEditorActions::VerifyToken();
			QuitLater(5.0f);
		});
		Add(TEXT("Edgegap.BuildAndPush"), TEXT("Edgegap.BuildAndPush [VersionOnly]"), [](const TArray<FString>& Args)
		{
			RunPipeline(EdgegapEditorActions::BuildAndPush(Args.Num() > 0 && Args[0].Equals(TEXT("VersionOnly"), ESearchCase::IgnoreCase)));
		});
		Add(TEXT("Edgegap.DeployTest"), TEXT("Deploy the current version near this machine."), [](const TArray<FString>&)
		{
			RunPipeline(EdgegapEditorActions::DeployTestServer());
		});
		Add(TEXT("Edgegap.ListDeployments"), TEXT("Log running deployments."), [](const TArray<FString>&)
		{
			EdgegapEditorActions::ListDeployments([](const TArray<FEdgegapAPIDeploymentListData>& Data)
			{
				for (const FEdgegapAPIDeploymentListData& D : Data)
				{
					FEdgegapPipeline::Log(FString::Printf(TEXT("  %s %s %s [%s]"), *D.RequestId, D.bReady ? TEXT("ready") : TEXT("starting"), *D.PublicIp, *FString::Join(D.Tags, TEXT(","))));
				}
				QuitLater(0.5f);
			});
		});
		Add(TEXT("Edgegap.StopDeployment"), TEXT("Edgegap.StopDeployment <request id>"), [](const TArray<FString>& Args)
		{
			if (Args.Num() < 1) { FEdgegapPipeline::Log(TEXT("Usage: Edgegap.StopDeployment <request id>"), true); return; }
			EdgegapEditorActions::StopDeployment(Args[0], [](bool) { QuitLater(0.5f); });
		});
	}

	static void RunPipeline(TSharedRef<FEdgegapPipeline> Pipeline)
	{
		Pipeline->Run([Pipeline](bool bOk)
		{
			if (FParse::Param(FCommandLine::Get(), TEXT("EdgegapQuitWhenDone")))
			{
				FEdgegapPipeline::Log(bOk ? TEXT("EDGEGAP PIPELINE OK") : TEXT("EDGEGAP PIPELINE FAILED"), !bOk);
				QuitLater(0.5f);
			}
		});
	}

	static void QuitLater(float Seconds)
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("EdgegapQuitWhenDone"))) { return; }
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { RequestEngineExit(TEXT("EdgegapQuitWhenDone")); return false; }), Seconds);
	}

	FDelegateHandle MenuCallback;
	TArray<IConsoleObject*> Commands;
};

IMPLEMENT_MODULE(FEdgegapSDKEditorModule, EdgegapSDKEditor)

#undef LOCTEXT_NAMESPACE
