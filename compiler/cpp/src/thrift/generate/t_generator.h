/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef T_GENERATOR_H
#define T_GENERATOR_H
#define MSC_2015_VER 1900

#include <cstring>
#include <string>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <limits>
#include <sstream>
#include "thrift/common.h"
#include "thrift/logging.h"
#include "thrift/version.h"
#include "thrift/generate/t_generator_registry.h"
#include "thrift/parse/t_program.h"

/**
 * Base class for a thrift code generator. This class defines the basic
 * routines for code generation and contains the top level method that
 * dispatches code generation across various components.
 *
 */
class t_generator {
public:
  t_generator(t_program* program) {
    update_keywords();

    tmp_ = 0;
    indent_ = 0;
    program_ = program;
    program_name_ = get_program_name(program);
    escape_['\n'] = "\\n";
    escape_['\r'] = "\\r";
    escape_['\t'] = "\\t";
    escape_['"'] = "\\\"";
    escape_['\\'] = "\\\\";
  }

  virtual ~t_generator() {}

  /**
   * Framework generator method that iterates over all the parts of a program
   * and performs general actions. This is implemented by the base class and
   * should not normally be overwritten in the subclasses.
   */
  virtual void generate_program();

  const t_program* get_program() const { return program_; }

  void generate_docstring_comment(std::ostream& out,
                                  const std::string& comment_start,
                                  const std::string& line_prefix,
                                  const std::string& contents,
                                  const std::string& comment_end);

  static void parse_options(const std::string& options, std::string& language,
                     std::map<std::string, std::string>& parsed_options);

  /**
   * check whether sub-namespace declaraction is used by generator.
   * e.g. allow
   * namespace py.twisted bar
   * to specify namespace to use when -gen py:twisted is specified.
   * Will be called with subnamespace, i.e. is_valid_namespace("twisted")
   * will be called for the above example.
   */
  static bool is_valid_namespace(const std::string& sub_namespace) {
    (void)sub_namespace;
    return false;
  }

  /**
   * Escape string to use one in generated sources.
   */
  virtual std::string escape_string(const std::string& in) const;

  std::string get_escaped_string(t_const_value* constval) {
    return escape_string(constval->get_string());
  }

  /**
   * Check if all identifiers are valid for the target language
   * See update_keywords()
   */
  virtual void validate_input() const;

protected:
  virtual std::set<std::string> lang_keywords() const;

  /**
   * Call this from constructor if you implement lang_keywords()
   */
  void update_keywords() {
    keywords_ = lang_keywords();
  }

  /**
   * A list of reserved words that cannot be used as identifiers.
   */
  std::set<std::string> keywords_;

  virtual void validate_id(const std::string& id) const;

  virtual void validate(t_enum const* en) const;
  virtual void validate(t_enum_value const* en_val) const;
  virtual void validate(t_typedef const* td) const;
  virtual void validate(t_const const* c) const;
  virtual void validate(t_service const* s) const;
  virtual void validate(t_struct const* c) const;
  virtual void validate(t_field const* f) const;
  virtual void validate(t_function const* f) const;

  template <typename T>
  void validate(const std::vector<T>& list) const;

  /**
   * Optional methods that may be implemented by subclasses to take necessary
   * steps at the beginning or end of code generation.
   */

  virtual void init_generator() {}
  virtual void close_generator() {}

  virtual void generate_consts(std::vector<t_const*> consts);

  /**
   * Pure virtual methods implemented by the generator subclasses.
   */

  virtual void generate_typedef(t_typedef* ttypedef) = 0;
  virtual void generate_enum(t_enum* tenum) = 0;
  virtual void generate_const(t_const* tconst) { (void)tconst; }
  virtual void generate_struct(t_struct* tstruct) = 0;
  virtual void generate_service(t_service* tservice) = 0;
  virtual void generate_forward_declaration(t_struct*) {}
  virtual void generate_xception(t_struct* txception) {
    // By default exceptions are the same as structs
    generate_struct(txception);
  }

  /**
   * Method to get the program name, may be overridden
   */
  virtual std::string get_program_name(t_program* tprogram) { return tprogram->get_name(); }

  /**
   * Method to get the service name, may be overridden
   */
  virtual std::string get_service_name(t_service* tservice) { return tservice->get_name(); }

  /**
   * Get the current output directory
   */
  virtual std::string get_out_dir() const {
    if (program_->is_out_path_absolute()) {
      return program_->get_out_path() + "/";
    }

    return program_->get_out_path() + out_dir_base_ + "/";
  }

  /**
   * Creates a unique temporary variable name, which is just "name" with a
   * number appended to it (i.e. name35)
   */
  std::string tmp(std::string name) {
    std::ostringstream out;
    out << name << tmp_++;
    return out.str();
  }

  /**
   * Generates a comment about this code being autogenerated, using C++ style
   * comments, which are also fair game in Java / PHP, yay!
   *
   * @return C-style comment mentioning that this file is autogenerated.
   */
  virtual std::string autogen_comment() {
    return std::string("/**\n") + " * " + autogen_summary() + "\n" + " *\n"
           + " * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING\n"
           + " *  @generated\n" + " */\n";
  }

  virtual std::string autogen_summary() {
    return std::string("Autogenerated by Thrift Compiler (") + THRIFT_VERSION + ")";
  }

  /**
   * Indentation level modifiers
   */

  void indent_up() { ++indent_; }

  void indent_down() { --indent_; }

  /**
  * Indentation validation helper
  */
  int indent_count() { return indent_; }

  void indent_validate( int expected, const char * func_name) {
    if (indent_ != expected) { 
      pverbose("Wrong indent count in %s: difference = %i \n", func_name, (expected - indent_));
    }
  }

  /**
   * Indentation print function
   */
  std::string indent() {
    std::string ind = "";
    int i;
    for (i = 0; i < indent_; ++i) {
      ind += indent_str();
    }
    return ind;
  }

  /**
   * Indentation utility wrapper
   */
  std::ostream& indent(std::ostream& os) { return os << indent(); }

  /**
   * Capitalization helpers
   */
  std::string capitalize(std::string in) {
    in[0] = toupper(in[0]);
    return in;
  }
  std::string decapitalize(std::string in) {
    in[0] = tolower(in[0]);
    return in;
  }
  static std::string lowercase(std::string in) {
    for (size_t i = 0; i < in.size(); ++i) {
      in[i] = tolower(in[i]);
    }
    return in;
  }
  static std::string uppercase(std::string in) {
    for (size_t i = 0; i < in.size(); ++i) {
      in[i] = toupper(in[i]);
    }
    return in;
  }

  /**
   * Transforms a camel case string to an equivalent one separated by underscores
   * e.g. aMultiWord -> a_multi_word
   *      someName   -> some_name
   *      CamelCase  -> camel_case
   *      name       -> name
   *      Name       -> name
   */
  std::string underscore(std::string in) {
    in[0] = tolower(in[0]);
    for (size_t i = 1; i < in.size(); ++i) {
      if (isupper(in[i])) {
        in[i] = tolower(in[i]);
        in.insert(i, "_");
      }
    }
    return in;
  }

  /**
    * Transforms a string with words separated by underscores to a camel case equivalent
    * e.g. a_multi_word -> aMultiWord
    *      some_name    ->  someName
    *      name         ->  name
    */
  std::string camelcase(std::string in) {
    std::ostringstream out;
    bool underscore = false;

    for (size_t i = 0; i < in.size(); i++) {
      if (in[i] == '_') {
        underscore = true;
        continue;
      }
      if (underscore) {
        out << (char)toupper(in[i]);
        underscore = false;
        continue;
      }
      out << in[i];
    }

    return out.str();
  }

  const std::string emit_double_as_string(const double value) {
      std::stringstream double_output_stream;
      // sets the maximum precision: http://en.cppreference.com/w/cpp/io/manip/setprecision
      // sets the output format to fixed: http://en.cppreference.com/w/cpp/io/manip/fixed (not in scientific notation)
      double_output_stream << std::setprecision(std::numeric_limits<double>::digits10 + 1);

      #ifdef _MSC_VER
          // strtod is broken in MSVC compilers older than 2015, so std::fixed fails to format a double literal.
          // more details: https://blogs.msdn.microsoft.com/vcblog/2014/06/18/
          //               c-runtime-crt-features-fixes-and-breaking-changes-in-visual-studio-14-ctp1/
          //               and
          //               http://www.exploringbinary.com/visual-c-plus-plus-strtod-still-broken/
          #if _MSC_VER >= MSC_2015_VER
              double_output_stream << std::fixed;
          #endif
      #else
          double_output_stream << std::fixed;
      #endif

      double_output_stream << value;

      return double_output_stream.str();
  }

public:
  /**
   * Get the true type behind a series of typedefs.
   */
  static const t_type* get_true_type(const t_type* type) { return type->get_true_type(); }
  static t_type* get_true_type(t_type* type) { return type->get_true_type(); }

protected:
  /**
   * The program being generated
   */
  t_program* program_;

  /**
   * Quick accessor for formatted program name that is currently being
   * generated.
   */
  std::string program_name_;

  /**
   * Quick accessor for formatted service name that is currently being
   * generated.
   */
  std::string service_name_;

  /**
   * Output type-specifc directory name ("gen-*")
   */
  std::string out_dir_base_;

  /**
   * Map of characters to escape in string literals.
   */
  std::map<char, std::string> escape_;

  virtual std::string indent_str() const {
    return "  ";
  }

private:
  /**
   * Current code indentation level
   */
  int indent_;

  /**
   * Temporary variable counter, for making unique variable names
   */
  int tmp_;
};

template<typename _CharT, typename _Traits = std::char_traits<_CharT> >
class template_ofstream_with_content_based_conditional_update : public std::ostringstream {
public:
  template_ofstream_with_content_based_conditional_update(): contents_written(false) {}

  template_ofstream_with_content_based_conditional_update(std::string const& output_file_path_)
  : output_file_path(output_file_path_), contents_written(false) {}

  ~template_ofstream_with_content_based_conditional_update() {
    if (!contents_written) {
      close();
    }
  }

  void open(std::string const& output_file_path_) {
    output_file_path = output_file_path_;
    clear_buf();
    contents_written = false;
  }

  void close() {
    if (contents_written || output_file_path == "")
      return;

    if (!is_readable(output_file_path)) {
      dump();
      return;
    }

    std::ifstream old_file;
    old_file.exceptions(old_file.exceptions() | std::ifstream::badbit | std::ifstream::failbit);
    old_file.open(output_file_path.c_str(), std::ios::in);

    if (old_file) {
      std::ostringstream oss;
      oss << old_file.rdbuf();
      std::string const old_file_contents(oss.str());
      old_file.close();

      if (old_file_contents != str()) {
        dump();
      }
    }
  }

protected:
  void dump() {
    std::ofstream out_file;
    out_file.exceptions(out_file.exceptions() | std::ofstream::badbit | std::ofstream::failbit);
    try {
      out_file.open(output_file_path.c_str(), std::ios::out);
    }
    catch (const std::ios_base::failure& e) {
      ::failure("failed to write the output to the file '%s', details: '%s'", output_file_path.c_str(), e.what());
    }
    out_file << str();
    out_file.close();
    clear_buf();
    contents_written = true;
  }

  void clear_buf() {
    str(std::string());
  }

  static bool is_readable(std::string const& file_name) {
    return static_cast<bool>(std::ifstream(file_name.c_str()));
  }

private:
  std::string output_file_path;
  bool contents_written;
};
typedef template_ofstream_with_content_based_conditional_update<char> ofstream_with_content_based_conditional_update;

#endif
