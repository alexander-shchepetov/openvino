// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "openvino/op/util/unary_elementwise_arithmetic.hpp"

namespace ov {
namespace op {
namespace v0 {
// clang-format off
/// \brief Elementwise square root operation.
///
/// ## Inputs
///
/// |       | Type                              | Description                                     |
/// | ----- | --------------------------------- | ----------------------------------------------- |
/// | `arg` | \f$N[d_1,\dots,d_n]~(n \geq 0)\f$ | A tensor of any shape and numeric element type. |
///
/// ## Output
///
/// | Type                   | Description                                                                           |
/// | ---------------------- | ------------------------------------------------------------------------------------- |
/// | \f$N[d_1,\dots,d_n]\f$ | The tensor \f$T\f$, where \f$T[i_1,\dots,i_n] = \sqrt{\texttt{arg}[i_1,\dots,i_n]}\f$ |
// clang-format on
class OPENVINO_API Sqrt : public util::UnaryElementwiseArithmetic {
public:
    OPENVINO_OP("Sqrt", "opset1", util::UnaryElementwiseArithmetic);
    BWDCMP_RTTI_DECLARATION;

    /// \brief Constructs a square operation.
    ///
    /// \param arg Node that produces the input tensor.
    Sqrt(const Output<Node>& arg);
    Sqrt() = default;

    bool visit_attributes(AttributeVisitor& visitor) override;
    std::shared_ptr<Node> clone_with_new_inputs(const OutputVector& new_args) const override;
    bool evaluate(const HostTensorVector& outputs, const HostTensorVector& inputs) const override;
    bool has_evaluate() const override;
};
}  // namespace v0
}  // namespace op
}  // namespace ov
