#include "SymExprParser.h"
#include "ModelIOData.h"

using namespace Thespeon::Inference;

static const TCHAR PLUS = '+';
static const TCHAR MINUS = '-';
static const TCHAR MUL = '*';
static const TCHAR DIV = '/';
static const TCHAR MOD = '%';

void FSymExprParser::SkipWhitespace(const FString& Expression, int32 Length, int32& Index)
{
	while (Index < Length && FChar::IsWhitespace(Expression[Index]))
	{
		++Index;
	}
}

bool FSymExprParser::ParseNumber(const FString& Expression, int32 Length, int32& Index, int32& OutValue)
{
	int32 Value = 0;
	bool bHasDigit = false;

	while (Index < Length && FChar::IsDigit(Expression[Index]))
	{
		bHasDigit = true;
		Value = Value * 10 + (Expression[Index] - TEXT('0'));
		++Index;
	}

	if (!bHasDigit)
	{
		return false;
	}

	OutValue = Value;
	return true;
}

bool FSymExprParser::ParseVariable(
    const FString& Expression,
    int32 Length,
    int32& Index,
    const TMap<FString, VarDictEntry>& VarDict,
    const FHostMap& Host,
    const SessionTensorPool& TensorPool,
    int32& OutValue
)
{
	int32 Start = Index;

	while (Index < Length && (FChar::IsAlnum(Expression[Index]) || Expression[Index] == TEXT('_')))
	{
		++Index;
	}

	if (Start == Index)
	{
		return false;
	}

	const FString Name = Expression.Mid(Start, Index - Start);

	const ModelIOData* Tensor = nullptr;
	const VarDictEntry* Entry = VarDict.Find(Name);
	if (Entry)
	{
		// check host first, as we want to be able to "override" a tensor dim value if necessary.
		if (Host.Contains(Entry->TargetTensor))
		{
			const FHostValue* HostValue = Host.Find(Entry->TargetTensor);
			Tensor = HostValue->TryGet<ModelIOData>();
		}
		else if (!TensorPool.TryGetTensor(Entry->TargetTensor, Tensor))
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("SymExprParser: invalid tensor %s"), *Entry->TargetTensor);
			return false;
		}

		OutValue = Tensor->GetTensorShape().GetData()[Entry->Dim];
		return true;
	}

	// If the entry is not in the symbolic dict, look through host and device dicts for matches
	if (Host.Contains(Name))
	{
		const FHostValue* HostValue = Host.Find(Name);
		Tensor = HostValue->TryGet<ModelIOData>();
		if (!Tensor)
		{
			const int64* resultDimInt = HostValue->TryGet<int64>();
			if (resultDimInt)
			{
				OutValue = static_cast<int32>(*resultDimInt);
				return true;
			}

			const float* resultDimFloat = HostValue->TryGet<float>();
			if (resultDimFloat)
			{
				OutValue = static_cast<int32>(*resultDimFloat);
				return true;
			}
			LINGO_LOG(EVerbosityLevel::Error, TEXT("SymExprParser: invalid host value type with name %s"), *Name);
			return false;
		}
	}
	else if (!TensorPool.TryGetTensor(Name, Tensor))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("SymExprParser: invalid tensor %s"), *Name);
		return false;
	}

	OutValue = Tensor->GetDataAsValue<int32>(0);
	return true;
}

bool FSymExprParser::EvaluateNextFactor(
    const FString& Expression,
    int32 Length,
    int32& Index,
    const TMap<FString, VarDictEntry>& VarDict,
    const FHostMap& Host,
    const SessionTensorPool& TensorPool,
    int32& OutValue
)
{
	SkipWhitespace(Expression, Length, Index);

	if (Index < Length && (Expression[Index] == PLUS || Expression[Index] == MINUS))
	{
		const bool bNegative = Expression[Index] == MINUS;
		++Index;

		if (!EvaluateNextFactor(Expression, Length, Index, VarDict, Host, TensorPool, OutValue))
		{
			return false;
		}

		if (bNegative)
		{
			OutValue = -OutValue;
		}
		return true;
	}

	if (Index < Length && Expression[Index] == TEXT('('))
	{
		++Index;

		if (!ParseExpression(Expression, Length, Index, VarDict, Host, TensorPool, OutValue))
		{
			return false;
		}

		SkipWhitespace(Expression, Length, Index);
		if (Index >= Length || Expression[Index] != TEXT(')'))
		{
			return false;
		}

		++Index;
		return true;
	}

	if (Index < Length && FChar::IsDigit(Expression[Index]))
	{
		return ParseNumber(Expression, Length, Index, OutValue);
	}

	return ParseVariable(Expression, Length, Index, VarDict, Host, TensorPool, OutValue);
}

bool FSymExprParser::EvaluateNextTerm(
    const FString& Expression,
    int32 Length,
    int32& Index,
    const TMap<FString, VarDictEntry>& VarDict,
    const FHostMap& Host,
    const SessionTensorPool& TensorPool,
    int32& OutValue
)
{
	if (!EvaluateNextFactor(Expression, Length, Index, VarDict, Host, TensorPool, OutValue))
	{
		return false;
	}

	while (true)
	{
		SkipWhitespace(Expression, Length, Index);

		if (Index >= Length || (Expression[Index] != MUL && Expression[Index] != DIV && Expression[Index] != MOD))
		{
			break;
		}

		TCHAR Op = Expression[Index++];
		int32 RHS;

		if (!EvaluateNextFactor(Expression, Length, Index, VarDict, Host, TensorPool, RHS))
		{
			return false;
		}

		switch (Op)
		{

			case MUL:
				OutValue = OutValue * RHS;
				break;

			case DIV:
				if (RHS == 0)
				{
					LINGO_LOG(EVerbosityLevel::Error, TEXT("division by zero"));
					return false;
				}
				OutValue = OutValue / RHS;
				break;

			case MOD:
				if (RHS == 0)
				{
					LINGO_LOG(EVerbosityLevel::Error, TEXT("modulo by zero"));
					return false;
				}
				OutValue = OutValue % RHS;
				break;

			default:
				break;
		}
	}

	return true;
}

bool FSymExprParser::ParseExpression(
    const FString& Expression,
    int32 Length,
    int32& Index,
    const TMap<FString, VarDictEntry>& VarDict,
    const FHostMap& Host,
    const SessionTensorPool& TensorPool,
    int32& OutValue
)
{
	if (!EvaluateNextTerm(Expression, Length, Index, VarDict, Host, TensorPool, OutValue))
	{
		return false;
	}

	while (true)
	{
		SkipWhitespace(Expression, Length, Index);

		if (Index >= Length || (Expression[Index] != PLUS && Expression[Index] != MINUS))
		{
			break;
		}

		TCHAR Op = Expression[Index++];
		int32 RHS;

		if (!EvaluateNextTerm(Expression, Length, Index, VarDict, Host, TensorPool, RHS))
		{
			return false;
		}

		OutValue = (Op == PLUS) ? (OutValue + RHS) : (OutValue - RHS);
	}

	return true;
}

bool FSymExprParser::Evaluate(
    const FString& Expression, const TMap<FString, VarDictEntry>& VarDict, const FHostMap& Host, const SessionTensorPool& TensorPool, uint32& OutValue
)
{
	int32 Index = 0;
	const int32 Length = Expression.Len();
	int32 Result = 0;
	if (!ParseExpression(Expression, Length, Index, VarDict, Host, TensorPool, Result) || Result < 0)
	{
		return false;
	}

	OutValue = (uint32)Result;
	return true;
}
