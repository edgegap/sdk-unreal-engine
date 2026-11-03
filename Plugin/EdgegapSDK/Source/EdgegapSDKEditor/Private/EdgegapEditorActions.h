// Edgegap SDK editor: what the panel's buttons do.

#pragma once

#include "CoreMinimal.h"
#include "Generated/EdgegapAPIModels.h"

class FEdgegapPipeline;

namespace EdgegapEditorActions
{
	/** Checks the API token by listing applications. */
	void VerifyToken();

	/** Package the Linux server, build and push the image, then create the app (if missing) and the version. */
	TSharedRef<FEdgegapPipeline> BuildAndPush(bool bSkipBuild);

	/** Deploy the current version near this machine and wait until it is ready. */
	TSharedRef<FEdgegapPipeline> DeployTestServer();

	void ListDeployments(TFunction<void(const TArray<FEdgegapAPIDeploymentListData>&)> Done);

	void StopDeployment(const FString& RequestId, TFunction<void(bool)> Done);

	/** Copies the API token from your secrets into DefaultGame.ini and turns on "include in packaged builds". */
	void CopyApiTokenToPackagedBuilds();

	/** Image tag in use: the VersionName setting, or the last built one, or a new UTC timestamp. */
	FString CurrentVersion();
}
