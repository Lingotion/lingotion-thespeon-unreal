// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "EditorLicenseKeyValidator.h"
#include "HttpModule.h"
#include "EditorThespeonSettings.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Core/LingotionLogger.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Core/ManifestHandler.h"
#include "Misc/ConfigCacheIni.h"
#include "EditorDataCache.h"
#include "Interfaces/IPluginManager.h"

bool FEditorLicenseKeyValidator::bVerifyInFlight = false;

void FEditorLicenseKeyValidator::ValidateLicenseAsync(FOnLicenseValidationResult ResultCallback)
{
	const auto* Settings = GetDefault<UEditorThespeonSettings>();
	FString LicenseKey = Settings->GetLicenseKey();
	if (LicenseKey.IsEmpty())
	{
		ResultCallback.ExecuteIfBound(false);
		return;
	}

	// Single-in-flight guard
	if (bVerifyInFlight)
	{
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("License verification already in flight; skipping."));
		ResultCallback.ExecuteIfBound(Settings->ValidationState == ELicenseValidationState::Valid);
		return;
	}
	bVerifyInFlight = true;

	// Snapshot the data cache now; the same snapshot is embedded in the payload and subtracted
	// back out on HTTP 200, so synths landing during the round-trip are preserved.
	FEditorDataCache::FSynthData DataSnapshot = FEditorDataCache::Snapshot();

	FString JsonPayload;
	BuildJsonPayload(LicenseKey, DataSnapshot, JsonPayload);
	if (JsonPayload.IsEmpty())
	{
		bVerifyInFlight = false;
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to build JSON payload for license verification."));
		ResultCallback.ExecuteIfBound(false);
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://portal.lingotion.com/v1/licenses/verify"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(JsonPayload);

	Request->OnProcessRequestComplete().BindLambda(
	    [ResultCallback, DataSnapshot = MoveTemp(DataSnapshot)](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess)
	    {
		    bVerifyInFlight = false;
		    bool bIsValid = false;

		    if (bSuccess && Resp.IsValid() && Resp->GetResponseCode() == 200)
		    {
			    bIsValid = true;
			    FEditorDataCache::SubtractAndPersist(DataSnapshot);
		    }

		    ResultCallback.ExecuteIfBound(bIsValid);
	    }
	);

	if (!Request->ProcessRequest())
	{
		bVerifyInFlight = false;
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Failed to dispatch license verification request."));
		ResultCallback.ExecuteIfBound(false);
	}
}

void FEditorLicenseKeyValidator::BuildJsonPayload(const FString& LicenseKey, const FEditorDataCache::FSynthData& DataSnapshot, FString& OutJson)
{

	// Get project GUID from DefaultGame.ini
	FString ProjectGuid;
	if (!(GConfig && GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectID"), ProjectGuid, GGameIni)))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get ProjectID from DefaultGame.ini"));
		return;
	}

	// Fetch module IDs from manifest
	TArray<FString> ModuleIDs;

	if (UManifestHandler* ManifestHandler = UManifestHandler::Get())
	{
		// Character modules
		for (const FCharacterModuleInfo& Module : ManifestHandler->GetAllCharacterModules())
		{
			ModuleIDs.AddUnique(Module.ModuleID);
		}

		// Language modules
		for (const FLanguageModuleInfo& Module : ManifestHandler->GetAllLanguageModules())
		{
			ModuleIDs.AddUnique(Module.ModuleID);
		}
	}

	// JSON root
	TSharedPtr<FJsonObject> PayloadObject = MakeShared<FJsonObject>();
	PayloadObject->SetStringField(TEXT("licenseKey"), LicenseKey);
	PayloadObject->SetStringField(TEXT("projectGuid"), ProjectGuid);
	PayloadObject->SetStringField(TEXT("platform"), TEXT("UNREAL"));

	// Data object
	TSharedPtr<FJsonObject> DataObject = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ModulesArray;

	for (const FString& ModuleID : ModuleIDs)
	{
		ModulesArray.Add(MakeShared<FJsonValueString>(ModuleID));
	}

	DataObject->SetArrayField(TEXT("Modules"), ModulesArray);

	DataObject->SetObjectField(TEXT("cacheData"), FEditorDataCache::ToJsonObject(DataSnapshot));

	FString PackageVersion;
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("LingotionThespeon")))
	{
		PackageVersion = Plugin->GetDescriptor().VersionName;
	}
	DataObject->SetStringField(TEXT("packageVersion"), PackageVersion);

	PayloadObject->SetObjectField(TEXT("data"), DataObject);

	// Convert to string
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	FJsonSerializer::Serialize(PayloadObject.ToSharedRef(), Writer);
}
