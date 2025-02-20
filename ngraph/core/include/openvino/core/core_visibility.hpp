// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "openvino/core/visibility.hpp"

#define OV_NEW_API 1
// Now we use the generic helper definitions above to define NGRAPH_API
// NGRAPH_API is used for the public API symbols. It either DLL imports or DLL exports
//    (or does nothing for static build)

#ifdef _WIN32
#    pragma warning(disable : 4251)
#    pragma warning(disable : 4275)
#endif

#ifdef NGRAPH_STATIC_LIBRARY  // defined if we are building or calling NGRAPH as a static library
#    define OPENVINO_API
#    define OPENVINO_API_C(...) __VA_ARGS__
#else
#    ifdef ngraph_EXPORTS  // defined if we are building the NGRAPH DLL (instead of using it)
#        define OPENVINO_API        OPENVINO_CORE_EXPORTS
#        define OPENVINO_API_C(...) OPENVINO_EXTERN_C OPENVINO_CORE_EXPORTS __VA_ARGS__ OPENVINO_CDECL
#    else
#        define OPENVINO_API        OPENVINO_CORE_IMPORTS
#        define OPENVINO_API_C(...) OPENVINO_EXTERN_C OPENVINO_CORE_IMPORTS __VA_ARGS__ OPENVINO_CDECL
#    endif  // ngraph_EXPORTS
#endif      // NGRAPH_STATIC_LIBRARY
