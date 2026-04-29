// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Misc/TVariant.h"
#include "SessionTensorPool.h"

namespace Thespeon::Inference
{
using FHostValue = TVariant<int64, float, bool, ModelIOData, TArray<float>, TArray<int64>, FString>;
using FHostMap = TMap<FString, FHostValue>;
class FSymExprParser
{
  public:
	struct VarDictEntry
	{
		FString TargetTensor;
		int32 Dim;
	};

	static bool Evaluate(
	    const FString& Expression,
	    const TMap<FString, VarDictEntry>& VarDict,
	    const FHostMap& Host,
	    const SessionTensorPool& TensorPool,
	    uint32& OutValue
	);

  private:
	static void SkipWhitespace(const FString& Expression, int32 Length, int32& Index);

	static bool ParseNumber(const FString& Expression, int32 Length, int32& Index, int32& OutValue);

	static bool ParseVariable(
	    const FString& Expression,
	    int32 Length,
	    int32& Index,
	    const TMap<FString, VarDictEntry>& VarDict,
	    const FHostMap& Host,
	    const SessionTensorPool& TensorPool,
	    int32& OutValue
	);

	static bool EvaluateNextFactor(
	    const FString& Expression,
	    int32 Length,
	    int32& Index,
	    const TMap<FString, VarDictEntry>& VarDict,
	    const FHostMap& Host,
	    const SessionTensorPool& TensorPool,
	    int32& OutValue
	);

	static bool EvaluateNextTerm(
	    const FString& Expression,
	    int32 Length,
	    int32& Index,
	    const TMap<FString, VarDictEntry>& VarDict,
	    const FHostMap& Host,
	    const SessionTensorPool& TensorPool,
	    int32& OutValue
	);

	static bool ParseExpression(
	    const FString& Expression,
	    int32 Length,
	    int32& Index,
	    const TMap<FString, VarDictEntry>& VarDict,
	    const FHostMap& Host,
	    const SessionTensorPool& TensorPool,
	    int32& OutValue
	);
};
} // namespace Thespeon::Inference
