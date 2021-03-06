
#include <set>
#include <string>
#include <algorithm>
#include <iterator>

#include "ast/ast_build.h"
#include "ast/parser_proxy.h"
#include "ast/ast_node_interface.h"
#include "ast/process_logic.h"
#include "ast/option_record.h"
#include "ast/ast_visitor_helper.h"

using std::set;
using std::string;
using std::set_difference;
using std::inserter;

namespace clidoc {

void CodeGenInfo::PrepareFocusedElements(
    set<Token> *elements_ptr,
    set<Token> *oom_elements_ptr) {
  // remove the intersection of OOM element from focused element.
  set<Token> temp_elements = *elements_ptr;
  elements_ptr->clear();
  set_difference(
      temp_elements.begin(), temp_elements.end(),
      oom_elements_ptr->begin(), oom_elements_ptr->end(),
      inserter(*elements_ptr, elements_ptr->end()));
  // setup focused elements/
  for (const Token &element : *elements_ptr) {
    if (recorder_.OptionIsBound(element)) {
      bound_options_.insert(element);
      continue;
    }
    switch (element.type()) {
      case TerminalType::ARGUMENT:
        arguments_.insert(element);
        break;
      case TerminalType::COMMAND:
        commands_.insert(element);
        break;
      default:
        unbound_options_.insert(element);
    }
  }
  // setup `oom_*`.
  for (const Token &element : *oom_elements_ptr) {
    switch (element.type()) {
      case TerminalType::ARGUMENT:
        oom_arguments_.insert(element);
        break;
      default:
        oom_bound_options_.insert(element);
    }
  }
  // setup `default_values_`.
  for (auto iter = recorder_.representative_option_to_property_.begin();
       iter != recorder_.representative_option_to_property_.end();
       ++iter) {
    const auto &rep_option = iter->first;
    const auto &property = iter->second;
    if (property.HasDefaultValue()) {
      default_values_[rep_option] = property.default_value_;
    }
  }
}

void CodeGenInfo::PostProcessedAST() {
  // uncategorized focused elements.
  set<Token> elements;
  set<Token> oom_elements;

  // 1. remove duplicated nodes.
  StructureOptimizerLogic structure_optimizer_logic;
  auto structure_optimizer = GenerateVisitor<NonTerminalVisitor>(
      &structure_optimizer_logic);
  doc_node_->Accept(&structure_optimizer);

  // 2. process `--`.
  DoubleHyphenHandlerLogic double_dash_handler_logic;
  auto double_dash_handler = GenerateVisitor<TerminalVisitor>(
      &double_dash_handler_logic);
  doc_node_->Accept(&double_dash_handler);

  // 3. handle ambiguous syntax.
  AmbiguityHandlerLogic ambiguity_handler_logic(&recorder_);
  auto ambiguity_handler = GenerateVisitor<TerminalVisitor>(
      &ambiguity_handler_logic);
  doc_node_->Accept(&ambiguity_handler);

  // 4. collect focused elements.
  FocusedElementCollectorLogic element_collector_logic(&recorder_);
  auto element_collector =
      GenerateVisitor<TerminalVisitor>(&element_collector_logic);
  doc_node_->Accept(&element_collector);
  elements = element_collector_logic.GetFocusedElements();

  // 5. collect OOM information and insert missing OOM nodes.
  // 5.1 collect information.
  OneOrMoreMarkedElementCollectorLogic oom_collector_logic(&recorder_);
  auto oom_collector =
      GenerateVisitor<NonTerminalVisitor>(&oom_collector_logic);
  doc_node_->Accept(&oom_collector);
  oom_elements = oom_collector_logic.GetOneOrMoreMarkedElements();
  // setup focused elements here!
  PrepareFocusedElements(&elements, &oom_elements);
  // 5.2 insert OOM nodes.
  OneOrMoreNodeInsertLogic oom_insert_logic(oom_bound_options_);
  auto oom_inserter = GenerateVisitor<TerminalVisitor>(&oom_insert_logic);
  doc_node_->Accept(&oom_inserter);

  // 6.TODO(huntzhan): issue #17.

  // 7. Remove bound arguments.
  auto bound_arguments = recorder_.GetBoundArguments();
  BoundArgumentCleanerLogic bound_argument_cleaner_logic(bound_arguments);
  auto bound_argument_cleaner = GenerateVisitor<TerminalVisitor>(
      &bound_argument_cleaner_logic);
  doc_node_->Accept(&bound_argument_cleaner);

  // 8. remove duplicated nodes again.
  doc_node_->Accept(&structure_optimizer);
}

void CodeGenInfo::Prepare(const std::string &raw_doc) {
  DocPreprocessor doc_preprocessor;
  ParserProxy parser_proxy;
  // setup `doc_text_`.
  doc_text_ = doc_preprocessor.PreprocessRawDocForCodeGen(raw_doc);
  // setup `doc_node_`, `recorder_`.
  parser_proxy.Parse(raw_doc, &doc_node_, &recorder_);
  PostProcessedAST();
}

}  // namespace clidoc
