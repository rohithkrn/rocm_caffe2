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

#include "caffe2/utils/math.h"
#include "queue_ops.h"

#include "caffe2/core/context_hip.h"

namespace caffe2 {

REGISTER_HIP_OPERATOR(CreateBlobsQueue, CreateBlobsQueueOp<HIPContext>);
REGISTER_HIP_OPERATOR(EnqueueBlobs, EnqueueBlobsOp<HIPContext>);
REGISTER_HIP_OPERATOR(DequeueBlobs, DequeueBlobsOp<HIPContext>);
REGISTER_HIP_OPERATOR(CloseBlobsQueue, CloseBlobsQueueOp<HIPContext>);

REGISTER_HIP_OPERATOR(SafeEnqueueBlobs, SafeEnqueueBlobsOp<HIPContext>);
REGISTER_HIP_OPERATOR(SafeDequeueBlobs, SafeDequeueBlobsOp<HIPContext>);

}
