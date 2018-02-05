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
#include "caffe2/operators/softplus_op.h"

namespace caffe2 {
namespace {
template <typename T>
__global__ void SoftplusKernel(const int N, const T* X, T* Y) {
  HIP_1D_KERNEL_LOOP(i, N) {
    Y[i] = log(exp(X[i]) + 1.0f);
  }
}

template <typename T>
__global__ void
SoftplusGradientKernel(const int N, const T* Y, const T* dY, T* dX) {
  HIP_1D_KERNEL_LOOP(i, N) {
    const float nexpY = exp(-Y[i]);
    dX[i] = dY[i] * (1 - nexpY);
  }
}
} // namespace

template <>
bool SoftplusOp<float, HIPContext>::RunOnDevice() {
  auto& X = Input(0);
  auto* Y = Output(0);
  DCHECK_GT(X.size(), 0);
  Y->ResizeLike(X);
  hipLaunchKernelGGL((SoftplusKernel), dim3(CAFFE_GET_BLOCKS(X.size())), dim3(CAFFE_HIP_NUM_THREADS), 0, context_.hip_stream(), 
      X.size(), X.data<float>(), Y->mutable_data<float>());
  return true;
}

template <>
bool SoftplusGradientOp<float, HIPContext>::RunOnDevice() {
  auto& Y = Input(0);
  auto& dY = Input(1);
  auto* dX = Output(0);
  DCHECK_GT(Y.size(), 0);
  DCHECK_EQ(dY.size(), Y.size());
  dX->ResizeLike(Y);
  hipLaunchKernelGGL((SoftplusGradientKernel), dim3(CAFFE_GET_BLOCKS(Y.size())), dim3(CAFFE_HIP_NUM_THREADS), 0, context_.hip_stream(), 
      Y.size(), Y.data<float>(), dY.data<float>(), dX->mutable_data<float>());
  return true;
}

REGISTER_HIP_OPERATOR(Softplus, SoftplusOp<float, HIPContext>);
REGISTER_HIP_OPERATOR(
    SoftplusGradient,
    SoftplusGradientOp<float, HIPContext>);
} // namespace caffe2
