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
#include "caffe2/core/miopen_wrappers.h"
#include "caffe2/core/operator.h"
#include "caffe2/core/types.h"

namespace caffe2 {

class MIOPEN_LRNOP final : public Operator<HIPContext> {
 public:
  USE_OPERATOR_FUNCTIONS(HIPContext);

  MIOPEN_LRNOP(const OperatorDef& operator_def, Workspace* ws)
      : Operator<HIPContext>(operator_def, ws),
        miopen_wrapper_(&context_),
        size_(OperatorBase::GetSingleArgument<int>("size", 0)),
        alpha_(OperatorBase::GetSingleArgument<float>("alpha", 0)),
        beta_(OperatorBase::GetSingleArgument<float>("beta", 0)),
        bias_(OperatorBase::GetSingleArgument<float>("bias", 1)) {
    MIOPEN_ENFORCE(miopenCreateTensorDescriptor(&data_desc_));

    MIOPEN_ENFORCE(miopenCreateLRNDescriptor(&norm_desc_));
    MIOPEN_ENFORCE(
        miopenSetLRNDescriptor(norm_desc_, size_, alpha_, beta_, bias_));
  }

  ~MIOPEN_LRNOP() {
    MIOPEN_ENFORCE(miopenDestroyTensorDescriptor(data_desc_));
    MIOPEN_ENFORCE(miopenDestroyLRNDescriptor(norm_desc_));
  }

  template <typename T, typename M>
  bool DoRunWithType();

  bool RunOnDevice() override;

 protected:
  MIOPENWrapper miopen_wrapper_;
  miopenTensorDescriptor_t data_desc_;
  miopenLRNDescriptor_t norm_desc_;

  vector<TIndex> miopen_input_dims_;

  const int size_;
  const float alpha_;
  const float beta_;
  const float bias_;

  // Input: X, Output: Y
};

class MIOPENLRNGradientOp final : public Operator<HIPContext> {
 public:
  USE_OPERATOR_FUNCTIONS(HIPContext);
  MIOPENLRNGradientOp(const OperatorDef& operator_def, Workspace* ws)
      : Operator<HIPContext>(operator_def, ws),
        miopen_wrapper_(&context_),
        size_(OperatorBase::GetSingleArgument<int>("size", 0)),
        alpha_(OperatorBase::GetSingleArgument<float>("alpha", 0)),
        beta_(OperatorBase::GetSingleArgument<float>("beta", 0)),
        bias_(OperatorBase::GetSingleArgument<float>("bias", 1)) {
    MIOPEN_ENFORCE(miopenCreateTensorDescriptor(&data_desc_));

    MIOPEN_ENFORCE(miopenCreateLRNDescriptor(&norm_desc_));
    MIOPEN_ENFORCE(
        miopenSetLRNDescriptor(norm_desc_, size_, alpha_, beta_, bias_));
  }

  ~MIOPENLRNGradientOp() {
    MIOPEN_ENFORCE(miopenDestroyTensorDescriptor(data_desc_));
    MIOPEN_ENFORCE(miopenDestroyLRNDescriptor(norm_desc_));
  }

  template <typename T, typename M>
  bool DoRunWithType();

  bool RunOnDevice() override;

 protected:
  MIOPENWrapper miopen_wrapper_;
  miopenTensorDescriptor_t data_desc_;
  miopenLRNDescriptor_t norm_desc_;

  vector<TIndex> miopen_input_dims_;

  const int size_;
  const float alpha_;
  const float beta_;
  const float bias_;

  // Input: X, Y, dY
  // Output: dX
};

template <typename T, typename M>
bool MIOPEN_LRNOP::DoRunWithType() {
  const auto& X = Input(0);
  auto* Y = Output(0);

  // Reshape tensor descriptors if necessary
  if (X.dims() != miopen_input_dims_) {
    VLOG(1) << "Setting descriptors";
    miopen_input_dims_ = X.dims();
    int C = 1, H = 1, W = 1;
    // Normal 4-dimensional tensors for images.
    C = X.dim32(1);
    H = X.dim32(2);
    W = X.dim32(3);
    MIOPEN_ENFORCE(miopenSetTensor4dDescriptor(
        data_desc_,
        GetMIOPENTensorFormat(StorageOrder::NCHW),
        miopenTypeWrapper<T>::type,
        X.dim32(0),
        C,
        H,
        W));
  }

  // now actually run the computation
  MIOPEN_ENFORCE(miopenLRNCrossChannelForward(
      miopen_wrapper_.inline_miopen_handle(),
      norm_desc_,
      MIOPEN_LRN_CROSS_CHANNEL_DIM1,
      miopenTypeWrapper<T>::kOne(),
      data_desc_,
      X.template data<T>(),
      miopenTypeWrapper<T>::kZero(),
      data_desc_,
      Y->template mutable_data<T>()));

  return true;
}

bool MIOPEN_LRNOP::RunOnDevice() {
  // dispatch based on contents of tensor(s)
  const auto& X = Input(0);
  auto* Y = Output(0);
  Y->ResizeLike(X);

  if (X.IsType<float>()) {
    return DoRunWithType<float, float>();
  } else if (X.IsType<float16>()) {
    return DoRunWithType<float16, float>();
  } else {
    CAFFE_THROW("Unsupported input type");
  }
  return false;
}

template <typename T, typename M>
bool MIOPENLRNGradientOp::DoRunWithType() {
  const auto& X = Input(0);
  const auto& Y = Input(1);
  const auto& dY = Input(2);
  auto* dX = Output(0);

  if (dY.dims() != miopen_input_dims_) {
    VLOG(1) << "Setting descriptors";
    miopen_input_dims_ = dY.dims();
    int C = 1, H = 1, W = 1;
    // Normal 4-dimensional tensors for images.
    C = dY.dim32(1);
    H = dY.dim32(2);
    W = dY.dim32(3);
    MIOPEN_ENFORCE(miopenSetTensor4dDescriptor(
        data_desc_,
        GetMIOPENTensorFormat(StorageOrder::NCHW),
        miopenTypeWrapper<T>::type,
        dY.dim32(0),
        C,
        H,
        W));
  }

  // run the computation
  MIOPEN_ENFORCE(miopenLRNCrossChannelBackward(
      miopen_wrapper_.inline_miopen_handle(),
      norm_desc_,
      MIOPEN_LRN_CROSS_CHANNEL_DIM1,
      miopenTypeWrapper<T>::kOne(),
      data_desc_,
      Y.template data<T>(),
      data_desc_,
      dY.template data<T>(),
      data_desc_,
      X.template data<T>(),
      miopenTypeWrapper<T>::kZero(),
      data_desc_,
      dX->template mutable_data<T>()));
  return true;
}

bool MIOPENLRNGradientOp::RunOnDevice() {
  // dispatch based on contents of tensor(s)
  const auto& X = Input(0);
  const auto& Y = Input(1);
  const auto& dY = Input(2);
  auto* dX = Output(0);

  dX->ResizeLike(dY);

  if (dY.IsType<float>()) {
    return DoRunWithType<float, float>();
  } else if (dY.IsType<float16>()) {
    return DoRunWithType<float16, float>();
  } else {
    CAFFE_THROW("Unsupported input type");
  }

  return false;
}

namespace {
REGISTER_MIOPEN_OPERATOR(LRN, MIOPEN_LRNOP);
REGISTER_MIOPEN_OPERATOR(LRNGradient, MIOPENLRNGradientOp);
}

}; // namespace caffe2
