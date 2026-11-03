// Edgegap SDK editor: Tools > Edgegap panel.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Generated/EdgegapAPIModels.h"

class FEdgegapPipeline;
class SMultiLineEditableTextBox;

class SEdgegapPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEdgegapPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SEdgegapPanel() override;

private:
	using FRow = TSharedPtr<FEdgegapAPIDeploymentListData>;

	FReply Run(TSharedRef<FEdgegapPipeline> Pipeline);
	FReply RefreshDeployments();
	TSharedRef<class ITableRow> MakeRow(FRow Row, const TSharedRef<class STableViewBase>& Owner);
	void AppendLog(const FString& Line);
	bool IsIdle() const;

	TSharedPtr<FEdgegapPipeline> Running;
	TArray<FRow> Deployments;
	TSharedPtr<SListView<FRow>> DeploymentList;
	TSharedPtr<SMultiLineEditableTextBox> LogBox;
	FString LogText;
	FDelegateHandle LogHandle;
};
