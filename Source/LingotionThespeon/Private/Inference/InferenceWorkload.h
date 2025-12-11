// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "NNETypes.h"
#include "NNERuntimeCPU.h"
#include "NNERuntimeGPU.h"
#include "../../Public/Engine/InferenceConfig.h"
#include "SessionTensorPool.h"


namespace Thespeon
{
    namespace Inference
    {
        /// @brief Class representing a workload that runs inference on a model. Only responsible for one onnx run with given input.
        /// Later make this a base/abstrac class with one implementation per runtime we support.
        class InferenceWorkload
        {
        public: 
            InferenceWorkload(const TStrongObjectPtr<UNNEModelData>& ModelData, EBackendType BackendType); // creates model instance based on runtime and stores in ModelInstance.
            /// @brief Creates our CPU runtime and model instance. Uses instance descriptors to get tensor names and extract input tensors from tensor pool. 
            /// Populates Runtime (ex: FTensorBindingCPU) with input model data and allocates output tensors. 
            /// Runs inference and sets output ModelIOData to TensorPool.
            /// We might be able to calculate OutputTensorShapes based on ModelData and a config. Unclear whether to do that here or in a separate helper which supplies OutputTensorShapes to this.
            void Infer(SessionTensorPool& TensorPool, const TConstArrayView<UE::NNE::FTensorShape>& OutputTensorShapes, const FInferenceConfig& Config, FString debugName = "");
            TConstArrayView<UE::NNE::FTensorShape> MockInfer(SessionTensorPool& TensorPool, const FInferenceConfig& Config, FString debugName = "");
            TConstArrayView<UE::NNE::FTensorDesc> GetOutputTensorDesc(); // Remove when vocoder remapping is fixed
        private:
            SIZE_T CalculateByteSize(const UE::NNE::FTensorShape& Shape, uint32 ByteSize); //helper for correct allocation.
            bool ValidateOutputs(const TArray<ModelIOData>& ModelIO); //not strictly necessary just for sanity. Remove when we are ready.
            FString GetDataAsString(const ModelIOData& Data); //For debugging.
            bool AnyZero(const TArray<ModelIOData>& ModelIO); //For debugging.
            FString FormatMel(const ModelIOData& MelData); //For debugging.
            // For now only allow the IModelInstanceCPU subclass of IModelInstanceRunSync. Later we want to change this ptr to IModelInstanceRunSync and maybe have different implementations of InferenceWorkload for each runtime.
            TSharedPtr<UE::NNE::IModelInstanceRunSync> ModelInstance;

        };
    }
}
