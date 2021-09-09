// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "../../common/tests_utils.h"
#include "../../common/utils.h"
#include "../../common/ie_pipelines/pipelines.h"

#include <string>

#include <inference_engine.hpp>

// tests_pipelines/tests_pipelines.cpp
TestResult common_test_pipeline(std::vector<std::function<void()>> test_pipeline, const int& n);
// tests_pipelines/tests_pipelines.cpp
