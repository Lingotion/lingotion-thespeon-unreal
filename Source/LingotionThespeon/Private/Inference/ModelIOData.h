// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "NNETypes.h"
#include "Core/LingotionLogger.h"

namespace Thespeon::Inference
{
	class ModelIOData
	{
	public:
		
		// factory methods
		static ModelIOData Make(const UE::NNE::FTensorShape& Shape,
		                                   ENNETensorDataType Type)
		{
			ModelIOData Obj(Shape, Type);
			checkf(Obj.MaxSizeInBytes <= (size_t)TNumericLimits<int32>::Max(),
           		TEXT("Buffer too large for TArray<uint8>"));
            Obj.DataBuffer.SetNumZeroed(Obj.MaxSizeInBytes);
			return Obj;
		}

		template<typename T>
		static ModelIOData MakeFromArray(const UE::NNE::FTensorShape& Shape,
		                                            TConstArrayView<T> Src)
		{
			ModelIOData result = Make(Shape, TNNEDataTypeOf<T>::Value);

			if (!result.TrySetValueFromArray<T>(Src))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("ModelIOData::MakeFromArray failed; returning zero-initialized ModelIOData"));
				return result;
			}

			return result;
		}

		template<typename T>
		bool TrySetValueFromArray(TConstArrayView<T> Src)
		{
			const uint64 Needed = MaxSizeInBytes;
			const uint64 Have   = static_cast<uint64>(Src.Num()) * sizeof(T);
			if (Have != Needed)
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("ModelIOData::TrySetValueFromArray: size mismatch (%llu vs %llu)"),
					Have,
					Needed);
				return false;
			}

			FMemory::Memcpy(DataBuffer.GetData(), Src.GetData(), Needed);
			return true;
		}

		// getters are const so that they can be used in a const context
		size_t GetMaxSizeInBytes() const { return MaxSizeInBytes; }
		const void* GetData() const { return DataBuffer.GetData(); }
		void*       GetData() { return DataBuffer.GetData(); }
		const UE::NNE::FTensorShape& GetTensorShape() const { return TensorShape; }
		ENNETensorDataType GetTensorDataType() const { return TensorDataType; }
		uint64 GetNumElements() const { return NumElementsOfType; }

		// quality-of-life data fetches
		template<typename T>
		TArray<T> GetDataAsArray() const
		{
			checkf(MaxSizeInBytes % sizeof(T) == 0,
			       TEXT("Type size does not divide buffer size"));
			TArray<T> Result;
			Result.AddUninitialized(MaxSizeInBytes / sizeof(T));
			FMemory::Memcpy(Result.GetData(), DataBuffer.GetData(), MaxSizeInBytes);
			return Result;
		}

        template<typename T>
		T GetDataAsValue(uint64 index) const
		{
            T result;
			FMemory::Memcpy(&result, DataBuffer.GetData() + index * sizeof(T), sizeof(T));
			return result;
		}

	private:
		// convert T to corresponding ENNETensorDataType
		template<typename T> struct TNNEDataTypeOf;
		template<> struct TNNEDataTypeOf<float>   { static constexpr ENNETensorDataType Value = ENNETensorDataType::Float; };
		template<> struct TNNEDataTypeOf<int32>   { static constexpr ENNETensorDataType Value = ENNETensorDataType::Int32; };
		template<> struct TNNEDataTypeOf<int64>   { static constexpr ENNETensorDataType Value = ENNETensorDataType::Int64; };
		template<> struct TNNEDataTypeOf<uint8>   { static constexpr ENNETensorDataType Value = ENNETensorDataType::UInt8; };

		// constructor is private so that none other than factory functions may create object
		ModelIOData(const UE::NNE::FTensorShape& Shape,
		            ENNETensorDataType Type)
			: MaxSizeInBytes(Shape.Volume() * UE::NNE::GetTensorDataTypeSizeInBytes(Type))
			, TensorShape(Shape)
			, TensorDataType(Type)
			, NumElementsOfType(Shape.Volume())
		{}

		size_t MaxSizeInBytes = 0;
		TArray<uint8> DataBuffer;
		UE::NNE::FTensorShape TensorShape;
		ENNETensorDataType TensorDataType;
		uint64 NumElementsOfType = 0;
	};
}
