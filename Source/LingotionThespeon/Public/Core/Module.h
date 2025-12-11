// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "NNEModelData.h"

namespace Thespeon
{
    namespace Core
    {
        struct FVersion
        {
            int Major = 0;
            int Minor = 0;
            int Patch = 0;

            FVersion() = default;
            FVersion(int InMajor, int InMinor, int InPatch)
                : Major(InMajor), Minor(InMinor), Patch(InPatch) {}

            inline FString ToString(bool bPrefixV = true) const
            {
                return FString::Printf(TEXT("%s%d.%d.%d"),
                    bPrefixV ? TEXT("v") : TEXT(""), Major, Minor, Patch);
            }

            inline bool operator==(const FVersion& Other) const { return Major==Other.Major && Minor==Other.Minor && Patch==Other.Patch; }
            inline bool operator!=(const FVersion& Other) const { return !(*this == Other); }
            inline bool operator<(const FVersion& Other) const
            {
                if (Major != Other.Major) return Major < Other.Major;
                if (Minor != Other.Minor) return Minor < Other.Minor;
                return Patch < Other.Patch;
            }
            inline bool operator>(const FVersion& Other) const { return Other < *this; }
            inline bool operator<=(const FVersion& Other) const { return !(*this > Other); }
            inline bool operator>=(const FVersion& Other) const { return !(*this < Other); }
        };

        struct FModuleEntry
        {
            FString ModuleID;
            FString JsonPath;
            Thespeon::Core::FVersion Version; //optional version for collision detection and such.

            // Default constructor
            FModuleEntry() = default;

            // Parameterized constructor
            FModuleEntry(const FString& InID, const FString& InPath, const Thespeon::Core::FVersion& InVersion)
                : ModuleID(InID), JsonPath(InPath), Version(InVersion)
            {
            }

            // Check if the module entry is empty
            bool IsEmpty() const
            {
                return ModuleID.IsEmpty() || JsonPath.IsEmpty();
            }
        };
        struct FModuleFile
        {
            FString FileName; //Directory part of path seems to be gotten by RTFL nowadays (GetRuntimeModelPath/GetRuntimeFilePath) so file name here is enough
            FString MD5;

            FModuleFile(FString fileName, FString md5) : FileName(fileName), MD5(md5) {}
        };

        
        /// @brief Base class for modules - Core abstraction for managing AI model resources
        /// Represents packaged units of functionality for text-to-speech synthesis
        class Module
        {
        public:
            virtual ~Module() = default;
            
            // Core metadata
            FString ModuleID;
            FString JSONPath;
            Thespeon::Core::FVersion Version;
            
            // File management
            TMap<FString, FModuleFile> InternalFileMappings; // map of file identifier to file path and md5
            TMap<FString, FString> InternalModelMappings; // map of internal model names to module md5s
            
            // Core shared methods
            bool LoadModels(const TSet<FString>& LoadedMD5s, TMap<FString, TStrongObjectPtr<UNNEModelData>>& OutLoadedModels) const;
            FString GetInternalModelID(const FString& InternalName) const;
            
            // Abstract methods that derived classes must implement
            virtual const FString GetModuleType() const = 0;
            virtual TSet<FString> GetAllFileMD5s() const = 0;
            virtual bool IsIncludedIn(const TSet<FString>& MD5s) const = 0; // Checks if module is fully included in provided MD5 set
            
        protected:
            // Protected constructor - only derived classes can create modules
            Module(const Thespeon::Core::FModuleEntry& Entry);
            
            // Helper method to initialize from JSON data - called by derived classes
            virtual bool InitializeFromJSON(const FString& JsonString) = 0;
        private:
            bool LoadModelsAsync(const TArray<FSoftObjectPath>& SoftPaths, TArray<TStrongObjectPtr<UNNEModelData>>& OutLoadedModels) const;

        };
    }
}
