// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "openvino/op/util/unary_elementwise_arithmetic.hpp"

namespace ov {
namespace op {
namespace v0 {
/// \brief Elementwise ceiling operation.
class OPENVINO_API Ceiling : public util::UnaryElementwiseArithmetic {
public:
    OPENVINO_OP("Ceiling", "opset1", util::UnaryElementwiseArithmetic);
    BWDCMP_RTTI_DECLARATION;
    /// \brief Constructs a ceiling operation.
    Ceiling() = default;
    /// \brief Constructs a ceiling operation.
    ///
    /// \param arg Node that produces the input tensor.
    Ceiling(const Output<Node>& arg);

    bool visit_attributes(AttributeVisitor&) override {
        return true;
    }
    std::shared_ptr<Node> clone_with_new_inputs(const OutputVector& new_args) const override;
    bool evaluate(const HostTensorVector& outputs, const HostTensorVector& inputs) const override;
    bool has_evaluate() const override;
};
}  // namespace v0
}  // namespace op
}  // namespace ov
