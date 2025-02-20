// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "ir_frontend/model.hpp"

#include <xml_parse_utils.h>

#include <ir_deserializer.hpp>
#include <ngraph/opsets/opset1.hpp>
#include <openvino/op/util/framework_node.hpp>
#include <pugixml.hpp>

using namespace ngraph;
using namespace InferenceEngine;

namespace {
void ParsePreProcess(pugi::xml_node& root, ov::Weights weights, std::shared_ptr<Function> f) {
    /* Preprocessing block can have two preprocessing types:
     *
     * <pre-process mean-precision="FP32" reference-layer-name="data">
     *     <channel id="0">
     *         <mean value="1.1"/>
     *     </channel>
     * </pre-process>
     *
     * OR
     *
     * <pre-process mean-precision="FP32" reference-layer-name="data">
     *     <channel id="0">
     *         <mean offset="0" size="1936"/>
     *     </channel>
     * </pre-process>
     */

    auto ppNode = root.child("pre-process");
    if (ppNode.empty()) {
        return;
    }
    // find out to what input this belongs to
    std::string inputName;
    std::shared_ptr<Node> input_node;

    inputName = XMLParseUtils::GetStrAttr(ppNode, "reference-layer-name", "");
    inputName = trim(inputName);

    if (inputName.empty()) {
        // fallback (old format), look for the picture in the inputs
        for (const auto& parameter : f->get_parameters()) {
            if (parameter->get_partial_shape().rank().is_static() &&
                parameter->get_partial_shape().rank().get_length() == 4) {
                input_node = parameter;
                break;
            }
        }

        IE_ASSERT(!f->get_parameters().empty());
        if (!input_node) {
            input_node = f->get_parameters()[0];
        }

        inputName = input_node->get_friendly_name();
    } else {
        for (const auto& parameter : f->get_parameters()) {
            if (parameter->get_friendly_name() == inputName) {
                input_node = parameter;
                break;
            }
        }
    }

    if (!input_node)
        IE_THROW() << "pre-process name ref '" << inputName << "' refers to un-existing input";

    const auto& input_shape = input_node->output(0).get_partial_shape();
    if (input_shape.is_dynamic()) {
        IE_THROW() << "can not apply pre-process for '" << inputName << "' input";
    }

    Shape mean_scalar_shape;  // [C, 1 ... 1]
    Shape mean_shape;         // [1, H, W] - for 4D case

    const auto inputDims = input_shape.to_shape();

    if (inputDims.size() < 2) {
        IE_THROW() << "network did not define input dimensions properly";
    } else if (inputDims.size() == 2) {  // NC
        mean_scalar_shape = {inputDims[1]};
        mean_shape = {1};
    } else if (inputDims.size() == 3) {  // CHW - legacy representation for 3D input shape
        mean_scalar_shape = {inputDims[0], 1, 1};
        mean_shape = {1, inputDims[1], inputDims[2]};
    } else if (inputDims.size() == 4) {  // NCHW
        mean_scalar_shape = {inputDims[1], 1, 1};
        mean_shape = {1, inputDims[2], inputDims[3]};
    } else if (inputDims.size() == 5) {  // NCDHW
        mean_scalar_shape = {inputDims[1], 1, 1, 1};
        mean_shape = {1, inputDims[2], inputDims[3], inputDims[4]};
    }
    const int64_t channels = mean_scalar_shape[0];

    int64_t next_channel_id{0};
    std::set<std::pair<int64_t, float>> mean_scalar_values;
    std::set<std::pair<int64_t, std::pair<int64_t, int64_t>>> mean_values;

    auto input_type = input_node->get_output_element_type(0);
    FOREACH_CHILD (chan, ppNode, "channel") {
        int chanNo = XMLParseUtils::GetIntAttr(chan, "id", next_channel_id++);

        auto meanNode = chan.child("mean");
        if (!meanNode.empty()) {
            if (!meanNode.attribute("value") && (!meanNode.attribute("size"))) {
                IE_THROW() << "mean should have at least one of the following attribute: value, size";
            }
            if (meanNode.attribute("value")) {
                mean_scalar_values.insert({chanNo, XMLParseUtils::GetFloatAttr(meanNode, "value")});
            }
            if (meanNode.attribute("size") && meanNode.attribute("offset")) {
                auto const_size = XMLParseUtils::GetIntAttr(meanNode, "size");
                auto const_offset = XMLParseUtils::GetIntAttr(meanNode, "offset");
                if (shape_size(mean_shape) * input_type.size() != const_size) {
                    IE_THROW() << "mean blob size mismatch expected input, got: " << const_size << " expecting "
                               << mean_shape << " x " << input_type.size();
                }
                if (const_offset + const_size > weights->size()) {
                    IE_THROW() << "mean value offset and size are out of weights size range";
                }
                mean_values.insert({chanNo, {const_size, const_offset}});
            }
        }
    }

    if (!mean_values.empty() && !mean_scalar_values.empty()) {
        IE_THROW() << "mean values have different types";
    }

    if (!mean_scalar_values.empty()) {
        if (mean_scalar_values.size() != channels) {
            IE_THROW() << "Number of mean values (" << mean_scalar_values.size()
                       << ") is not equal to number of channels (" << channels << ")";
        }
        std::vector<float> values(channels);
        for (const auto& item : mean_scalar_values) {
            if (item.first >= channels) {
                IE_THROW() << "Mean values channel index " << item.first << " is out of range (" << channels << ")";
            }
            values[item.first] = item.second;
        }
        auto mean_values_constant = ngraph::op::Constant::create(input_type, mean_scalar_shape, values);

        const auto& consumers = input_node->output(0).get_target_inputs();
        auto add = std::make_shared<ngraph::opset1::Subtract>(input_node, mean_values_constant);
        for (const auto& consumer : consumers) {
            consumer.replace_source_output(add);
        }
    }

    if (!mean_values.empty()) {
        if (mean_values.size() != channels) {
            IE_THROW() << "Number of mean values (" << mean_values.size() << ") is not equal to number of channels ("
                       << channels << ")";
        }
        NodeVector per_channel_values(channels);
        for (const auto& item : mean_values) {
            if (item.first >= channels) {
                IE_THROW() << "Mean values channel index " << item.first << " is out of range (" << channels << ")";
            }
            const size_t offset = item.second.second;
            const char* data = weights->get_ptr<char>() + offset;
            per_channel_values[item.first] = ngraph::opset1::Constant::create(input_type, mean_shape, data);
        }
        auto const_node = get_constant_from_source(std::make_shared<ngraph::opset1::Concat>(per_channel_values, 0));
        IE_ASSERT(const_node);
        const auto& consumers = input_node->output(0).get_target_inputs();
        auto add = std::make_shared<ngraph::opset1::Subtract>(input_node, const_node);
        for (const auto& consumer : consumers) {
            consumer.replace_source_output(add);
        }
    }
}
}  // namespace

namespace ngraph {
namespace frontend {
class InputModelIR::InputModelIRImpl {
    ov::Weights m_weights;
    ov::Extensions m_extensions;
    pugi::xml_node m_root;
    pugi::xml_document m_xml_doc;

public:
    InputModelIRImpl(std::istream& stream, const ov::Weights& weights, const ov::Extensions& extensions)
        : m_weights(weights),
          m_extensions(extensions) {
        pugi::xml_parse_result res = m_xml_doc.load(stream);
        if (res.status != pugi::status_ok) {
            IE_THROW() << res.description() << "at offset " << res.offset;
        }
        m_root = m_xml_doc.document_element();
    }

    std::shared_ptr<Function> convert();
};

InputModelIR::InputModelIR(std::istream& stream, const ov::Weights& weights, const ov::Extensions& extensions) {
    _impl = std::make_shared<InputModelIRImpl>(stream, weights, extensions);
}

std::shared_ptr<Function> InputModelIR::convert() {
    return _impl->convert();
}

std::shared_ptr<Function> InputModelIR::InputModelIRImpl::convert() {
    std::unordered_map<std::string, ngraph::OpSet> opsets;
    std::unordered_map<std::string, std::shared_ptr<ngraph::Variable>> variables;

    // Load default opsets
    opsets["opset1"] = ngraph::get_opset1();
    opsets["opset2"] = ngraph::get_opset2();
    opsets["opset3"] = ngraph::get_opset3();
    opsets["opset4"] = ngraph::get_opset4();
    opsets["opset5"] = ngraph::get_opset5();
    opsets["opset6"] = ngraph::get_opset6();
    opsets["opset7"] = ngraph::get_opset7();
    opsets["opset8"] = ngraph::get_opset8();

    // Load custom opsets
    for (const auto& it : m_extensions) {
        if (opsets.find(it.first) != opsets.end())
            IE_THROW() << "Cannot add opset with name: " << it.first << ". Opset with the same name already exists.";
        opsets[it.first] = it.second;
    }

    ov::XmlDeserializer visitor(m_root, m_weights, opsets, variables);
    visitor.use_framework_node(opsets.count("framework_node_ext"));
    std::shared_ptr<ngraph::Function> function;
    visitor.on_attribute("net", function);

    ParsePreProcess(m_root, m_weights, function);

    return function;
}
}  // namespace frontend
}  // namespace ngraph
