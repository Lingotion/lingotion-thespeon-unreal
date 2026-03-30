// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "Core/IO/RuntimeFileLoader.h"
#include "Core/LingotionLogger.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Interfaces/IPluginManager.h"

using namespace Thespeon::Core::IO;

const FString& RuntimeFileLoader::GetPluginDir()
{
	static FString Dir;
	static bool bInitialized = false;
	if (!bInitialized)
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("LingotionThespeon"));
		if (Plugin.IsValid())
		{
			Dir = Plugin->GetBaseDir();
		}
		else
		{
			LINGO_LOG(
			    EVerbosityLevel::Error, TEXT("Could not find LingotionThespeon plugin. Make sure the plugin is correctly installed and enabled.")
			);
			Dir = TEXT("");
		}
		bInitialized = true;
	}
	return Dir;
}

const FString& RuntimeFileLoader::GetRuntimeFileDir()
{
	static FString Dir = FPaths::Combine(GetPluginDir(), TEXT("RuntimeData"));
	return Dir;
}

const FString& RuntimeFileLoader::GetManifestPath()
{
	static FString Dir = FPaths::Combine(GetRuntimeFileDir(), RuntimeFileLoader::ManifestName);
	return Dir;
}

const FString& RuntimeFileLoader::GetRuntimeModelDir()
{
	static FString Dir = TEXT("/Game/LingotionThespeon/RuntimeData/");
	return Dir;
}

FString RuntimeFileLoader::GetRuntimeFilePath(const FString& FileName)
{
	return FPaths::Combine(GetRuntimeFileDir(), FileName);
}

FString RuntimeFileLoader::GetRuntimeModelPath(const FString& ModelName)
{
	return FPaths::Combine(GetRuntimeModelDir(), ModelName);
}

FString RuntimeFileLoader::LoadFileAsString(const FString& FilePath)
{
	if (FilePath.IsEmpty())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Could not load file: %s. Empty file path provided"), *FilePath);
		return FString();
	}

	FString Result;
	if (!FFileHelper::LoadFileToString(Result, *FilePath))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to load file as string: %s"), *FilePath);
		return FString();
	}

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Successfully loaded file as string: %s"), *FilePath);
	return Result;
}

TUniquePtr<IFileHandle> RuntimeFileLoader::LoadFileAsStream(const FString& FilePath)
{
	if (FilePath.IsEmpty())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Could not load file: %s. Empty file path provided"), *FilePath);
		return nullptr;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.FileExists(*FilePath))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("File does not exist: %s"), *FilePath);
		return nullptr;
	}

	TUniquePtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*FilePath));
	if (!FileHandle.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to open file for reading: %s"), *FilePath);
		return nullptr;
	}

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Successfully opened file stream: %s"), *FilePath);
	return FileHandle;
}