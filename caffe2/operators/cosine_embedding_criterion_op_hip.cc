#include "hip/hip_runtime.h"
/**
 * Copyright (c) 2016-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "caffe2/core/context_hip.h"
#include "caffe2/operators/cosine_embedding_criterion_op.h"

namespace caffe2 {
namespace {


__global__ void CECKernel(
    const int N, const float* S, const int* Y, const float margin,
    float* output) {
  HIP_1D_KERNEL_LOOP(i, N) {
    output[i] = Y[i] == 1 ? (1. - S[i]) : max(0.f, S[i] - margin);
  }
}

__global__ void CECGradientKernel(
    const int N, const float* S, const int* Y, const float* dOutput,
    const float margin, float* dS) {
  HIP_1D_KERNEL_LOOP(i, N) {
    dS[i] = dOutput[i] * (Y[i] == 1 ? -1 : static_cast<float>(S[i] >= margin));
  }
}
}  // namespace

template <>
bool CosineEmbeddingCriterionOp<HIPContext>::RunOnDevice() {
  auto& S = Input(0);
  auto& Y = Input(1);
  auto* output = Output(0);
  CAFFE_ENFORCE(S.size() == Y.size(),
                "The embedding and label should have the same size.");
  output->ResizeLike(S);

  const float* Sdata = S.data<float>();
  const int* Ydata = Y.data<int>();
  float* output_data = output->mutable_data<float>();
 
  hipLaunchKernelGGL((CECKernel), dim3(CAFFE_GET_BLOCKS(S.size())), dim3(CAFFE_HIP_NUM_THREADS), 0, context_.hip_stream(), 
      S.size(), Sdata, Ydata, margin_, output_data);
  return true;
}

template <>
bool CosineEmbeddingCriterionGradientOp<HIPContext>::RunOnDevice() {
  auto& S = Input(0);
  auto& Y = Input(1);
  auto& dOutput = Input(2);
  auto* dS = Output(0);

  dS->ResizeLike(S);

  const float* Sdata = S.data<float>();
  const int* Ydata = Y.data<int>();
  const float* dOutput_data = dOutput.data<float>();
  float* dSdata = dS->mutable_data<float>();
  hipLaunchKernelGGL((CECGradientKernel), dim3(CAFFE_GET_BLOCKS(S.size())), dim3(CAFFE_HIP_NUM_THREADS), 0, context_.hip_stream(), 
      S.size(), Sdata, Ydata, dOutput_data, margin_, dSdata);
  return true;
}

REGISTER_HIP_OPERATOR(
    CosineEmbeddingCriterion,
    CosineEmbeddingCriterionOp<HIPContext>);
REGISTER_HIP_OPERATOR(
    CosineEmbeddingCriterionGradient,
    CosineEmbeddingCriterionGradientOp<HIPContext>);
}  // namespace caffe2
