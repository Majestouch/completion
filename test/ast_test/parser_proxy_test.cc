
#include <string>
#include "gtest/gtest.h"

#include "ast/ast_node_interface.h"
#include "ast/ast_nodes.h"
#include "ast/parser_proxy.h"
#include "ast/option_record.h"
#include "ast/string_utils.h"

using std::string;

namespace clidoc {

TEST(StringUtilsTest, ExtractSection) {
  string input1 =
      "Usage:\n"
      "   this is line one.\n"
      "   this is line two.\n"
      "Options:\n"
      "   this is line one.\n"
      "   this is line two.\n";
  string expect1 = 
      "Usage:\n"
      "   this is line one.\n"
      "   this is line two.\n";

  string input2 =
      "Usage  \t :\n"
      "   this is line one.\n"
      "   this is line two.\n"
      "Options:\n"
      "   this is line one.\n"
      "   this is line two.\n";
  string expect2 = 
      "Usage:\n"
      "   this is line one.\n"
      "   this is line two.\n";

  string input3 =
      "Usage:\n"
      "   this is line one.\n"
      "   this is line two.\n"
      "Options:\n"
      "   this is line one.\n"
      "   this is line two. [default: \"42\"]\n"
      "   this is line two. [default: \"42\"]\n"
      "   this is line three.\n";
  string expect3 =
      "Options:\n"
      "   this is line one.\n"
      "   this is line two. [default: \"42\"]\n"
      "   this is line two. [default: \"42\"]\n"
      "   this is line three.\n";

  string output1;
  EXPECT_TRUE(StringUtils::ExtractSection("Usage", input1, &output1));
  EXPECT_EQ(expect1, output1);

  string output2;
  EXPECT_TRUE(StringUtils::ExtractSection("usage", input1, &output2));
  EXPECT_EQ(expect2, output2);

  string output3;
  EXPECT_TRUE(StringUtils::ExtractSection("options", input3, &output3));
  EXPECT_EQ(expect3, output3);
}

TEST(DocPreprocessorTest, RemoveComment) {
  DocPreprocessor preprocessor;
  preprocessor.text_ =
      "  line one #  whatever.\n"
      "line two on comment.\n"
      "# to be remove\n"
      "line three#test.\n\n";
  string expect =
      "  line one \n"
      "line two on comment.\n"
      "\n"
      "line three\n\n";
  preprocessor.RemoveComment();
  EXPECT_EQ(expect, preprocessor.text_);
}

TEST(DocPreprocessorTest, RemoveEmptyLine) {
  DocPreprocessor preprocessor;
  preprocessor.text_ =
      "line one\n"
      "\t line two\n"
      "  \t   \n"
      "\n\n\n\n\n\n"
      "\t\n\n\n\n\n"
      "     line three\n"
      "\n";
  string expect =
      "line one\n"
      "line two\n"
      "line three\n";
  preprocessor.RemoveEmptyLine();
  EXPECT_EQ(expect, preprocessor.text_);
}

TEST(DocPreprocessorTest, ReplaceUtilityName) {
  DocPreprocessor preprocessor;
  preprocessor.usage_section_ =
      "Usage:\n"
      "   some_program+py -f --long command\n"
      "   some_program+py [whatever] command2\n";
  string expect =
      "Usage:\n"
      "   *UTILITY_DELIMITER* -f --long command\n"
      "   *UTILITY_DELIMITER* [whatever] command2\n";
  preprocessor.ReplaceUtilityName();
  EXPECT_EQ(expect, preprocessor.usage_section_);
}

TEST(DocPreprocessorTest, ExtractAndProcessOptionsSection) {
  DocPreprocessor preprocessor;
  preprocessor.text_ =
      "Options:\n"
      "   this is line one.\n"
      "   this is line two.\n";
  string expect =
      "Options:\n"
      "   this is line one.\n*DESC_DELIMITER*"
      "   this is line two.\n*DESC_DELIMITER*";
  preprocessor.ExtractAndProcessOptionsSection();
  EXPECT_EQ(expect, preprocessor.options_section_);
}

TEST(ParserProxyTest, PreprocessRawDoc) {
  string input =
      "Usage  :\n"
      "   some_program.py [-f] FILE [options] -- <foo  \t bar>\n"
      "     # brbrbr.\n"
      "   some_program.py (foo|bar) --long=<newline>\n"
      "\n\t \n\n"
      "Options \t:\n"
      "   this is line one. [default: \"a   b\"] # whatever\n"
      "       # whatever\n"
      "   this is line two. [default: \"a   b\"] \n"
      " -h <arg 1> [default: \"42\"]\n"
      " --help=ARG-2 [default:\"43\"]\n"
      "\n\n\n";

  string expect =
    "Usage:"
    " *UTILITY_DELIMITER* [ -f ] FILE [ options ] -- <foo  \t bar>"
    " *UTILITY_DELIMITER* ( foo | bar ) --long = <newline>"
    " Options:"
    " this is line one. [ default: \"a   b\" ] *DESC_DELIMITER*"
    " this is line two. [ default: \"a   b\" ] *DESC_DELIMITER*"
    " -h <arg 1> [ default: \"42\" ] *DESC_DELIMITER*"
    " --help = ARG-2 [ default: \"43\" ] *DESC_DELIMITER*";
  ParserProxy proxy;
  EXPECT_EQ(expect, proxy.PreprocessRawDoc(input));
}

// black box.
TEST(OptionBindingRecorderTest, RecordBinding) {
  auto BuildRecord = [](const string &input,
                        OptionBindingRecorder *recorder_ptr) {
    // clean up recorder.
    recorder_ptr->option_to_representative_option_.clear();
    recorder_ptr->representative_option_to_property_.clear();
    // build.
    ParserProxy proxy;
    auto preprocess_doc = proxy.PreprocessRawDoc(input);
    Doc::SharedPtr doc_ptr;
    proxy.ParseByBison(preprocess_doc, &doc_ptr, recorder_ptr);
  };
  string input;
  OptionBindingRecorder recorder;

  auto option_h = Token(TerminalType::POSIX_OPTION, "-h");
  auto option_help = Token(TerminalType::GNU_OPTION, "--help");
  auto argument_1 = Token(TerminalType::ARGUMENT, "<arg 1>");
  auto argument_2 = Token(TerminalType::ARGUMENT, "ARG-2");
  RepresentativeOptionProperty *rop_ptr;

  // case 1.
  input =
      "Usage:\n"
      " utility_name -c <some arg>\n"
      " Options:\n"
      " -h --help\n";
  BuildRecord(input, &recorder);

  EXPECT_EQ(
      option_help,
      recorder.option_to_representative_option_[option_h]);
  EXPECT_EQ(
      option_help,
      recorder.option_to_representative_option_[option_help]);

  //case 2.
  // bind -h, --help to <arg 1>, no default value.
  input =
      "Usage:\n"
      " utility_name -c <some arg>\n"
      " Options:\n"
      " -h <arg 1> --help=<arg 1>\n";
  BuildRecord(input, &recorder);
  EXPECT_EQ(
      option_help,
      recorder.option_to_representative_option_[option_h]);
  rop_ptr = &recorder.representative_option_to_property_[option_help];
  EXPECT_EQ(argument_1, rop_ptr->option_argument_);

  // case 3.
  // bind -h to <arg 1>, with default value "42";
  // bind --help to ARG-2, with default value "43";
  input =
      "Usage:\n"
      " utility_name -c <some arg>\n"
      " Options:\n"
      " -h <arg 1> [default: \"42\"]\n"
      " --help=ARG-2 [DeFaUlt:\"43\"]\n";
  
  BuildRecord(input, &recorder);
  rop_ptr = &recorder.representative_option_to_property_[option_h];
  EXPECT_EQ("42", rop_ptr->default_value_);
  rop_ptr = &recorder.representative_option_to_property_[option_help];
  EXPECT_EQ("43", rop_ptr->default_value_);
}

}  // namespace clidoc
