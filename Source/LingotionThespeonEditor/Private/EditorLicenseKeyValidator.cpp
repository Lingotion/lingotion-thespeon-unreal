// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "EditorLicenseKeyValidator.h"
#include "HttpModule.h"
#include "EditorThespeonSettings.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Core/LingotionLogger.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Core/PackManifestHandler.h"
#include "Misc/ConfigCacheIni.h"

void FEditorLicenseKeyValidator::ValidateLicenseAsync(
    FOnLicenseValidationResult ResultCallback)
{
    const auto* Settings = GetDefault<UEditorThespeonSettings>();
    FString LicenseKey = Settings->GetLicenseKey();
    if (LicenseKey.IsEmpty())
    {
        ResultCallback.ExecuteIfBound(false);
        return;
    }

    FString JsonPayload;
    BuildJsonPayload(LicenseKey, JsonPayload);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("https://portal.lingotion.com/v1/licenses/verify"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(JsonPayload);

    Request->OnProcessRequestComplete().BindLambda(
        [ResultCallback](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess)
        {
            bool bIsValid = false;

            if (bSuccess && Resp.IsValid() && Resp->GetResponseCode() == 200)
            {
                bIsValid = true;
            }

            ResultCallback.ExecuteIfBound(bIsValid);
        }
    );

    Request->ProcessRequest();
}

void FEditorLicenseKeyValidator::BuildJsonPayload(const FString& LicenseKey, FString& OutJson)
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

    if (UPackManifestHandler* ManifestHandler = UPackManifestHandler::Get())
    {
        // Actor modules
        for (const FActorModuleInfo& Module : ManifestHandler->GetAllActorModules())
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

    // Data object
    TSharedPtr<FJsonObject> DataObject = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> ModulesArray;

    for (const FString& ModuleID : ModuleIDs)
    {
        ModulesArray.Add(MakeShared<FJsonValueString>(ModuleID));
    }

    DataObject->SetArrayField(TEXT("Modules"), ModulesArray);
    PayloadObject->SetObjectField(TEXT("data"), DataObject);

    // Convert to string
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
    FJsonSerializer::Serialize(PayloadObject.ToSharedRef(), Writer);
}

