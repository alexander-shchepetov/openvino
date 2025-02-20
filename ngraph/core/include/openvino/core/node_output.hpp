// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cstring>
#include <map>
#include <unordered_set>

#include "openvino/core/core_visibility.hpp"
#include "openvino/core/descriptor/tensor.hpp"
#include "openvino/core/partial_shape.hpp"
#include "openvino/core/shape.hpp"
#include "openvino/core/type/element_type.hpp"
#include "openvino/core/variant.hpp"

namespace ov {
class Node;

template <typename NodeType>
class Input;

template <typename NodeType>
class Output {};

/// \brief A handle for one of a node's outputs.
template <>
class OPENVINO_API Output<Node> {
public:
    /// \brief Constructs a Output.
    /// \param node A pointer to the node for the output handle.
    /// \param index The index of the output.
    Output(Node* node, size_t index);

    /// \brief Constructs a Output.
    /// \param node A `shared_ptr` to the node for the output handle.
    /// \param index The index of the output.
    ///
    /// TODO: Make a plan to deprecate this.
    Output(const std::shared_ptr<Node>& node, size_t index);

    /// \brief Constructs a Output, referencing the zeroth output of the node.
    /// \param node A `shared_ptr` to the node for the output handle.
    template <typename T>
    Output(const std::shared_ptr<T>& node) : Output(node ? node->get_default_output() : Output<Node>()) {}

    /// A null output
    Output() = default;

    void reset();

    /// This output position for a different node
    Output<Node> for_node(const std::shared_ptr<Node>& node);
    /// \return A pointer to the node referred to by this output handle.
    Node* get_node() const;
    /// \return A `shared_ptr` to the node referred to by this output handle.
    ///
    /// TODO: Make a plan to deprecate this.
    std::shared_ptr<Node> get_node_shared_ptr() const;

    /// \return The index of the output referred to by this output handle.
    size_t get_index() const;
    /// \return A reference to the tensor descriptor for this output.
    descriptor::Tensor& get_tensor() const;
    /// \return A shared point to the tensor ptr for this output.
    std::shared_ptr<descriptor::Tensor> get_tensor_ptr() const;
    /// \return The element type of the output referred to by this output handle.
    const element::Type& get_element_type() const;
    /// \return The shape of the output referred to by this output handle.
    const Shape& get_shape() const;
    /// \return The partial shape of the output referred to by this output handle.
    const PartialShape& get_partial_shape() const;

    /// \return The reference to runtime info map
    RTMap& get_rt_info();
    /// \return The constant reference to runtime info map
    const RTMap& get_rt_info() const;

    /// \return A set containing handles for all inputs targeted by the output referenced by
    ///        this output handle.
    std::set<Input<Node>> get_target_inputs() const;

    /// \brief Removes a target input from the output referenced by this output handle.
    /// \param target_input The target input to remove.
    ///
    // TODO(amprocte): Investigate whether this really ought to be public.
    void remove_target_input(const Input<Node>& target_input) const;

    /// \brief Replace all users of this value with replacement
    void replace(const Output<Node>& replacement);

    bool operator==(const Output& other) const;
    bool operator!=(const Output& other) const;
    bool operator<(const Output& other) const;
    bool operator>(const Output& other) const;
    bool operator<=(const Output& other) const;
    bool operator>=(const Output& other) const;

private:
    std::shared_ptr<Node> m_node;
    size_t m_index{0};
};

template <>
class OPENVINO_API Output<const Node> {
public:
    /// \brief Constructs a Output.
    /// \param node A pointer to the node for the output handle.
    /// \param index The index of the output.
    Output(const Node* node, size_t index);

    /// \brief Constructs a Output.
    /// \param node A `shared_ptr` to the node for the output handle.
    /// \param index The index of the output.
    ///
    /// TODO: Make a plan to deprecate this.
    Output(const std::shared_ptr<const Node>& node, size_t index);

    /// \brief Constructs a Output, referencing the zeroth output of the node.
    /// \param node A `shared_ptr` to the node for the output handle.
    template <typename T>
    Output(const std::shared_ptr<const T>& node) : Output(node ? node->get_default_output() : Output<const Node>()) {}

    /// A null output
    Output() = default;

    void reset();

    /// This output position for a different node
    Output<const Node> for_node(const std::shared_ptr<const Node>& node);

    /// \return A pointer to the node referred to by this output handle.
    const Node* get_node() const;
    /// \return A `shared_ptr` to the node referred to by this output handle.
    ///
    /// TODO: Make a plan to deprecate this.
    std::shared_ptr<const Node> get_node_shared_ptr() const;
    /// \return The index of the output referred to by this output handle.
    size_t get_index() const;
    /// \return A reference to the tensor descriptor for this output.
    descriptor::Tensor& get_tensor() const;
    /// \return A shared point to the tensor ptr for this output.
    std::shared_ptr<descriptor::Tensor> get_tensor_ptr() const;
    /// \return The element type of the output referred to by this output handle.
    const element::Type& get_element_type() const;
    /// \return The shape of the output referred to by this output handle.
    const Shape& get_shape() const;
    /// \return The partial shape of the output referred to by this output handle.
    const PartialShape& get_partial_shape() const;

    /// \return The constant reference to runtime info map
    const RTMap& get_rt_info() const;
    /// \return A set containing handles for all inputs targeted by the output referenced by
    ///        this output handle.
    std::set<Input<Node>> get_target_inputs() const;

    bool operator==(const Output& other) const;
    bool operator!=(const Output& other) const;
    bool operator<(const Output& other) const;
    bool operator>(const Output& other) const;
    bool operator<=(const Output& other) const;
    bool operator>=(const Output& other) const;

private:
    std::shared_ptr<const Node> m_node;
    size_t m_index{0};
};

OPENVINO_API std::ostream& operator<<(std::ostream& out, const Output<Node>& output);
OPENVINO_API std::ostream& operator<<(std::ostream& out, const Output<const Node>& output);
}  // namespace ov
