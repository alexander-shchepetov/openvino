// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "shared_test_classes/single_layer/convert_color_nv12.hpp"

namespace LayerTestsDefinitions {

TEST_P(ConvertColorNV12LayerTest, CompareWithRefs) {
    Run();
};

} // namespace LayerTestsDefinitions