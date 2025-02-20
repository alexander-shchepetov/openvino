// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "openvino/runtime/core.hpp"

namespace ov {
namespace test {

class PluginCache {
public:
    std::shared_ptr<ov::runtime::Core> core(const std::string& deviceToCheck = std::string());

    static PluginCache& get();

    void reset();

    PluginCache(const PluginCache&) = delete;
    PluginCache& operator=(const PluginCache&) = delete;

private:
    PluginCache();
    ~PluginCache() = default;

    std::mutex g_mtx;
    std::shared_ptr<ov::runtime::Core> ov_core;
};
}  // namespace test
}  // namespace ov
