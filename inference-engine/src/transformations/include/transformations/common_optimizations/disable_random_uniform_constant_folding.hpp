// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <memory>
#include <ngraph/pass/graph_rewrite.hpp>

namespace ngraph {
namespace pass {

class DisableRandomUniformConstantFolding;

}  // namespace pass
}  // namespace ngraph

/**
 * @ingroup ie_transformation_common_api
 * @brief Disables ConstantFolding for RandomUniform operation. It is required as RandomUniform
 * should generate new sequence each run.
 */
class ngraph::pass::DisableRandomUniformConstantFolding : public ngraph::pass::MatcherPass {
public:
    NGRAPH_RTTI_DECLARATION;
    DisableRandomUniformConstantFolding();
};
