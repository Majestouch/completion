#ifndef SRC_AST_AST_VISITOR_HELPER_H_
#define SRC_AST_AST_VISITOR_HELPER_H_

#include <type_traits>
#include <memory>

#include "ast/ast_node_interface.h"
#include "ast/ast_nodes.h"

namespace clidoc {

template <typename NodeSharedPtr, typename NodeType>
struct NodeIs {
  static const bool value = std::is_same<
      typename NodeSharedPtr::element_type,
      NodeType>::value;
};

template <typename... Types>
struct NodeIsNot_;

template <typename NodeSharedPtr, typename NodeType>
struct NodeIsNot_<NodeSharedPtr, NodeType> {
  using Type = typename std::conditional<
      NodeIs<NodeSharedPtr, NodeType>::value,
      std::false_type,
      std::true_type>::type;
};

template <typename NodeSharedPtr, typename NodeType, typename... RemainTypes>
struct NodeIsNot_<NodeSharedPtr, NodeType, RemainTypes...> {
  using Type = typename std::conditional<
      NodeIs<NodeSharedPtr, NodeType>::value,
      std::false_type,
      typename NodeIsNot_<NodeSharedPtr, RemainTypes...>::Type>::type;
};

template <typename NodeSharedPtr, typename... NodeTypes>
struct NodeIsNot {
  static const bool value = std::is_same<
      typename NodeIsNot_<NodeSharedPtr, NodeTypes...>::Type,
      std::true_type>::value;
};

template <typename NodeSharedPtr, typename... NodeTypes>
struct NodeIsOneOf {
  static const bool value = !NodeIsNot<NodeSharedPtr, NodeTypes...>::value;
};

template <typename... T>
struct ExtractParameter;

template <typename ProcessLogicType, typename Parameter>
struct ExtractParameter<void (ProcessLogicType::*)(Parameter)> {
  using Type = Parameter;
};

template <typename... T>
struct ExtractNodeType;

template <TerminalType T>
struct ExtractNodeType<std::shared_ptr<Terminal<T>>> {
  static const TerminalType value = T;
};

template <NonTerminalType T>
struct ExtractNodeType<std::shared_ptr<NonTerminal<T>>> {
  static const NonTerminalType value = T;
};

template <typename ProcessLogicType, typename Parameter>
struct CanInvoke {
  // for member function.
  template <typename T>
  static
  typename ExtractParameter<decltype(&T::ProcessNode)>::Type
  Check(void *unused);
  // for template member function form 1.
  template <typename T>
  static
  typename ExtractParameter<
      decltype(&T::template ProcessNode<Parameter>)>::Type
  Check(void *unused);
  // for template member function form 2.
  template <typename T>
  static
  typename ExtractParameter<
      decltype(&T::template ProcessNode<
          ExtractNodeType<Parameter>::value>)>::Type
  Check(void *unused);
  // for else cases.
  template <typename T>
  static
  std::false_type
  Check(...);  // lowest ranking for overload resolution.

  // `value` is true meaning `Parameter` can be passed to
  // `ProcessLogicType::ProcessNode`.
  static const bool value = std::is_same<
      decltype(Check<ProcessLogicType>(nullptr)),
      Parameter>::value;
};

struct VisitorProcessLogic {
  NodeVisitorInterface *visitor_ptr_;
};

struct DefaultBehaviorOfNonTerminal {
  template <typename ProcessLogicType, typename NonTerminalTypeSharedPtr>
  static void ApplyVisitorProcessLogic(
      ProcessLogicType *process_logic_ptr,
      NonTerminalTypeSharedPtr node) {
    // process logic can not handle the type of `node`.
    auto cache_children = node->children_;
    for (auto child_node : cache_children) {
      child_node->Accept(process_logic_ptr->visitor_ptr_);
    }
  }
};

struct DefaultBehaviorOfTerminal {
  template <typename ProcessLogicType, typename TerminalTypeSharedPtr>
  static void ApplyVisitorProcessLogic(
      ProcessLogicType *process_logic_ptr,
      TerminalTypeSharedPtr node) {
    /* empty */
  }
};

struct VisitorWithProcessLogicInterface {
 protected:
  struct ProcessLogicInvoker {
    template <typename ProcessLogicType, typename NonTerminalTypeSharedPtr>
    static void ApplyVisitorProcessLogic(
        ProcessLogicType *process_logic_ptr,
        NonTerminalTypeSharedPtr node) {
      // ONLY apply process logic to CURRENT node.
      process_logic_ptr->ProcessNode(node);
    }
  };
  template <typename ProcessLogicType, typename NodeType,
            typename DefaultBehavior>
  void ConditionalSelectBehavior(ProcessLogicType *process_logic_ptr,
                                 NodeType node) {
    using Type = typename std::conditional<
        CanInvoke<ProcessLogicType, NodeType>::value,
        ProcessLogicInvoker,
        DefaultBehavior>::type;
    Type::template ApplyVisitorProcessLogic(process_logic_ptr, node);
  }
};

template <typename ProcessLogicType>
class TerminalVisitor : public NodeVisitorInterface,
                        public VisitorWithProcessLogicInterface {
 public:
  using NodeVisitorInterface::ProcessNode;

  explicit TerminalVisitor(ProcessLogicType *process_logic_ptr);

  void ProcessNode(KDoubleHyphen::SharedPtr  node) override;
  void ProcessNode(KOptions::SharedPtr       node) override;
  void ProcessNode(PosixOption::SharedPtr    node) override;
  void ProcessNode(GroupedOptions::SharedPtr node) override;
  void ProcessNode(GnuOption::SharedPtr      node) override;
  void ProcessNode(Argument::SharedPtr       node) override;
  void ProcessNode(Command::SharedPtr        node) override;

 private:
  friend DefaultBehaviorOfTerminal;
  ProcessLogicType *process_logic_ptr_;
};

template <typename ProcessLogicType>
class NonTerminalVisitor : public NodeVisitorInterface,
                           public VisitorWithProcessLogicInterface {
 public:
  using NodeVisitorInterface::ProcessNode;
  explicit NonTerminalVisitor(ProcessLogicType *process_logic_ptr);

  void ProcessNode(Doc::SharedPtr            node) override;
  void ProcessNode(LogicAnd::SharedPtr       node) override;
  void ProcessNode(LogicXor::SharedPtr       node) override;
  void ProcessNode(LogicOr::SharedPtr        node) override;
  void ProcessNode(LogicOptional::SharedPtr  node) override;
  void ProcessNode(LogicOneOrMore::SharedPtr node) override;

 private:
  friend DefaultBehaviorOfTerminal;
  ProcessLogicType *process_logic_ptr_;
};

template <typename ProcessLogicType>
class AllNodeVisitor : public NodeVisitorInterface,
                       public VisitorWithProcessLogicInterface {
 public:
  using NodeVisitorInterface::ProcessNode;
  explicit AllNodeVisitor(ProcessLogicType *process_logic_ptr);

  void ProcessNode(KDoubleHyphen::SharedPtr  node) override;
  void ProcessNode(KOptions::SharedPtr       node) override;
  void ProcessNode(PosixOption::SharedPtr    node) override;
  void ProcessNode(GroupedOptions::SharedPtr node) override;
  void ProcessNode(GnuOption::SharedPtr      node) override;
  void ProcessNode(Argument::SharedPtr       node) override;
  void ProcessNode(Command::SharedPtr        node) override;

  void ProcessNode(Doc::SharedPtr            node) override;
  void ProcessNode(LogicAnd::SharedPtr       node) override;
  void ProcessNode(LogicXor::SharedPtr       node) override;
  void ProcessNode(LogicOr::SharedPtr        node) override;
  void ProcessNode(LogicOptional::SharedPtr  node) override;
  void ProcessNode(LogicOneOrMore::SharedPtr node) override;

 private:
  friend DefaultBehaviorOfTerminal;
  friend DefaultBehaviorOfNonTerminal;
  ProcessLogicType *process_logic_ptr_;
};

template <template <typename> class Visitor, typename ProcessLogicType>
Visitor<ProcessLogicType>
GenerateVisitor(ProcessLogicType *process_logic_ptr) {
  return Visitor<ProcessLogicType>(process_logic_ptr);
}

template <typename ProcessLogicType>
struct NodeTypeModifier {
  // Could be invoked other visitors.
  template <typename TerminalTypeSharedPtr>
  static void ChangeTerminalType(TerminalTypeSharedPtr node);
  template <typename NonTerminalTypeSharedPtr>
  static void ChangeNonTerminalType(NonTerminalTypeSharedPtr node);
};

}  // namespace clidoc

namespace clidoc {

#define CONDITIONAL_SELECT_BEHAVIOR(behavior_cls) \
ConditionalSelectBehavior<                        \
    ProcessLogicType,                             \
    decltype(node),                               \
    behavior_cls                                  \
    >(process_logic_ptr_, node)                   \

template <typename TargetType>
template <typename TerminalTypeSharedPtr>
void NodeTypeModifier<TargetType>::ChangeTerminalType(
    TerminalTypeSharedPtr node) {
  auto new_node = TargetType::Init(node->token_.value());
  node->node_connection.ReplacedWith(new_node);
}

template <typename TargetType>
template <typename NonTerminalTypeSharedPtr>
void NodeTypeModifier<TargetType>::ChangeNonTerminalType(
    NonTerminalTypeSharedPtr node) {
  auto new_node = TargetType::Init();
  for (auto child : node->children_) {
    new_node->AddChild(child);
  }
  node->node_connection.ReplacedWith(new_node);
}

template <typename ProcessLogicType>
TerminalVisitor<ProcessLogicType>::TerminalVisitor(
    ProcessLogicType *process_logic_ptr)
    : process_logic_ptr_(process_logic_ptr) {
  process_logic_ptr_->visitor_ptr_ = this;
}

template <typename ProcessLogicType>
NonTerminalVisitor<ProcessLogicType>::NonTerminalVisitor(
    ProcessLogicType *process_logic_ptr)
    : process_logic_ptr_(process_logic_ptr) {
  process_logic_ptr_->visitor_ptr_ = this;
}

template <typename ProcessLogicType>
AllNodeVisitor<ProcessLogicType>::AllNodeVisitor(
    ProcessLogicType *process_logic_ptr)
    : process_logic_ptr_(process_logic_ptr) {
  process_logic_ptr_->visitor_ptr_ = this;
}

// TerminalVisitor.
template <typename ProcessLogicType>
void TerminalVisitor<ProcessLogicType>::ProcessNode(
    KDoubleHyphen::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void TerminalVisitor<ProcessLogicType>::ProcessNode(
    KOptions::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void TerminalVisitor<ProcessLogicType>::ProcessNode(
    PosixOption::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void TerminalVisitor<ProcessLogicType>::ProcessNode(
    GroupedOptions::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void TerminalVisitor<ProcessLogicType>::ProcessNode(
    GnuOption::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void TerminalVisitor<ProcessLogicType>::ProcessNode(
    Argument::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void TerminalVisitor<ProcessLogicType>::ProcessNode(
    Command::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

// NonTerminalVisitor.
template <typename ProcessLogicType>
void NonTerminalVisitor<ProcessLogicType>::ProcessNode(
    Doc::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

template <typename ProcessLogicType>
void NonTerminalVisitor<ProcessLogicType>::ProcessNode(
    LogicAnd::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

template <typename ProcessLogicType>
void NonTerminalVisitor<ProcessLogicType>::ProcessNode(
    LogicXor::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

template <typename ProcessLogicType>
void NonTerminalVisitor<ProcessLogicType>::ProcessNode(
    LogicOr::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

template <typename ProcessLogicType>
void NonTerminalVisitor<ProcessLogicType>::ProcessNode(
    LogicOptional::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

template <typename ProcessLogicType>
void NonTerminalVisitor<ProcessLogicType>::ProcessNode(
    LogicOneOrMore::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

// AllNodeVisitor
template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    KDoubleHyphen::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    KOptions::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    PosixOption::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    GroupedOptions::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    GnuOption::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    Argument::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    Command::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    Doc::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    LogicAnd::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    LogicXor::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    LogicOr::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    LogicOptional::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

template <typename ProcessLogicType>
void AllNodeVisitor<ProcessLogicType>::ProcessNode(
    LogicOneOrMore::SharedPtr node) {
  CONDITIONAL_SELECT_BEHAVIOR(DefaultBehaviorOfNonTerminal);
}

}  // namespace clidoc

#endif  // SRC_AST_AST_VISITOR_HELPER_H_
