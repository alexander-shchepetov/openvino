// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <memory>
#include <vector>

#include "openvino/pass/pass.hpp"

namespace ov {
namespace pass {
/**
 * @brief The transformation finds all TensorIterator/Loop layers in the network,
 * processes all back edges that describe a connection between Result and Parameter
 * of the TensorIterator/Loop bodies,and inserts ReadValue and Assign layers at the
 * input and output corresponding to this back edge.
 * Supported platforms: CPU, GNA.
 *
 * The example below describes the changes made by the transformation
 *  [] - TensorIterator body
 *  () - new layer
 *  BE - back-edge
 *
 *  before applying the transformation:
 *  -> input1[BE_1 -> Parameter -> Layers ... -> Result  -> BE_1 ]output1->
 *
 *  after applying the transformation:
 *  ->(ReadValue)-> input1[BE_1 ->Parameter->Layers ...->Result->BE_1]output1 ->(Assign)
 *                                                                      \
 *                                                                       ->...
 * After applying the transformation, the resulting network can be inferred
 * step by step, the states will store between inferences.
 */
class OPENVINO_API LowLatency2 : public FunctionPass {
public:
    OPENVINO_RTTI("LowLatency2");

    explicit LowLatency2(bool use_const_initializer = true) : m_use_const_initializer(use_const_initializer) {}

    bool run_on_function(std::shared_ptr<ngraph::Function> f) override;

private:
    bool m_use_const_initializer;
};
}  // namespace pass
}  // namespace ov
