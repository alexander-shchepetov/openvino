// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "engines_util/execute_tools.hpp"
#include "gtest/gtest.h"
#include "ngraph/ngraph.hpp"
#include "ngraph/runtime/tensor.hpp"
#include "runtime/backend.hpp"
#include "util/all_close.hpp"
#include "util/all_close_f.hpp"
#include "util/ndarray.hpp"
#include "util/test_control.hpp"

using namespace std;
using namespace ngraph;

static string s_manifest = "${MANIFEST}";

NGRAPH_TEST(${BACKEND_NAME}, relu_2Dfprop) {
    auto shape_a = Shape{2, 5};
    auto A = make_shared<op::Parameter>(element::f32, shape_a);
    auto relu = make_shared<op::Relu>(A);
    auto shape_rt = Shape{2, 5};
    auto f = make_shared<Function>(relu, ParameterVector{A});

    auto backend = runtime::Backend::create("${BACKEND_NAME}");

    auto a = backend->create_tensor(element::f32, shape_a);
    copy_data(a, vector<float>{1, 8, -8, 17, -0.5, 1, 8, -8, 17, -0.5});
    auto result = backend->create_tensor(element::f32, shape_rt);
    vector<float> expected{1, 8, 0, 17, 0, 1, 8, 0, 17, 0};

    auto handle = backend->compile(f);
    handle->call_with_validate({result}, {a});
    EXPECT_TRUE(test::all_close_f(read_vector<float>(result), expected, MIN_FLOAT_TOLERANCE_BITS));
}

NGRAPH_TEST(${BACKEND_NAME}, relu_2Dfprop_i32) {
    auto shape_a = Shape{2, 5};
    auto A = make_shared<op::Parameter>(element::i32, shape_a);
    auto relu = make_shared<op::Relu>(A);
    auto shape_rt = Shape{2, 5};
    auto f = make_shared<Function>(relu, ParameterVector{A});

    auto backend = runtime::Backend::create("${BACKEND_NAME}");

    auto a = backend->create_tensor(element::i32, shape_a);
    copy_data(a, vector<int32_t>{1, 8, -8, 17, -2, 1, 8, -8, 17, -1});
    auto result = backend->create_tensor(element::i32, shape_rt);
    vector<int32_t> expected{1, 8, 0, 17, 0, 1, 8, 0, 17, 0};

    auto handle = backend->compile(f);
    handle->call_with_validate({result}, {a});
    EXPECT_EQ(expected, read_vector<int32_t>(result));
}

NGRAPH_TEST(${BACKEND_NAME}, relu_4Dfprop) {
    auto shape_a = Shape{2, 2, 2, 2};
    auto A = make_shared<op::Parameter>(element::f32, shape_a);
    auto relu = make_shared<op::Relu>(A);
    auto shape_rt = Shape{2, 2, 2, 2};
    auto f = make_shared<Function>(relu, ParameterVector{A});

    auto backend = runtime::Backend::create("${BACKEND_NAME}");

    auto a = backend->create_tensor(element::f32, shape_a);
    copy_data(a, vector<float>{1, 8, -8, 17, -0.5, 1, 8, -8, 17, -0.5, 1, 8, -8, 17, -0.5, 1});
    auto result = backend->create_tensor(element::f32, shape_rt);
    vector<float> expected{1, 8, 0, 17, 0, 1, 8, 0, 17, 0, 1, 8, 0, 17, 0, 1};

    auto handle = backend->compile(f);
    handle->call_with_validate({result}, {a});
    EXPECT_TRUE(test::all_close_f(read_vector<float>(result), expected, MIN_FLOAT_TOLERANCE_BITS));
}

NGRAPH_TEST(${BACKEND_NAME}, fuse_max_with_constant_zero_input_as_relu) {
    auto shape_a = Shape{2, 5};
    auto A = op::Constant::create(element::f32, shape_a, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    auto B = make_shared<op::Parameter>(element::f32, shape_a);
    auto max = make_shared<op::v1::Maximum>(A, B);
    auto shape_rt = Shape{2, 5};
    auto f = make_shared<Function>(max, ParameterVector{B});

    auto backend = runtime::Backend::create("${BACKEND_NAME}");

    auto b = backend->create_tensor(element::f32, shape_a);
    copy_data(b, vector<float>{1, 8, -8, 17, -0.5, 1, 8, -8, 17, -0.5});
    auto result = backend->create_tensor(element::f32, shape_rt);
    vector<float> expected{1, 8, 0, 17, 0, 1, 8, 0, 17, 0};

    auto handle = backend->compile(f);
    handle->call_with_validate({result}, {b});
    EXPECT_TRUE(test::all_close_f(read_vector<float>(result), expected, MIN_FLOAT_TOLERANCE_BITS));
}
