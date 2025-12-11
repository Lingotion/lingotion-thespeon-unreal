// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Misc/Paths.h"

namespace Thespeon
{
    namespace Core
    {
        namespace IO
        {
            /// @brief Class for loading files at runtime. Keeps all hard coded file paths
            class LINGOTIONTHESPEON_API RuntimeFileLoader
            {
            public:
                static const FString& GetPluginDir();
                static const FString& GetRuntimeFileDir();
                static const FString& GetManifestPath();

                static constexpr const TCHAR* ManifestName  = TEXT("PackManifest.json");

                // Separate content dir for actual assets, should work both in runtime and in editor
                static const FString& GetRuntimeModelDir();

                static FString GetRuntimeFilePath(const FString& FileName);
                static FString GetRuntimeModelPath(const FString& ModelName);

                static FString LoadFileAsString(const FString& FilePath); // Let this be a wrapper for FFileHelper::LoadFileToString with failure checking.
                static TUniquePtr<IFileHandle> LoadFileAsStream(const FString& FilePath); // loads file from disk and returns as stream. Exception if not found.
            };
        }
    }
}
