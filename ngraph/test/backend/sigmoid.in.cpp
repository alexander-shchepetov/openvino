// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

// clang-format off
#ifdef ${BACKEND_NAME}_FLOAT_TOLERANCE_BITS
#define DEFAULT_FLOAT_TOLERANCE_BITS ${BACKEND_NAME}_FLOAT_TOLERANCE_BITS
#endif

#ifdef ${BACKEND_NAME}_DOUBLE_TOLERANCE_BITS
#define DEFAULT_DOUBLE_TOLERANCE_BITS ${BACKEND_NAME}_DOUBLE_TOLERANCE_BITS
#endif
// clang-format on

#include "gtest/gtest.h"
#include "runtime/backend.hpp"
#include "ngraph/runtime/tensor.hpp"
#include "ngraph/ngraph.hpp"
#include "util/all_close.hpp"
#include "util/all_close_f.hpp"
#include "util/ndarray.hpp"
#include "util/test_control.hpp"
#include "engines_util/execute_tools.hpp"

using namespace std;
using namespace ngraph;

static string s_manifest = "${MANIFEST}";

NGRAPH_TEST(${BACKEND_NAME}, sigmoid_n1c1h2w2) {
    auto input = make_shared<op::Parameter>(element::f32, Shape{1, 1, 2, 2});
    auto sigmoid_node = make_shared<op::Sigmoid>(input);
    auto func = make_shared<Function>(sigmoid_node, ParameterVector{input});

    auto backend = runtime::Backend::create("${BACKEND_NAME}");

    shared_ptr<runtime::Tensor> a = backend->create_tensor(element::f32, input->get_shape());
    shared_ptr<runtime::Tensor> result = backend->create_tensor(element::f32, input->get_shape());

    float x1 = 1.0f;
    float x2 = 4.0f;
    float sigma1 = 1.0f / (1.0f + std::exp(-x1));
    float sigma2 = 1.0f / (1.0f + std::exp(-x2));

    vector<float> dataA{x1, x2, x1, x2};
    copy_data(a, dataA);

    auto handle = backend->compile(func);
    handle->call_with_validate({result}, {a});
    vector<float> expected{sigma1, sigma2, sigma1, sigma2};
    EXPECT_TRUE(test::all_close_f(read_vector<float>(result), expected));
}

NGRAPH_TEST(${BACKEND_NAME}, sigmoid_n1c1h4) {
    auto input = make_shared<op::Parameter>(element::f32, Shape{1, 1, 4});
    auto sigmoid_node = make_shared<op::Sigmoid>(input);
    auto func = make_shared<Function>(sigmoid_node, ParameterVector{input});

    auto backend = runtime::Backend::create("${BACKEND_NAME}");

    shared_ptr<runtime::Tensor> a = backend->create_tensor(element::f32, input->get_shape());
    shared_ptr<runtime::Tensor> result = backend->create_tensor(element::f32, input->get_shape());

    float x1 = 1.0f;
    float x2 = 4.0f;
    float sigma1 = 1.0f / (1.0f + std::exp(-x1));
    float sigma2 = 1.0f / (1.0f + std::exp(-x2));

    vector<float> dataA{x1, x2, x1, x2};
    copy_data(a, dataA);

    auto handle = backend->compile(func);
    handle->call_with_validate({result}, {a});
    vector<float> expected{sigma1, sigma2, sigma1, sigma2};
    EXPECT_TRUE(test::all_close_f(read_vector<float>(result), expected));
}
