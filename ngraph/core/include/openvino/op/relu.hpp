// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <memory>

#include "openvino/op/util/unary_elementwise_arithmetic.hpp"

namespace ov {
namespace op {
namespace v0 {
/// \brief Elementwise Relu operation.
///
class OPENVINO_API Relu : public util::UnaryElementwiseArithmetic {
public:
    OPENVINO_OP("Relu", "opset1", util::UnaryElementwiseArithmetic);
    BWDCMP_RTTI_DECLARATION;
    Relu() = default;
    /// \brief Constructs a Relu operation.
    ///
    /// \param arg Node that produces the input tensor.
    Relu(const Output<ngraph::Node>& arg);

    std::shared_ptr<Node> clone_with_new_inputs(const OutputVector& new_args) const override;

    bool evaluate(const HostTensorVector& outputs, const HostTensorVector& inputs) const override;
    bool has_evaluate() const override;
    bool visit_attributes(AttributeVisitor& visitor) override;
};
}  // namespace v0
}  // namespace op
}  // namespace ov
