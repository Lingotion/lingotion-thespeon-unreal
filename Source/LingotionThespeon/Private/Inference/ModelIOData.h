// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "NNETypes.h"
#include "Core/LingotionLogger.h"

namespace Thespeon::Inference
{
/**
 * Typed tensor data container for NNE model input/output.
 *
 * Wraps a raw byte buffer with shape and type metadata. Created exclusively
 * through static factory methods (Make, MakeFromArray). Provides type-safe
 * accessors for reading data as arrays or individual values.
 */
class ModelIOData
{
  public:
	/** @brief Creates a zero-initialized tensor with the given shape and data type.
	 *  @param Shape The tensor dimensions.
	 *  @param Type The NNE data type.
	 *  @return A zero-initialized ModelIOData. */
	static ModelIOData Make(const UE::NNE::FTensorShape& Shape, ENNETensorDataType Type)
	{
		ModelIOData Obj(Shape, Type);
		checkf(Obj.MaxSizeInBytes <= (size_t)TNumericLimits<int32>::Max(), TEXT("Buffer too large for TArray<uint8>"));
		Obj.DataBuffer.SetNumZeroed(Obj.MaxSizeInBytes);
		return Obj;
	}

	/** @brief Creates a tensor initialized to a uniform value of type T.
	 *  @param Shape The tensor dimensions.
	 *  @param InitValue The value to fill every element with.
	 *  @return A uniformly initialized ModelIOData. */
	template <typename T> static ModelIOData Make(const UE::NNE::FTensorShape& Shape, T InitValue)
	{
		ModelIOData result(Shape, TNNEDataTypeOf<T>::Value);
		checkf(result.MaxSizeInBytes <= (size_t)TNumericLimits<int32>::Max(), TEXT("Buffer too large for TArray<uint8>"));
		result.DataBuffer.SetNumZeroed(result.MaxSizeInBytes);
		TArray<T> initialValues;
		initialValues.Init(InitValue, result.GetNumElements());
		if (!result.TrySetValueFromArray<T>(initialValues))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Tensor make failed. Returning zero-initialized ModelIOData."));
			return result;
		}

		return result;
	}

	/** @brief Creates a tensor from an existing array of type T.
	 *  @param Shape The tensor dimensions.
	 *  @param Src Source data to copy into the tensor.
	 *  @return A ModelIOData populated from Src. */
	template <typename T> static ModelIOData MakeFromArray(const UE::NNE::FTensorShape& Shape, TConstArrayView<T> Src)
	{
		ModelIOData result = Make(Shape, TNNEDataTypeOf<T>::Value);

		if (!result.TrySetValueFromArray<T>(Src))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Tensor make from array failed. Returning zero-initialized ModelIOData."));
			return result;
		}

		return result;
	}

	/** @brief Copies data from an array into the tensor buffer.
	 *  @param Src Source array to copy from.
	 *  @return False on size mismatch. */
	template <typename T> bool TrySetValueFromArray(TConstArrayView<T> Src)
	{
		const uint64 Needed = MaxSizeInBytes;
		const uint64 Have = static_cast<uint64>(Src.Num()) * sizeof(T);
		if (Have != Needed)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Tensor set from array failed due to size mismatch (%llu vs %llu)"), Have, Needed);
			return false;
		}

		FMemory::Memcpy(DataBuffer.GetData(), Src.GetData(), Needed);
		return true;
	}

	/** @brief Returns the maximum buffer size in bytes.
	 *  @return Size in bytes. */
	size_t GetMaxSizeInBytes() const
	{
		return MaxSizeInBytes;
	}

	/** @brief Returns a const pointer to the raw data buffer.
	 *  @return Const void pointer to the data. */
	const void* GetData() const
	{
		return DataBuffer.GetData();
	}

	/** @brief Returns a mutable pointer to the raw data buffer.
	 *  @return Mutable void pointer to the data. */
	void* GetData()
	{
		return DataBuffer.GetData();
	}

	/** @brief Returns the tensor shape (dimensions).
	 *  @return Const reference to the tensor shape. */
	const UE::NNE::FTensorShape& GetTensorShape() const
	{
		return TensorShape;
	}

	/** @brief Returns the NNE data type of this tensor.
	 *  @return The tensor's data type. */
	ENNETensorDataType GetTensorDataType() const
	{
		return TensorDataType;
	}

	/** @brief Returns the total number of elements in this tensor.
	 *  @return Element count. */
	uint64 GetNumElements() const
	{
		return NumElementsOfType;
	}

	/** @brief Copies the entire tensor buffer into a TArray of type T.
	 *  @return Array containing a copy of the tensor data. */
	template <typename T> TArray<T> GetDataAsArray() const
	{
		checkf(MaxSizeInBytes % sizeof(T) == 0, TEXT("Type size does not divide buffer size"));
		TArray<T> Result;
		Result.AddUninitialized(MaxSizeInBytes / sizeof(T));
		FMemory::Memcpy(Result.GetData(), DataBuffer.GetData(), MaxSizeInBytes);
		return Result;
	}

	/** @brief Returns a single element at the given index, cast to type T.
	 *  @param index Zero-based element index into the tensor buffer.
	 *  @return The value at the given index. */
	template <typename T> T GetDataAsValue(uint64 index) const
	{
		T result;
		FMemory::Memcpy(&result, DataBuffer.GetData() + index * sizeof(T), sizeof(T));
		return result;
	}

  private:
	// convert T to corresponding ENNETensorDataType
	template <typename T> struct TNNEDataTypeOf;
	template <> struct TNNEDataTypeOf<float>
	{
		static constexpr ENNETensorDataType Value = ENNETensorDataType::Float;
	};
	template <> struct TNNEDataTypeOf<int32>
	{
		static constexpr ENNETensorDataType Value = ENNETensorDataType::Int32;
	};
	template <> struct TNNEDataTypeOf<int64>
	{
		static constexpr ENNETensorDataType Value = ENNETensorDataType::Int64;
	};
	template <> struct TNNEDataTypeOf<uint8>
	{
		static constexpr ENNETensorDataType Value = ENNETensorDataType::UInt8;
	};

	// constructor is private so that none other than factory functions may create object
	ModelIOData(const UE::NNE::FTensorShape& Shape, ENNETensorDataType Type)
	    : MaxSizeInBytes(Shape.Volume() * UE::NNE::GetTensorDataTypeSizeInBytes(Type))
	    , TensorShape(Shape)
	    , TensorDataType(Type)
	    , NumElementsOfType(Shape.Volume())
	{
	}

	size_t MaxSizeInBytes = 0;
	TArray<uint8> DataBuffer;
	UE::NNE::FTensorShape TensorShape;
	ENNETensorDataType TensorDataType;
	uint64 NumElementsOfType = 0;
};
} // namespace Thespeon::Inference
