#include "SubsystemUtils.h"
#include "Inference/InferenceWorkloadManager.h"
#include "Language/LookupTableManager.h"

UInferenceWorkloadManager* FLingotionThespeonSubsystemUtils::GetInferenceWorkloadManager()
{
	return GetGameInstanceSubsystem<UInferenceWorkloadManager>();
}

ULookupTableManager* FLingotionThespeonSubsystemUtils::GetLookupTableManager()
{
	return GetGameInstanceSubsystem<ULookupTableManager>();
}

UModuleManager* FLingotionThespeonSubsystemUtils::GetModuleManager()
{
	return GetGameInstanceSubsystem<UModuleManager>();
}

bool FLingotionThespeonSubsystemUtils::GetSubsystems(
    UInferenceWorkloadManager*& OutInferenceWorkloadManager, ULookupTableManager*& OutLookupTableManager
)
{
	OutInferenceWorkloadManager = GetInferenceWorkloadManager();
	OutLookupTableManager = GetLookupTableManager();

	return (OutInferenceWorkloadManager != nullptr && OutLookupTableManager != nullptr);
}