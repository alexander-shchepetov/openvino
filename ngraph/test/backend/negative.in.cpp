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
#include "ngraph/ngraph.hpp"
#include "engines_util/test_engines.hpp"
#include "engines_util/test_case.hpp"
#include "util/test_control.hpp"

using namespace std;
using namespace ngraph;

static string s_manifest = "${MANIFEST}";
using TestEngine = test::ENGINE_CLASS_NAME(${BACKEND_NAME});

NGRAPH_TEST(${BACKEND_NAME}, negative) {
    Shape shape{2, 3};
    auto A = make_shared<op::Parameter>(element::f32, shape);
    auto f = make_shared<Function>(make_shared<op::Negative>(A), ParameterVector{A});

    std::vector<float> a{1, -2, 0, -4.75f, 8.75f, -8.75f};

    auto test_case = test::TestCase<TestEngine>(f);
    test_case.add_input<float>(a);
    test_case.add_expected_output<float>(shape, {-1, 2, 0, 4.75f, -8.75f, 8.75f});
    test_case.run(MIN_FLOAT_TOLERANCE_BITS);
}

NGRAPH_TEST(${BACKEND_NAME}, negative_i32) {
    auto shape_a = Shape{2, 5};
    auto A = make_shared<op::Parameter>(element::i32, shape_a);
    auto negative = make_shared<op::Negative>(A);
    auto shape_rt = Shape{2, 5};
    auto f = make_shared<Function>(negative, ParameterVector{A});

    std::vector<int32_t> a{1, 8, -8, 17, -2, 1, 8, -8, 17, -1};
    std::vector<int32_t> r{-1, -8, 8, -17, 2, -1, -8, 8, -17, 1};

    auto test_case = test::TestCase<TestEngine>(f);
    test_case.add_input<int32_t>(shape_a, a);
    test_case.add_expected_output<int32_t>(shape_rt, r);
    test_case.run();
}

NGRAPH_TEST(${BACKEND_NAME}, negative_f32) {
    auto shape_a = Shape{2, 5};
    auto A = make_shared<op::Parameter>(element::f32, shape_a);
    auto negative = make_shared<op::Negative>(A);
    auto shape_rt = Shape{2, 5};
    auto f = make_shared<Function>(negative, ParameterVector{A});

    std::vector<float> a{1.35f, 8.76f, -8.0f, 17.234f, -2.121f, 1.0f, 8.7f, -8.92f, 17.0f, -1.0f};

    auto test_case = test::TestCase<TestEngine>(f);
    test_case.add_input<float>(shape_a, a);
    test_case.add_expected_output<float>(shape_rt,
                                         {-1.35f, -8.76f, 8.0f, -17.234f, 2.121f, -1.0f, -8.7f, 8.92f, -17.0f, 1.0f});
    test_case.run();
}
