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

#include "hip/hip_runtime.h"
#include "caffe2/core/context_hip.h"
#include "caffe2/operators/cast_op.h"
#include "caffe2/utils/conversions.h"

namespace caffe2 {

template <typename DstType, typename SrcType>
__global__ void CastKernel(const int N, const SrcType* X, DstType* Y) {
  HIP_1D_KERNEL_LOOP(i, N) {
    Y[i] = convert::To<SrcType, DstType>(X[i]);
  }
}

template <>
template <typename DstType, typename SrcType>
bool CastOp<HIPContext>::DoRunWithType() {
  auto& input = Input(0);
  auto* output = Output(0);
  output->ResizeLike(input);
  const auto* data = input.template data<SrcType>();
  auto* out = output->template mutable_data<DstType>();
  DCHECK(input.size() < INT_MAX);
  int N = input.size();
  if (N == 0) {
    // skip the rest of the computation if input is empty
    return true;
  }
  hipLaunchKernelGGL((CastKernel<DstType, SrcType>), dim3(CAFFE_GET_BLOCKS(N)), dim3(CAFFE_HIP_NUM_THREADS), 0, context_.hip_stream(), N, data, out);
  return true;
}

template <>
template <typename DstType>
bool CastOp<HIPContext>::DoRunWithDstType() {
  return DispatchHelper<
      TensorTypes<
          float,
          int32_t,
          bool,
          uint8_t,
          int8_t,
          uint16_t,
          int16_t,
          int64_t,
          double>,
      DstType>::call(this, Input(0));
}

// specific version that allows for casting to fp16
template <>
template <>
bool CastOp<HIPContext>::DoRunWithDstType<float>() {
  return DispatchHelper<
      TensorTypes<
          float,
          float16,
          int32_t,
          bool,
          uint8_t,
          int8_t,
          uint16_t,
          int16_t,
          int64_t,
          double>,
      float /* DstType */>::call(this, Input(0));
}

// specific version for casting _from_ fp16
template <>
template <>
bool CastOp<HIPContext>::DoRunWithDstType<float16>() {
  return DispatchHelper<
      TensorTypes<
          float,
          float16>,
      float16 /* DstType */>::call(this, Input(0));
}
template <>
void CastOp<HIPContext>::SetBody(TensorProto_DataType to) {
  switch (to) {
    case TensorProto_DataType_FLOAT:
      body_ = &CastOp<HIPContext>::DoRunWithDstType<float>;
      break;
    case TensorProto_DataType_INT32:
      body_ = &CastOp<HIPContext>::DoRunWithDstType<int>;
      break;
    case TensorProto_DataType_BYTE:
      LOG(FATAL) << "BYTE is deprecated";
      break;
    case TensorProto_DataType_STRING:
      CAFFE_THROW("Casting to and from strings is not supported yet");
      // break;
    case TensorProto_DataType_BOOL:
      body_ = &CastOp<HIPContext>::DoRunWithDstType<bool>;
      break;
    case TensorProto_DataType_UINT8:
      body_ = &CastOp<HIPContext>::DoRunWithDstType<uint8_t>;
      break;
    case TensorProto_DataType_INT8:
      body_ = &CastOp<HIPContext>::DoRunWithDstType<int8_t>;
      break;
    case TensorProto_DataType_UINT16:
      body_ = &CastOp<HIPContext>::DoRunWithDstType<uint16_t>;
      break;
    case TensorProto_DataType_INT16:
      body_ = &CastOp<HIPContext>::DoRunWithDstType<int16_t>;
      break;
    case TensorProto_DataType_INT64:
      body_ = &CastOp<HIPContext>::DoRunWithDstType<int64_t>;
      break;
    case TensorProto_DataType_FLOAT16:
      body_ = &CastOp<HIPContext>::DoRunWithDstType<float16>;
      break;
    case TensorProto_DataType_DOUBLE:
      body_ = &CastOp<HIPContext>::DoRunWithDstType<double>;
      break;
    case TensorProto_DataType_UNDEFINED:
      CAFFE_THROW("Cast op must have 'to' argument of type DataType");
      // break;
    default:
      CAFFE_THROW("Unexpected 'to' argument value: ", to);
  }
}

REGISTER_HIP_OPERATOR(Cast, CastOp<HIPContext>);

}  // namespace caffe2
