// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cfloat>

#include "ngraph/ngraph.hpp"

namespace ov {
struct MemBandwidthPressure {
    float max_mem_tolerance = UNKNOWN;
    float ratio_compute_convs = 0;
    float ratio_mem_limited_convs = 0;
    float ratio_compute_deconvs = 0;

    static constexpr float UNKNOWN = FLT_MAX;
    static constexpr float ALL = 1.0f;
    static constexpr float NONE = 0.0f;
    static constexpr float LIMITED = 0.5f;  // conservatively assume 1/2 utilization of the cache
};

MemBandwidthPressure MemBandwidthPressureTolerance(
    const std::shared_ptr<ngraph::Function> nGraphFunc,
    const float L2_cache_size,
    const float L3_cache_size,
    const float memThresholdAssumeLimited = MemBandwidthPressure::LIMITED) {
    int total_convs = 0, mem_limited_convs = 0, compute_convs = 0, total_gemms = 0, mem_limited_gemms = 0,
        total_deconvs = 0, compute_deconvs = 0, mem_limited_deconvs = 0;
    auto memLimitedFactor = [&](int size_data_moved, int datatype_size = 4) -> float {
        return (L2_cache_size * 1.0f /*util factor, tbd */
                / (size_data_moved * datatype_size));
    };
    auto isLowPrecision = [&](ngraph::element::Type type) -> bool {
        return (type == ngraph::element::i8) || (type == ngraph::element::u8);
    };
    auto isHalfPrecision = [&](ngraph::element::Type type) -> bool {
        return (type == ngraph::element::bf16) || (type == ngraph::element::f16);
    };

    float worst_case = MemBandwidthPressure::UNKNOWN;
    // Traverse nGraph Function in topological order
    for (auto& node : nGraphFunc->get_ordered_ops()) {
        const auto node_name = node->get_type_info().name;
        if (std::strcmp("MatMul", node_name) && std::strcmp("Convolution", node_name) &&
            std::strcmp("ConvolutionBackpropData", node_name)) {
            if (!std::strcmp("GRUSequence", node_name) || !std::strcmp("TensorIterator", node_name)) {
                MemBandwidthPressure res;
                res.max_mem_tolerance = MemBandwidthPressure::UNKNOWN;
                return res;
            }
            continue;
        }
        auto type1 = node->input_value(1).get_element_type();  // weights
        const bool isINT8 = isLowPrecision(type1);
        const bool isBF16orFP16 = isHalfPrecision(type1);
        const int data_type_size = isINT8 ? 1 : isBF16orFP16 ? 2 : 4;

        int dataSizeInput = 0, dataSizeOutput = 0;
        if (!std::strcmp("MatMul", node_name)) {
            const auto input0 = node->input(0);
            const auto input1 = node->input(1);
            const auto output = node->output(0);
            // Check that input and output shape a fully defined (not dynamic)
            if (input0.get_partial_shape().is_static() && input1.get_partial_shape().is_static() &&
                output.get_partial_shape().is_static()) {
                const auto& shapeInput0 = input0.get_shape();
                const auto& shapeInput1 = input1.get_shape();
                const auto non_const = !get_constant_from_source(node->input_value(1));
                const auto& shapeOutput = output.get_shape();
                const auto dataSizeInput0 =
                    std::accumulate(shapeInput0.begin(), shapeInput0.end(), 1, std::multiplies<int>());
                const auto dataSizeInput1 =
                    std::accumulate(shapeInput1.begin(), shapeInput1.end(), 1, std::multiplies<int>());
                dataSizeOutput = std::accumulate(shapeOutput.begin(), shapeOutput.end(), 1, std::multiplies<int>());
                const auto total_data = dataSizeInput0 + non_const * dataSizeInput1 + dataSizeOutput;
                total_gemms++;
                const auto factor = memLimitedFactor(total_data, data_type_size);
                mem_limited_gemms += factor < memThresholdAssumeLimited;
                worst_case = std::min(factor, worst_case);
            }
        } else if (!std::strcmp("Convolution", node_name)) {
            // Check that input and output shape a fully defined (not dynamic)
            const auto input = node->input(0);
            const auto output = node->output(0);
            const auto kernels = node->input(1);
            const auto& shape = kernels.get_shape();
            total_convs++;
            if (shape.size() >= 4 /* conventional 2D/3D conv */ && shape[2] >= 3 && shape[3] >= 3) {
                compute_convs++;
                continue;
            }
            if (input.get_partial_shape().is_static() && output.get_partial_shape().is_static()) {
                const auto& shapeInput = input.get_shape();
                const auto& shapeOutput = output.get_shape();
                if (shapeInput.size() > 4 /*5D*/ && isINT8) {
                    compute_convs++;
                    continue;
                }
                dataSizeInput = std::accumulate(shapeInput.begin(), shapeInput.end(), 1, std::multiplies<int>());
                dataSizeOutput = std::accumulate(shapeOutput.begin(), shapeOutput.end(), 1, std::multiplies<int>());
                const auto factor = memLimitedFactor(dataSizeInput + dataSizeOutput, data_type_size);
                mem_limited_convs += factor < memThresholdAssumeLimited;
                worst_case = std::min(factor, worst_case);
            }
        } else if (!std::strcmp("ConvolutionBackpropData", node_name)) {
            const auto input = node->input(0);
            const auto output = node->output(0);
            total_deconvs++;

            // Check that input and output shape a fully defined (not dynamic)
            if (input.get_partial_shape().is_static() && output.get_partial_shape().is_static()) {
                const auto shapeInput = input.get_shape();
                const auto shapeOutput = output.get_shape();
                if (shapeInput.size() > 4 /*5D*/ && isINT8) {
                    compute_deconvs++;
                    continue;
                }
                dataSizeInput = std::accumulate(shapeInput.begin(), shapeInput.end(), 1, std::multiplies<int>());
                dataSizeOutput = std::accumulate(shapeOutput.begin(), shapeOutput.end(), 1, std::multiplies<int>());
                const auto factor = memLimitedFactor(dataSizeInput + dataSizeOutput, data_type_size);
                mem_limited_deconvs += factor < memThresholdAssumeLimited;
                worst_case = std::min(factor, worst_case);
            }
        }
    }
    MemBandwidthPressure res;
    res.max_mem_tolerance = worst_case;
    res.ratio_mem_limited_convs = total_convs ? static_cast<float>(mem_limited_convs) / total_convs : 0;
    res.ratio_compute_convs = total_convs ? static_cast<float>(compute_convs) / total_convs : 0;
    res.ratio_compute_deconvs = total_deconvs ? static_cast<float>(compute_deconvs) / total_deconvs : 0;
    return res;
}

}  // namespace ov