// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "layer_transformation.hpp"

#include <sstream>
#include <memory>
#include <vector>
#include <gtest/gtest.h>

#include <low_precision/rt_info/precision_preserved_attribute.hpp>
#include <low_precision/rt_info/intervals_alignment_attribute.hpp>
#include <low_precision/rt_info/quantization_alignment_attribute.hpp>

#include <low_precision/common/operation_precision_restriction.hpp>
#include <low_precision/common/operation_per_tensor_quantization_restriction.hpp>
#include <low_precision/concat.hpp>
#include <low_precision/fake_quantize_decomposition.hpp>
#include <low_precision/fuse_subtract_to_fake_quantize.hpp>
#include <low_precision/fuse_multiply_to_fake_quantize.hpp>

#include "common_test_utils/ngraph_test_utils.hpp"
#include "lpt_ngraph_functions/concat_function.hpp"
#include "lpt_ngraph_functions/common/builders.hpp"
#include "lpt_ngraph_functions/common/fake_quantize_on_data.hpp"
#include "simple_low_precision_transformer.hpp"

using namespace testing;
using namespace ngraph;
using namespace ngraph::pass;
using namespace ngraph::builder::subgraph;

namespace {

class ConcatTransformationActualValues {
public:
    ngraph::builder::subgraph::FakeQuantizeOnDataWithConstant fakeQuantize1;
    ngraph::builder::subgraph::DequantizationOperations::Convert convert1;
    ngraph::builder::subgraph::DequantizationOperations dequantization1;
    ngraph::builder::subgraph::FakeQuantizeOnDataWithConstant fakeQuantize2;
    ngraph::builder::subgraph::DequantizationOperations::Convert convert2;
    ngraph::builder::subgraph::DequantizationOperations dequantization2;
};

inline std::ostream& operator<<(std::ostream& out, const ConcatTransformationActualValues& values) {
    return out << "_" <<
        values.fakeQuantize1 << "_" <<
        values.convert1.outPrecision << "_" <<
        values.dequantization1 << "_" <<
        values.fakeQuantize2 << "_" <<
        values.convert2.outPrecision << "_" <<
        values.dequantization2;
}

class ConcatTransformationResultValues {
public:
    ngraph::builder::subgraph::FakeQuantizeOnDataWithConstant fakeQuantize1;
    ngraph::builder::subgraph::DequantizationOperations::Convert convert1;
    ngraph::builder::subgraph::DequantizationOperations dequantization1;
    ngraph::builder::subgraph::FakeQuantizeOnDataWithConstant fakeQuantize2;
    ngraph::builder::subgraph::DequantizationOperations::Convert convert2;
    ngraph::builder::subgraph::DequantizationOperations dequantization2;
    ngraph::element::Type precisionAfterOperation;
    ngraph::builder::subgraph::DequantizationOperations dequantizationAfter;
};

inline std::ostream& operator<<(std::ostream& out, const ConcatTransformationResultValues& values) {
    return out << "_" <<
        values.fakeQuantize1 << "_" <<
        values.convert1.outPrecision << "_" <<
        values.dequantization1 << "_" <<
        values.fakeQuantize2 << "_" <<
        values.convert2.outPrecision << "_" <<
        values.dequantization2 << "_" <<
        values.dequantizationAfter;
}

class ConcatTransformationTestValues {
public:
    ConcatTransformationTestValues() = default;
    ConcatTransformationTestValues(
        const TestTransformationParams& params,
        const bool multiChannels,
        const  std::int64_t axis,
        const ConcatTransformationActualValues& actual,
        const ConcatTransformationResultValues& result,
        const bool addNotPrecisionPreservedOperation = false,
        const bool checkIntervalsAlignmentAttributes = true) :
        params(params),
        multiChannels(multiChannels),
        axis(axis),
        actual(actual),
        result(result),
        addNotPrecisionPreservedOperation(addNotPrecisionPreservedOperation),
        checkIntervalsAlignmentAttributes(checkIntervalsAlignmentAttributes) {}

    TestTransformationParams params;
    bool multiChannels;
    std::int64_t axis;
    ConcatTransformationActualValues actual;
    ConcatTransformationResultValues result;
    // add not precision preserved operation to set output precision for FakeQuantize
    // don't set to 'true' by default to keep test cases with tested operation as output
    bool addNotPrecisionPreservedOperation;
    bool checkIntervalsAlignmentAttributes;
};

inline std::ostream& operator<<(std::ostream& out, const ConcatTransformationTestValues& values) {
    return out << "_" << values.multiChannels << "_" << values.actual << "_" << values.result;
}

typedef std::tuple <
    ngraph::element::Type,
    ngraph::PartialShape,
    ConcatTransformationTestValues
> ConcatTransformationParams;

class ConcatTransformation : public LayerTransformation, public testing::WithParamInterface<ConcatTransformationParams> {
public:
    void SetUp() override {
        const ngraph::element::Type precision = std::get<0>(GetParam());
        const ngraph::PartialShape shape = std::get<1>(GetParam());
        ConcatTransformationTestValues testValues = std::get<2>(GetParam());

        // dequantization output precision depends on input precision
        // to avoid huge amount of tests cases let's define dequantization output precision as input precision
        if (!testValues.actual.dequantization1.multiply.empty()) {
            testValues.actual.dequantization1.multiply.outPrecision = precision;
        }
        if (!testValues.actual.dequantization2.multiply.empty()) {
            testValues.actual.dequantization2.multiply.outPrecision = precision;
        }

        actualFunction = ngraph::builder::subgraph::ConcatFunction::get(
            precision,
            shape,
            testValues.actual.fakeQuantize1,
            testValues.actual.convert1,
            testValues.actual.dequantization1,
            testValues.actual.fakeQuantize2,
            testValues.actual.convert2,
            testValues.actual.dequantization2,
            {},
            ngraph::element::undefined,
            {},
            testValues.axis,
            testValues.addNotPrecisionPreservedOperation);

        auto supportedPrecisionsOnActivation = std::vector<ngraph::pass::low_precision::OperationPrecisionRestriction>({
            ngraph::pass::low_precision::OperationPrecisionRestriction::create<ngraph::opset1::AvgPool>({{0, testValues.params.precisionsOnActivations}})
        });

        auto quantizationRestrictions = testValues.multiChannels ?
            std::vector<ngraph::pass::low_precision::OperationPerTensorQuantizationRestriction>() :
            std::vector<ngraph::pass::low_precision::OperationPerTensorQuantizationRestriction>({
                ngraph::pass::low_precision::OperationPerTensorQuantizationRestriction::create<ngraph::opset1::AvgPool>()
            });

        const auto params = TestTransformationParams::toParams(testValues.params);
        SimpleLowPrecisionTransformer transformer(supportedPrecisionsOnActivation, quantizationRestrictions);
        transformer.commonGraphRewrite->add_matcher<ngraph::pass::low_precision::FakeQuantizeDecompositionTransformation>(params);
        transformer.commonGraphRewrite->add_matcher<ngraph::pass::low_precision::ConcatTransformation>(params);
        transformer.transform(actualFunction);

        {
            ngraph::pass::Manager standaloneCleanupManager;
            standaloneCleanupManager.register_pass<ngraph::pass::low_precision::FuseSubtractToFakeQuantizeTransformation>();
            standaloneCleanupManager.run_passes(actualFunction);
        }

        {
            ngraph::pass::Manager standaloneCleanupManager;
            standaloneCleanupManager.register_pass<ngraph::pass::low_precision::FuseMultiplyToFakeQuantizeTransformation>();
            standaloneCleanupManager.run_passes(actualFunction);
        }

        // dequantization output precision depends on input precision
        // to avoid huge amount of tests cases let's define dequantization output precision as input precision
        if (!testValues.result.dequantizationAfter.multiply.empty()) {
            testValues.result.dequantizationAfter.multiply.outPrecision = precision;
        }

        if (!testValues.params.updatePrecisions &&
            (precision == ngraph::element::f32) &&
            !testValues.result.dequantizationAfter.convert.empty()) {
            testValues.result.dequantizationAfter.convert = {};
        }

        IntervalsAlignmentSharedValue::Interval interval{-1.28f, 2.55f};

        referenceFunction = ngraph::builder::subgraph::ConcatFunction::get(
            precision,
            shape,
            testValues.result.fakeQuantize1,
            testValues.result.convert1,
            testValues.result.dequantization1,
            testValues.result.fakeQuantize2,
            testValues.result.convert2,
            testValues.result.dequantization2,
            {
                make_shared_attribute_ptr<PrecisionPreservedAttribute>(true),
                make_shared_attribute_ptr<IntervalsAlignmentAttribute>(interval, 256),
                make_shared_attribute_ptr<QuantizationAlignmentAttribute>(false)
            },
            testValues.result.precisionAfterOperation,
            testValues.result.dequantizationAfter,
            testValues.axis,
            testValues.addNotPrecisionPreservedOperation);
    }

    static std::string getTestCaseName(testing::TestParamInfo<ConcatTransformationParams> obj) {
        const ngraph::element::Type precision = std::get<0>(obj.param);
        const ngraph::PartialShape shape = std::get<1>(obj.param);
        const ConcatTransformationTestValues testValues = std::get<2>(obj.param);

        std::ostringstream result;
        result <<
            LayerTransformation::getTestCaseNameByParams(precision, shape, testValues.params) << "_" <<
            (testValues.multiChannels ? "multiChannels_" : "notMultiChannels_") <<
            "axis_" << testValues.axis << "_" <<
            testValues.actual << "_" <<
            testValues.result << "_";
        return result.str();
    }
};

TEST_P(ConcatTransformation, CompareFunctions) {
    actualFunction->validate_nodes_and_infer_types();
    auto res = compare_functions(referenceFunction, actualFunction, true, true, false, true, false);
    ASSERT_TRUE(res.first) << res.second;

    ConcatTransformationTestValues testValues = std::get<2>(GetParam());
    const auto actualFakeQuantizes = LayerTransformation::get<opset1::FakeQuantize>(actualFunction);
    if (testValues.axis == 1) {
        ASSERT_TRUE(checkIfOutputAttributesSharedValuesAreTheSame<std::shared_ptr<PrecisionsAttribute>>(actualFakeQuantizes)) <<
            "PrecisionsAttribute are not the same";

        if (testValues.checkIntervalsAlignmentAttributes) {
            auto operations = LayerTransformation::get<opset1::Concat>(actualFunction);
            operations.insert(operations.end(), actualFakeQuantizes.begin(), actualFakeQuantizes.end());
            ASSERT_TRUE(checkIfAttributesSharedValuesAreTheSame<std::shared_ptr<IntervalsAlignmentAttribute>>(operations)) <<
                "IntervalsAlignmentAttribute are not the same";
        }
    }
}

const std::vector<ngraph::element::Type> precisions = {
    ngraph::element::f32,
    //ngraph::element::f16
};

namespace testValues1 {
const std::vector<ngraph::PartialShape> shapes = {
    { 1, 3, 9, 9 },
    { 4, 3, 9, 9 },
    { Dimension::dynamic(), 3, Dimension::dynamic(), Dimension::dynamic() }
};

const std::vector<ConcatTransformationTestValues> testValues = {
    // U8: concat: levels less then threshold is ignored, function is not transformed
    // U8: concat: per-channel quantization: function is transformed
    {
        LayerTransformation::createParamsU8I8(),
        true,
        1,
        {
            { 256ul, {}, {0.f}, {2550.f}, {0.f}, {2550.f} },
            {},
            {},
            { 256ul, {}, {0.f}, {0.1f}, {0.f}, {0.1f} }
        },
        {
            {
                256ul, {}, {0.f}, {2550.f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {0.f}, {0.1f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {}, {{ 10.f, 10.f, 10.f, 0.000392157f, 0.000392157f, 0.000392157f }} },
        },
        true
    },
    // right branch is not quantized
    {
        LayerTransformation::createParamsU8I8(),
        false,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            {}
        },
        {
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f}, ngraph::element::f32,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {},
            {},
            {},
            ngraph::element::f32,
        }
    },
    // left branch is not quantized
    {
        LayerTransformation::createParamsU8I8(),
        false,
        1,
        {
            {},
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} }
        },
        {
            {},
            {},
            {},
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f}, ngraph::element::f32,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::f32,
        }
    },
    // U8: concat
    {
        LayerTransformation::createParamsU8I8(),
        false,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} }
        },
        {
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {}, { 0.01f } },
        }
    },
    // U8: concat
    {
        LayerTransformation::createParamsU8I8(),
        false,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f} },
            { ngraph::element::u8 },
            {
                { element::f32 },
                {},
                { 0.01f }
            },
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f} },
            { ngraph::element::u8 },
            {
                { element::f32 },
                {},
                { 0.01f }
            },
        },
        {
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {}, { 0.01f } }
        }
    },
    // U8: concat
    {
        LayerTransformation::createParamsU8I8(),
        true,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f} },
            { ngraph::element::u8 },
            {
                { element::f32 },
                {},
                { 0.01f }
            },
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f} },
            { ngraph::element::u8 },
            {
                { element::f32 },
                {},
                { 0.01f }
            },
        },
        {
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {}, { 0.01f } }
        }
    },
    // U8: concat
    {
        LayerTransformation::createParamsU8I8(),
        false,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f} },
            { ngraph::element::u8 },
            {
                { element::f32 },
                {},
                { 0.01f }
            },
        },
        {
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {}, { 0.01f } }
        }
    },
    // U8: concat
    {
        LayerTransformation::createParamsU8I8(),
        true,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f} },
            { ngraph::element::u8 },
            {
                { element::f32 },
                {},
                { 0.01f }
            },
        },
        {
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {}, { 0.01f } }
        }
    },
    // U8: concat
    {
        LayerTransformation::createParamsU8I8(),
        false,
        1,
        {
            { 256ul, {{1}, {1}, {1}, {1}}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {{1}, {1}, {1}, {1}}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {}
        },
        {
            {
                256ul, {{1}, {1}, {}, {}}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {{1}, {1}, {}, {}}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {}, { 0.01f } }
        }
    },
    // U8: concat
    {
        LayerTransformation::createParamsU8I8(),
        false,
        1,
        {
            { 256ul, {{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {}
        },
        {
            {
                256ul, {{1, 1, 1, 1}, {1, 1, 1, 1}, {}, {}}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {{1, 1, 1, 1}, {1, 1, 1, 1}, {}, {}}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {}, { 0.01f } }
        }
    },
    // U8: concat multi channels
    {
        LayerTransformation::createParamsU8I8(),
        true,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {0.f}, {1.275f}, {0.f}, {1.275f} },
            {},
            {}
        },
        {
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {0.f}, {1.275f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {}, {{ 0.01f, 0.01f, 0.01f, 0.005f, 0.005f, 0.005f }} }
        }
    },
    // U8: concat multi channels
    {
        LayerTransformation::createParamsU8I8(),
        true,
        1,
        {
            { 256ul, {{1}, {1}, {1}, {1}}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {{1}, {1}, {1}, {1}}, {0.f}, {1.275f}, {0.f}, {1.275f} },
            {},
            {}
        },
        {
            {
                256ul, {{1}, {1}, {}, {}}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {{1}, {1}, {}, {}}, {0.f}, {1.275f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {}, {{ 0.01f, 0.01f, 0.01f, 0.005f, 0.005f, 0.005f }} }
        }
    },
    // U8: concat multi channels
    {
        LayerTransformation::createParamsU8I8(),
        true,
        1,
        {
            {
                256ul,
                {{1, 3, 1, 1}, {1, 3, 1, 1}, {1, 3, 1, 1}, {1, 3, 1, 1}},
                {0.f, 0.f, 0.f}, {2.55f, 2.55f, 2.55f}, {0.f, 0.f, 0.f}, {2.55f / 1.f, 2.55f / 2.f, 2.55f / 3.f}
            },
            {},
            {},
            {
                256ul,
                {{1, 3, 1, 1}, {1, 3, 1, 1}, {1, 3, 1, 1}, {1, 3, 1, 1}},
                {0.f, 0.f, 0.f}, {1.275f, 1.275f, 1.275f}, {0.f, 0.f, 0.f}, {1.275f / 1.f, 1.275f / 2.f, 1.275f / 3.f}
            },
            {},
            {}
        },
        {
            {
                256ul,
                {{1, 3, 1, 1}, {1, 3, 1, 1}, {}, {}},
                {0.f, 0.f, 0.f}, {2.55f, 2.55f, 2.55f}, {0.f}, {255.f},
                ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul,
                {{1, 3, 1, 1}, {1, 3, 1, 1}, {}, {}},
                {0.f, 0.f, 0.f}, {1.275f, 1.275f, 1.275f}, {0.f}, {255.f},
                ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {}, {{ 0.01f / 1.f, 0.01f / 2.f, 0.01f / 3.f, 0.005f / 1.f, 0.005f / 2.f, 0.005f / 3.f }} }
        },
        false,
        false
    },
    // I8
    {
        LayerTransformation::createParamsI8I8(),
        false,
        1,
        {
            { 256ul, {}, {-1.28f}, {1.27f}, {-1.28f}, {1.27f} },
            {},
            {},
            { 256ul, {}, {-1.28f}, {1.27f}, {-1.28f}, {1.27f} },
            {},
            {}
        },
        {
            {
                256ul, {}, {-1.28f}, {1.27f}, {-128.f}, {127.f}, ngraph::element::i8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {-1.28f}, {1.27f}, {-128.f}, {127.f}, ngraph::element::i8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::i8,
            { ngraph::element::f32, {}, { 0.01f } }
        }
    },
    // mixed: U8 + I8: concat (check constant values here)
    {
        LayerTransformation::createParamsU8I8(),
        false,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {-1.28f}, {1.27f}, {-1.28f}, {1.27f} },
            {},
            {}
        },
        {
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{-1.28f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {-1.28f}, {1.27f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{-1.28f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, { {0.f, 0.f, 0.f, 128.f, 128.f, 128.f } }, { 0.01f } }
        }
    },
    // mixed: U8 + I8: concat multi channels
    {
        LayerTransformation::createParamsU8I8(),
        true,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {-1.28f}, {1.27f}, {-1.28f}, {1.27f} },
            {},
            {}
        },
        {
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{-1.28f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {-1.28f}, {1.27f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{-1.28f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, {{ 0.f, 0.f, 0.f, 128.f, 128.f, 128.f }}, { 0.01f } }
        }
    },
    // mixed: I8 + U8: concat (check constant values here)
    {
        LayerTransformation::createParamsU8I8(),
        false,
        1,
        {
            { 256ul, {}, {-1.28f}, {1.27f}, {-1.28f}, {1.27f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {}
        },
        {
            { 256ul, {}, {-1.28f}, {1.27f}, {0.f}, {170.f}, ngraph::element::u8 },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {85.f}, {255.f}, ngraph::element::u8 },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, { 85 }, { 0.015f } }
        },
        true
    },
    // real case from ctdet_coco_dlav0_384 model, coverage bad rounding
    {
        LayerTransformation::createParamsU8I8(),
        false,
        1,
        {
            { 256ul, {}, {-1.28f}, {1.27f}, {0.f}, {2.3007815f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {-3.873046875f}, {3.84375} },
            {},
            {}
        },
        {
            { 256ul, {}, {-1.28f}, {1.27f}, {128.f}, {204.f}, ngraph::element::u8 },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8 },
            {},
            {},
            ngraph::element::u8,
            { ngraph::element::f32, { 128 }, { 0.0302619f } }
        },
        true
    },
    // U8: concat multi channels with subtract, negative axis
    {
        LayerTransformation::createParamsU8I8(),
        true,
        -3,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {1.275f}, {2.55f}, {1.275f}, {2.55f} },
            {},
            {}
        },
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8 },
            {},
            {},
            { 256ul, {}, {1.275f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8 },
            {},
            {},
            ngraph::element::u8,
            {
                ngraph::element::f32,
                {{ 0.f, 0.f, 0.f, -255.f, -255.f, -255.f }},
                {{ 0.01f, 0.01f, 0.01f, 0.005f, 0.005f, 0.005f }}
            }
        }
    },
    // U8: concat multi channels with subtract, not supported axis
    {
        LayerTransformation::createParamsU8I8(),
        true,
        0,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {1.275f}, {2.55f}, {1.275f}, {2.55f} },
            {},
            {}
        },
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {1.275f}, {2.55f}, {1.275f}, {2.55f} },
            {},
            {}
        },
    },
    // U8: concat multi channels with subtract
    // Features:
    //  1. fakeQuantize1 defines precision
    //  2. fakeQuantize2 has zero point (doesn't define precision)
    //  3. FakeQuantize operations order is not important.
    {
        LayerTransformation::createParamsU8I8(),
        true,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {1.275f}, {2.55f}, {1.275f}, {2.55f} },
            {},
            {}
        },
        {
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {1.275f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            ngraph::element::u8,
            {
                ngraph::element::f32,
                {{ 0.f, 0.f, 0.f, -255.f, -255.f, -255.f }},
                {{ 0.01f, 0.01f, 0.01f, 0.005f, 0.005f, 0.005f }}
            }
        },
    },
    // U8: concat multi channels with subtract
    // Features:
    //  1. fakeQuantize2 has zero point (doesn't define precision)
    //  2. fakeQuantize1 defines precision
    //  3. FakeQuantize operations order is not important.
    {
        LayerTransformation::createParamsU8I8(),
        true,
        1,
        {
            { 256ul, {}, {1.275f}, {2.55f}, {1.275f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {}
        },
        {
            {
                256ul, {}, {1.275f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul) }
            },
            {},
            {},
            {
                256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8,
                { make_shared_attribute_ptr<IntervalsAlignmentAttribute>(IntervalsAlignmentSharedValue::Interval{0.f, 2.55f}, 256ul)
            }
            },
            {},
            {},
            ngraph::element::u8,
            {
                ngraph::element::f32,
                {{ -255.f, -255.f, -255.f, 0.f, 0.f, 0.f }},
                {{ 0.005f, 0.005f, 0.005f, 0.01f, 0.01f, 0.01f }}
            }
        },
    },
    // not update precisions
    {
        LayerTransformation::createParamsU8I8().setUpdatePrecisions(false),
        false,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {}
        },
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f} },
            {},
            {},
            ngraph::element::f32,
            { {element::f32}, {}, { 0.01f } },
        }
    },
    // unexpected quantization levels, concat
    {
        LayerTransformation::createParamsU8I8(),
        false,
        1,
        {
            { 16ul, {}, {0.f}, {1.5f}, {0.f}, {15.f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {}
        },
        {
            { 16ul, {}, {0.f}, {1.5f}, {0.f}, {15.f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            ngraph::element::f32,
            {},
        },
        false,
        false,
    },
    // unexpected quantization levels, concat multi channels
    {
        LayerTransformation::createParamsU8I8(),
        true,
        1,
        {
            { 16ul, {}, {0.f}, {1.5f}, {0.f}, {15.f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {}
        },
        {
            { 16ul, {}, {0.f}, {1.5f}, {0.f}, {15.f} },
            {},
            {},
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            ngraph::element::f32,
            {},
        },
        false,
        false
    }
};

INSTANTIATE_TEST_SUITE_P(
    smoke_LPT,
    ConcatTransformation,
    ::testing::Combine(
        ::testing::ValuesIn(precisions),
        ::testing::ValuesIn(shapes),
        ::testing::ValuesIn(testValues)),
    ConcatTransformation::getTestCaseName);
} // namespace testValues1

namespace testValues2 {
const std::vector<ngraph::PartialShape> shapesWithDynamicChannels = {
    { Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic() },
};

const std::vector<ConcatTransformationTestValues> testValues = {
    {
        LayerTransformation::createParamsU8I8(),
        true,
        1,
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {1.275f}, {2.55f}, {1.275f}, {2.55f} },
            {},
            {}
        },
        {
            { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
            {},
            {},
            { 256ul, {}, {1.275f}, {2.55f}, {1.275f}, {2.55f} },
            {},
            {}
        },
    },
};

INSTANTIATE_TEST_SUITE_P(
    smoke_LPT,
    ConcatTransformation,
    ::testing::Combine(
        ::testing::ValuesIn(precisions),
        ::testing::ValuesIn(shapesWithDynamicChannels),
        ::testing::ValuesIn(testValues)),
    ConcatTransformation::getTestCaseName);
} // namespace testValues2

namespace testValues3 {
const std::vector<ngraph::PartialShape> shapesWithDynamicChannels = {
    PartialShape::dynamic()
};

const std::vector<ConcatTransformationTestValues> testValues = {
    // issue #58915
    //{
    //    LayerTransformation::createParamsU8I8(),
    //    true,
    //    1,
    //    {
    //        { 256ul, {}, {0.f}, {2.55f}, {0.f}, {2.55f} },
    //        {},
    //        {},
    //        { 256ul, {}, {1.275f}, {2.55f}, {1.275f}, {2.55f} },
    //        {},
    //        {}
    //    },
    //    {
    //        { 256ul, {}, {0.f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8, },
    //        {},
    //        {{ngraph::element::f32}, {}, {0.01f}},
    //        { 256ul, {}, {1.275f}, {2.55f}, {0.f}, {255.f}, ngraph::element::u8 },
    //        {},
    //        {{ngraph::element::f32}, {-255.f}, {0.005f}},
    //        ngraph::element::f32,
    //        {},
    //    },
    //},
};

INSTANTIATE_TEST_SUITE_P(
    smoke_LPT,
    ConcatTransformation,
    ::testing::Combine(
        ::testing::ValuesIn(precisions),
        ::testing::ValuesIn(shapesWithDynamicChannels),
        ::testing::ValuesIn(testValues)),
    ConcatTransformation::getTestCaseName);
} // namespace testValues3
} // namespace
