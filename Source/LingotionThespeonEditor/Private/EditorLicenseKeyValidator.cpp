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
bool FEditorLicenseKeyValidator::bRevalidatePending = false;
TArray<FEditorLicenseKeyValidator::FOnLicenseValidationResult> FEditorLicenseKeyValidator::PendingCallbacks;

void FEditorLicenseKeyValidator::ValidateLicenseAsync(FOnLicenseValidationResult ResultCallback)
{
	// Single-in-flight guard: the key may have changed since the in-flight request was sent,
	// so queue a fresh verification instead of answering from the current state.
	if (bVerifyInFlight)
	{
		LINGO_LOG(EVerbosityLevel::Debug, TEXT("License verification already in flight; queuing another once it completes."));
		bRevalidatePending = true;
		if (ResultCallback.IsBound())
		{
			PendingCallbacks.Add(MoveTemp(ResultCallback));
		}
		return;
	}

	FString LicenseKey = UEditorThespeonSettings::GetLicenseKey();
	if (LicenseKey.IsEmpty())
	{
		ResultCallback.ExecuteIfBound(ELicenseCheckResult::Inconclusive);
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
		ResultCallback.ExecuteIfBound(ELicenseCheckResult::Inconclusive);
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://portal.lingotion.com/v1/licenses/verify"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(JsonPayload);

	Request->OnProcessRequestComplete().BindLambda(
	    [ResultCallback, SentKey = LicenseKey, DataSnapshot = MoveTemp(DataSnapshot)](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess)
	    {
		    bVerifyInFlight = false;

		    // Only an actual 200 validates and only an actual 400/401/403 rejects. Anything else
		    // (offline, timeout, server errors, rate limiting, proxies) leaves the state untouched.
		    ELicenseCheckResult Result = ELicenseCheckResult::Inconclusive;
		    if (bSuccess && Resp.IsValid())
		    {
			    const int32 ResponseCode = Resp->GetResponseCode();
			    if (ResponseCode == 200)
			    {
				    Result = ELicenseCheckResult::Valid;
				    FEditorDataCache::SubtractAndPersist(DataSnapshot);
			    }
			    else if (ResponseCode == 400 || ResponseCode == 401 || ResponseCode == 403)
			    {
				    Result = ELicenseCheckResult::Rejected;
			    }
			    else
			    {
				    LINGO_LOG(
				        EVerbosityLevel::Info,
				        TEXT("License verification returned unexpected status %d; keeping current license state."),
				        ResponseCode
				    );
			    }
		    }
		    else
		    {
			    LINGO_LOG(EVerbosityLevel::Info, TEXT("License verification could not reach the server; keeping current license state."));
		    }

		    // The key was edited while this request was in flight; its answer is for the old key.
		    if (SentKey != UEditorThespeonSettings::GetLicenseKey())
		    {
			    Result = ELicenseCheckResult::Inconclusive;
		    }

		    ApplyResult(Result);
		    ResultCallback.ExecuteIfBound(Result);

		    if (bRevalidatePending)
		    {
			    bRevalidatePending = false;
			    ValidateLicenseAsync(FOnLicenseValidationResult::CreateLambda(
			        [Callbacks = MoveTemp(PendingCallbacks)](ELicenseCheckResult PendingResult)
			        {
				        for (const FOnLicenseValidationResult& Callback : Callbacks)
				        {
					        Callback.ExecuteIfBound(PendingResult);
				        }
			        }
			    ));
			    PendingCallbacks.Reset();
		    }
	    }
	);

	if (!Request->ProcessRequest())
	{
		// Unbind so a failure-path completion can't report a second time.
		Request->OnProcessRequestComplete().Unbind();
		bVerifyInFlight = false;
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Failed to dispatch license verification request."));
		ResultCallback.ExecuteIfBound(ELicenseCheckResult::Inconclusive);
	}
}

void FEditorLicenseKeyValidator::ApplyResult(ELicenseCheckResult Result)
{
	if (Result == ELicenseCheckResult::Inconclusive)
	{
		return;
	}

	UEditorThespeonSettings* Settings = UEditorThespeonSettings::Get();
	const ELicenseValidationState NewState = Result == ELicenseCheckResult::Valid ? ELicenseValidationState::Valid : ELicenseValidationState::Invalid;
	if (Settings->ValidationState != NewState)
	{
		Settings->ValidationState = NewState;
		Settings->SaveConfig(CPF_Config, *Settings->GetDefaultConfigFilename());
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
