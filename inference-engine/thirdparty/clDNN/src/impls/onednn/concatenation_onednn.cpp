// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "concatenation_inst.h"
#include "eltwise_inst.h"
#include "quantize_inst.h"
#include "primitive_onednn_base.h"
#include "impls/implementation_map.hpp"

#include "kernel_selector_common.h"

#include <oneapi/dnnl/dnnl.hpp>

#include <algorithm>
#include <memory>
namespace cldnn {
namespace onednn {



namespace detail {

attach_concatenation_onednn::attach_concatenation_onednn() {
}

}  // namespace detail
}  // namespace onednn
}  // namespace cldnn
