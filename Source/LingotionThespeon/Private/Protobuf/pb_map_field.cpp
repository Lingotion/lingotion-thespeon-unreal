// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

// Wrapper for protobuf source file
#if (defined(PLATFORM_MAC) && PLATFORM_MAC) || (defined(PLATFORM_LINUX) && PLATFORM_LINUX)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wshadow"
    #pragma clang diagnostic ignored "-Wunused-parameter"
    #pragma clang diagnostic ignored "-Wunused-variable"
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"
    #pragma clang diagnostic ignored "-Wdeprecated-pragma"
    #pragma clang diagnostic ignored "-Wundef"
#elif PLATFORM_WINDOWS
    #pragma warning(push)
    #pragma warning(disable: 4125)
    #pragma warning(disable: 4244)
    #pragma warning(disable: 4800)
    #pragma warning(disable: 4456)
    #pragma warning(disable: 4459)
#endif

#include "../../../ThirdParty/Protobuf/protobuf/src/google/protobuf/map_field.cc"

#if (defined(PLATFORM_MAC) && PLATFORM_MAC) || (defined(PLATFORM_LINUX) && PLATFORM_LINUX)
    #pragma clang diagnostic pop
#elif PLATFORM_WINDOWS
    #pragma warning(pop)
#endif
