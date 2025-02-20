// Copyright (C) 2019-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "cldnn/runtime/event.hpp"
#include "cldnn/runtime/stream.hpp"
#include "ocl_common.hpp"
#include "ocl_engine.hpp"

#include <memory>
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

namespace cldnn {
namespace ocl {

// Possible sync methods for kernels in stream
enum class sync_methods {
    /* Build dependency graph using events. Each kernel creates proper cl_event which is set as dependency of users
       At this moment it requires multiple retain/release calls for cl_event after each enqueueNDRange
       which is less performant comparing to the barriers version
    */
    events = 0,
    /* Enqueue barriers between dependent kernels. For example consider the following dimond dependency graph:
             kernel_0
             /      \
         kernel_1   kernel_2
             \      /
             kernel_3
       In that case we do the following:
       1. Enqueue kernel_0
       2. Enqueue barrier (ensures kernel_0 is completed)
       3. Enqueue kernel_1
       4. Enqueue kernel_2 (doesn't depend on kernel_1)
       5. Enqueue barrier (ensures kernel_1 and kernel_2 are completed)
       6. Enqueue kernel_3
    */
    barriers = 1,
    /* No explicit syncronization is needed. Applicable for in-order queue only */
    none = 2
};
class ocl_stream : public stream {
public:
    const ocl_queue_type& get_cl_queue() const { return _command_queue; }

    explicit ocl_stream(const ocl_engine& engine);
    ocl_stream(ocl_stream&& other)
        : stream(other._engine.configuration().queue_type)
        , _engine(other._engine)
        , _command_queue(other._command_queue)
        , _queue_counter(other._queue_counter.load())
        , _last_barrier(other._last_barrier.load())
        , _last_barrier_ev(other._last_barrier_ev)
        , sync_method(other.sync_method) {}

    ~ocl_stream() = default;

    void flush() const override;
    void finish() const override;

    void set_arguments(kernel& kernel, const kernel_arguments_desc& args_desc, const kernel_arguments_data& args) override;
    event::ptr enqueue_kernel(kernel& kernel,
                              const kernel_arguments_desc& args_desc,
                              const kernel_arguments_data& args,
                              std::vector<event::ptr> const& deps,
                              bool is_output = false) override;
    event::ptr enqueue_marker(std::vector<event::ptr> const& deps, bool is_output) override;
    event::ptr group_events(std::vector<event::ptr> const& deps) override;
    void wait_for_events(const std::vector<event::ptr>& events) override;
    void enqueue_barrier() override;
    event::ptr create_user_event(bool set) override;
    event::ptr create_base_event() override;

    const cl::UsmHelper& get_usm_helper() const { return _engine.get_usm_helper(); }

#ifdef ENABLE_ONEDNN_FOR_GPU
    dnnl::stream& get_onednn_stream() override;
#endif

private:
    void sync_events(std::vector<event::ptr> const& deps, bool is_output = false);

    const ocl_engine& _engine;
    ocl_queue_type _command_queue;
    std::atomic<uint64_t> _queue_counter{0};
    std::atomic<uint64_t> _last_barrier{0};
    cl::Event _last_barrier_ev;

    sync_methods sync_method;

#ifdef ENABLE_ONEDNN_FOR_GPU
    std::shared_ptr<dnnl::stream> _onednn_stream = nullptr;
#endif
};

}  // namespace ocl
}  // namespace cldnn
