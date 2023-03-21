/* Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
Copyright (c) 2022 NVIDIA Corporation. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */
#pragma once

#include <functional>
#include <future>  // NOLINT
#include <memory>
#include <mutex>  // NOLINT
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "paddle/fluid/memory/malloc.h"
#include "paddle/fluid/platform/device/gpu/gpu_types.h"
#include "paddle/phi/backends/context_pool.h"
#include "paddle/phi/backends/cpu/cpu_context.h"
#include "paddle/phi/backends/custom/custom_context.h"
#include "paddle/phi/backends/gpu/gpu_decls.h"
#include "paddle/phi/core/device_context.h"
#ifdef PADDLE_WITH_CUDA
#include "paddle/fluid/platform/device/gpu/gpu_helper.h"
#include "paddle/fluid/platform/dynload/cublas.h"
#include "paddle/fluid/platform/dynload/cublasLt.h"
#include "paddle/fluid/platform/dynload/cudnn.h"
#include "paddle/fluid/platform/dynload/cusolver.h"
#include "paddle/fluid/platform/dynload/cusparse.h"
#include "paddle/phi/backends/gpu/gpu_context.h"
#if !defined(__APPLE__) && defined(PADDLE_WITH_NCCL)
#include "paddle/fluid/platform/dynload/nccl.h"
#endif
#include "paddle/fluid/platform/device/gpu/gpu_info.h"
#endif

#ifdef PADDLE_WITH_HIP
#include "paddle/fluid/platform/device/gpu/gpu_helper.h"  // NOLINT
#include "paddle/fluid/platform/dynload/miopen.h"
#include "paddle/fluid/platform/dynload/rocblas.h"
#include "paddle/phi/backends/gpu/gpu_context.h"  // NOLINT
#if !defined(__APPLE__) && defined(PADDLE_WITH_RCCL)
#include "paddle/fluid/platform/dynload/rccl.h"
#endif
#include "paddle/fluid/platform/device/gpu/gpu_info.h"  // NOLINT
#endif

#if defined(PADDLE_WITH_XPU_BKCL)
#include "xpu/bkcl.h"
#endif

#ifdef PADDLE_WITH_DNNL
#include "dnnl.hpp"  // NOLINT
#include "paddle/fluid/framework/data_layout.h"
#include "paddle/phi/backends/onednn/onednn_context.h"
#endif

#include <map>

#include "glog/logging.h"
#include "paddle/fluid/platform/enforce.h"
#include "paddle/fluid/platform/place.h"
#ifdef PADDLE_WITH_ASCEND_CL
#include "paddle/fluid/platform/device/npu/enforce_npu.h"
#include "paddle/fluid/platform/device/npu/npu_stream.h"
#endif

#include "paddle/phi/backends/device_ext.h"
#include "paddle/phi/backends/stream.h"

#if !defined(PADDLE_WITH_XPU_KP) || defined(__xpu_on_host__)
#include "unsupported/Eigen/CXX11/Tensor"
#endif

namespace Eigen {
struct DefaultDevice;
struct GpuDevice;
}  // namespace Eigen

#ifdef PADDLE_WITH_XPU
#include "paddle/fluid/platform/device/xpu/xpu_header.h"
#include "paddle/fluid/platform/device/xpu/xpu_info.h"
#include "paddle/phi/backends/xpu/xpu_context.h"
#endif

#ifdef PADDLE_WITH_ASCEND_CL
#include "acl/acl.h"
#include "paddle/fluid/platform/device/npu/npu_info.h"
#endif

namespace paddle {
namespace platform {

enum DeviceType {
  CPU = 0,
  CUDA = 1,
  NPU = 2,
  XPU = 3,
  IPU = 4,
  MLU = 5,
  CUSTOM_DEVICE = 6,

  MAX_DEVICE_TYPES = 7,
};

DeviceType Place2DeviceType(const platform::Place& place);

constexpr DeviceType kCPU = DeviceType::CPU;
constexpr DeviceType kCUDA = DeviceType::CUDA;
constexpr DeviceType kXPU = DeviceType::XPU;
constexpr DeviceType kNPU = DeviceType::NPU;
constexpr DeviceType kIPU = DeviceType::IPU;
constexpr DeviceType kMLU = DeviceType::MLU;
constexpr DeviceType kCUSTOM_DEVICE = DeviceType::CUSTOM_DEVICE;

using DeviceContext = phi::DeviceContext;

// Graphcore IPU
#ifdef PADDLE_WITH_IPU
class IPUDeviceContext
    : public DeviceContext,
      public phi::TypeInfoTraits<DeviceContext, IPUDeviceContext> {
 public:
  IPUDeviceContext() = delete;
  explicit IPUDeviceContext(IPUPlace place);
  virtual ~IPUDeviceContext();
  Eigen::DefaultDevice* eigen_device() const { return nullptr; }
  const Place& GetPlace() const override;
  /*! \brief  Wait for all operations completion in the stream. */
  void Wait() const override;

  static const char* name() { return "IPUDeviceContext"; }

 private:
  IPUPlace place_;
};
#endif

#ifdef PADDLE_WITH_MLU
class MLUDeviceContext;
#endif

#ifdef PADDLE_WITH_XPU
namespace xpu = baidu::xpu::api;
using XPUDeviceContext = phi::XPUContext;
#endif

#ifdef PADDLE_WITH_ASCEND_CL
class NPUDeviceContext
    : public DeviceContext,
      public phi::TypeInfoTraits<DeviceContext, NPUDeviceContext> {
 public:
  explicit NPUDeviceContext(NPUPlace place);
  virtual ~NPUDeviceContext();
  Eigen::DefaultDevice* eigen_device() const { return nullptr; }
  const Place& GetPlace() const override;
  aclrtContext context() const;

  /*! \brief  Wait for all operations completion in the stream. */
  void Wait() const override;

  /*! \brief  Return npu stream in the device context. */
  aclrtStream stream() const;

  template <typename Callback>
  void AddStreamCallback(Callback&& callback) const {
    return stream_->AddCallback(callback);
  }

  void WaitStreamCallback() const { return stream_->WaitCallback(); }

#if defined(PADDLE_WITH_ASCEND_CL)
  /*! \brief  Return hccl communicators. */
  HcclComm hccl_comm() const { return hccl_comm_; }

  /*! \brief  Set hccl communicators. */
  void set_hccl_comm(HcclComm comm) { hccl_comm_ = comm; }
#endif

  // template <typename Callback>
  // void AddStreamCallback(Callback&& callback) const {
  //   return stream_->AddCallback(callback);
  // }

  // void WaitStreamCallback() const { return stream_->WaitCallback(); }

  static const char* name() { return "NPUDeviceContext"; }

 private:
  NPUPlace place_;
  aclrtContext context_;

#ifdef PADDLE_WITH_ASCEND_CL
  // HCCLContext_t hccl_context_;
  HcclComm hccl_comm_{nullptr};
#endif

  // Need to be the same with other DeviceContext,
  // Eventhough eigen_device_ is not used in NPU
  // NOTE(zhiqiu): why need?
  std::unique_ptr<Eigen::DefaultDevice> eigen_device_;
  std::shared_ptr<stream::NPUStream> stream_;

  DISABLE_COPY_AND_ASSIGN(NPUDeviceContext);
};

// Currently, NPUPinnedDeviceContext is only used to data copying.
class NPUPinnedDeviceContext
    : public DeviceContext,
      public phi::TypeInfoTraits<DeviceContext, NPUPinnedDeviceContext> {
 public:
  NPUPinnedDeviceContext();
  explicit NPUPinnedDeviceContext(NPUPinnedPlace place);

  const Place& GetPlace() const override;

  Eigen::DefaultDevice* eigen_device() const;

  static const char* name() { return "NPUPinnedDeviceContext"; }

 private:
  NPUPinnedPlace place_;
  std::unique_ptr<Eigen::DefaultDevice> eigen_device_;
};

#endif

#if defined(PADDLE_WITH_CUDA) || defined(PADDLE_WITH_HIP)
using CUDAPinnedDeviceContext = phi::GPUPinnedContext;
#endif

#ifdef PADDLE_WITH_CUSTOM_DEVICE
using CustomDeviceContext = phi::CustomContext;
#endif

void EmplaceDeviceContexts(
    std::map<Place, std::shared_future<std::unique_ptr<DeviceContext>>>*
        place_to_device_context,
    const std::vector<phi::Place>& places,
    bool disable_setting_default_stream_for_allocator,
    int stream_priority);

using DeviceContextPool = phi::DeviceContextPool;

}  // namespace platform
}  // namespace paddle

namespace phi {

#ifdef PADDLE_WITH_IPU
template <>
struct DefaultDeviceContextType<phi::IPUPlace> {
  using TYPE = paddle::platform::IPUDeviceContext;
};
#endif

#ifdef PADDLE_WITH_MLU
template <>
struct DefaultDeviceContextType<phi::MLUPlace>;
#endif

#ifdef PADDLE_WITH_ASCEND_CL
template <>
struct DefaultDeviceContextType<phi::NPUPlace> {
  using TYPE = paddle::platform::NPUDeviceContext;
};

template <>
struct DefaultDeviceContextType<phi::NPUPinnedPlace> {
  using TYPE = paddle::platform::NPUPinnedDeviceContext;
};
#endif

#if defined(PADDLE_WITH_CUDA) || defined(PADDLE_WITH_HIP)
template <>
struct DefaultDeviceContextType<phi::GPUPinnedPlace> {
  using TYPE = paddle::platform::CUDAPinnedDeviceContext;
};
#endif

}  //  namespace phi
