// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "mkldnn_extension_utils.h"
#include "utils/general_utils.h"
#include <vector>
#include "memory_desc/dnnl_blocked_memory_desc.h"

using namespace mkldnn;
using namespace MKLDNNPlugin;

uint8_t MKLDNNExtensionUtils::sizeOfDataType(mkldnn::memory::data_type dataType) {
    switch (dataType) {
    case mkldnn::memory::data_type::f32:
        return 4;
    case mkldnn::memory::data_type::s32:
        return 4;
    case mkldnn::memory::data_type::bf16:
        return 2;
    case mkldnn::memory::data_type::s8:
        return 1;
    case mkldnn::memory::data_type::u8:
        return 1;
    case mkldnn::memory::data_type::bin:
        return 1;
    case mkldnn::memory::data_type::undef:
        return 0;
    default:
        IE_THROW() << "Unsupported data type.";
    }
}

memory::data_type MKLDNNExtensionUtils::IEPrecisionToDataType(const InferenceEngine::Precision& prec) {
    switch (prec) {
        case InferenceEngine::Precision::FP32:
            return memory::data_type::f32;
        case InferenceEngine::Precision::I32:
            return memory::data_type::s32;
        case InferenceEngine::Precision::BF16:
            return memory::data_type::bf16;
        case InferenceEngine::Precision::I8:
            return memory::data_type::s8;
        case InferenceEngine::Precision::U8:
        case InferenceEngine::Precision::BOOL:
            return memory::data_type::u8;
        case InferenceEngine::Precision::BIN:
            return memory::data_type::bin;
        case InferenceEngine::Precision::UNSPECIFIED:
            return memory::data_type::undef;
        default: {
            IE_THROW() << "The plugin does not support " << prec.name();
        }
    }
}

InferenceEngine::Precision MKLDNNExtensionUtils::DataTypeToIEPrecision(memory::data_type dataType) {
    switch (dataType) {
        case memory::data_type::f32:
            return InferenceEngine::Precision::FP32;
        case memory::data_type::s32:
            return InferenceEngine::Precision::I32;
        case memory::data_type::bf16:
            return InferenceEngine::Precision::BF16;
        case memory::data_type::s8:
            return InferenceEngine::Precision::I8;
        case memory::data_type::u8:
            return InferenceEngine::Precision::U8;
        case memory::data_type::bin:
            return InferenceEngine::Precision::BIN;
        case memory::data_type::undef:
            return InferenceEngine::Precision::UNSPECIFIED;
        default: {
            IE_THROW() << "Unsupported data type.";
        }
    }
}

Dim MKLDNNExtensionUtils::convertToDim(const dnnl::memory::dim &dim) {
    return dim == DNNL_RUNTIME_DIM_VAL ?  Shape::UNDEFINED_DIM : static_cast<size_t>(dim);
}
dnnl::memory::dim MKLDNNExtensionUtils::convertToDnnlDim(const Dim &dim) {
    return dim == Shape::UNDEFINED_DIM ? DNNL_RUNTIME_DIM_VAL : static_cast<mkldnn::memory::dim>(dim);
}

VectorDims MKLDNNExtensionUtils::convertToVectorDims(const memory::dims& dims) {
    std::vector<size_t> vecResult;
    vecResult.reserve(dims.size());
    std::back_insert_iterator<std::vector<size_t>> itr(vecResult);
    std::transform(dims.begin(), dims.end(), itr, convertToDim);
    return vecResult;
}

memory::dims MKLDNNExtensionUtils::convertToDnnlDims(const VectorDims& dims) {
    memory::dims vecResult;
    vecResult.reserve(dims.size());
    std::back_insert_iterator<memory::dims> itr(vecResult);
    std::transform(dims.begin(), dims.end(), itr, convertToDnnlDim);
    return vecResult;
}

memory::format_tag MKLDNNExtensionUtils::GetPlainFormatByRank(size_t rank) {
    switch (rank) {
        case 0:
        case 1:
            return memory::format_tag::a;
        case 2:
            return memory::format_tag::ab;
        case 3:
            return memory::format_tag::abc;
        case 4:
            return memory::format_tag::abcd;
        case 5:
            return memory::format_tag::abcde;
        case 6:
            return memory::format_tag::abcdef;
        default:
            return memory::format_tag::undef;
    }
}

DnnlMemoryDescPtr MKLDNNExtensionUtils::makeDescriptor(const mkldnn::memory::desc &desc) {
    if (desc.data.format_kind == dnnl_blocked) {
        return std::shared_ptr<DnnlBlockedMemoryDesc>(new DnnlBlockedMemoryDesc(desc));
    } else {
        return std::shared_ptr<DnnlMemoryDesc>(new DnnlMemoryDesc(desc));
    }
}

size_t MKLDNNExtensionUtils::getMemSizeForDnnlDesc(mkldnn::memory::desc desc) {
    const auto offset0 = desc.data.offset0;
    desc.data.offset0 = 0;
    size_t size = desc.get_size();
    if (size == DNNL_RUNTIME_SIZE_VAL)
        return MemoryDesc::UNDEFINED_SIZE;
    size += offset0 * sizeOfDataType(desc.data_type());
    return size;
}
