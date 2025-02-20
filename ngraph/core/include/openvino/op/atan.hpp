// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <memory>

#include "openvino/op/util/unary_elementwise_arithmetic.hpp"

namespace ov {
namespace op {
namespace v0 {
/// \brief Elementwise inverse tangent (arctan) operation.
///
class OPENVINO_API Atan : public util::UnaryElementwiseArithmetic {
public:
    OPENVINO_OP("Atan", "opset1", util::UnaryElementwiseArithmetic);
    BWDCMP_RTTI_DECLARATION;
    /// \brief Constructs an arctan operation.
    Atan() = default;

    /// \brief Constructs an arctan operation.
    ///
    /// \param arg Output that produces the input tensor.<br>
    /// `[d1, ...]`
    ///
    /// Output `[d1, ...]`
    ///
    Atan(const Output<Node>& arg);

    std::shared_ptr<Node> clone_with_new_inputs(const OutputVector& new_args) const override;
    bool visit_attributes(AttributeVisitor&) override {
        return true;
    }
    bool evaluate(const HostTensorVector& outputs, const HostTensorVector& inputs) const override;
    bool has_evaluate() const override;
};
}  // namespace v0
}  // namespace op
}  // namespace ov
