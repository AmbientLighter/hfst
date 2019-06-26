// Copyright (c) 2016 University of Helsinki
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
// See the file COPYING included with this distribution for more
// information.

//! @file XfstCompiler.cc
//!
//! @brief Implemetation of class encapsulating yacc and flex parsers for XFST
//!        scripts

#if HAVE_CONFIG_H
#   include <config.h>
#endif

#include <string>
#include <map>
#include <queue>
#include <stack>

using std::string;
using std::map;
using std::queue;
using std::stack;

#include <cstdio>
#include <cstdlib>

#include <cstdarg>

#ifndef _WIN32
  #include <glob.h>
#else
  #pragma warning(disable: 4161)
  #pragma warning(disable: 4103)
  #include <windows.h>
#endif  // WINDOWS

// TODO!!!
#include "hfst-string-conversions.h"

#include "XfstCompiler.h"
#include "xfst-utils.h"

#ifdef YACC_USE_PARSER_H_EXTENSION
  #include "xfst-parser.h"
#else
  #include "xfst-parser.hh"
#endif

#include "HfstStrings2FstTokenizer.h"
#include "HfstPrintDot.h"

#ifdef HAVE_READLINE
  #include <readline/readline.h>
  #include <readline/history.h>
#endif

#ifndef DEBUG_MAIN
extern FILE* hxfstin;
extern int hxfstparse(void);

extern int hxfstlex(void);
struct yy_buffer_state;
typedef yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE hxfst_scan_string(const char*);
extern void hxfst_delete_buffer(YY_BUFFER_STATE);

#include "implementations/HfstIterableTransducer.h"

using hfst::implementations::HfstIterableTransducer;
using hfst::implementations::HfstTransition;
#ifdef WINDOWS
  using hfst::hfst_fprintf_console;
#endif

#define GET_TOP(x) HfstTransducer * x = this->top(); if ((x) == NULL) { xfst_lesser_fail(); return *this; }
#define PROMPT_AND_RETURN_THIS prompt(); return *this;
#define PRINT_INFO_PROMPT_AND_RETURN_THIS print_transducer_info(); prompt(); return *this;
#define IF_NULL_PROMPT_AND_RETURN_THIS(x) if (x == NULL) { if(variables_["quit-on-fail"] == "ON") { this->fail_flag_ = true; } prompt(); return *this; }
#define MAYBE_MINIMIZE(x) x->optimize();
#define MAYBE_ASSERT(assertion, value) if (!value && ((variables_["assert"] == "ON" || assertion) && (variables_["quit-on-fail"] == "ON"))) { this->fail_flag_ = true; }
#define MAYBE_QUIT if(variables_["quit-on-fail"] == "ON") { this->fail_flag_ = true; }
#define CHECK_FILENAME(x) if (! this->check_filename(x)) { return *this; }

#ifdef PYTHON_BINDINGS
#define ERROR(x) { std::ostringstream oss(""); oss x; hfst::py_print_error(oss.str().c_str(),true); }
#define OUTPUT(x) { std::ostringstream oss(""); oss x; hfst::py_print_output(oss.str().c_str(), true); }
#define OUTPUT_LINE(x) { std::ostringstream oss(""); oss x; hfst::py_print_output(oss.str().c_str(), false); }
#define OUTPUT_END hfst::py_print_output("", true);
#define EMPTY_STACK hfst::py_print_error("Empty stack.", true);
#else
#define ERROR(x) error() x << std::endl;
#define OUTPUT(x) output() x << std::endl;
#define OUTPUT_LINE(x) output() x;
#define OUTPUT_END output() << std::endl;
#define EMPTY_STACK error() << "Empty stack." << std::endl;
#endif

#define WEIGHT_PRECISION "5"
#define LOOKUP_CYCLE_CUTOFF "5"
#define PRINT_WORDS_CYCLE_CUTOFF "5"

#include "xfst_help_message.h"

namespace hfst {
namespace xfst {

  // whether we need to reset the lexc parser before reading lexc
  static bool has_lexc_been_read_ = false;

  static std::map<std::string, std::string> variable_explanations_;

  static void initialize_variable_explanations()
  {
    variable_explanations_["assert"] = "quit the application if test result is 0 and quit-on-fail is ON";
    variable_explanations_["att-epsilon"] = "epsilon symbol used when reading from att files";
    variable_explanations_["char-encoding"] = "character encoding used";
    variable_explanations_["copyright-owner"] = "";
    variable_explanations_["directory"] = "<NOT IMPLEMENTED>";
    variable_explanations_["encode-weights"] = "encode weights when minimizing";
    variable_explanations_["flag-is-epsilon"] = "treat flag diacritics as epsilons in composition";
    variable_explanations_["harmonize-flags"] = "harmonize flag diacritics before composition";
    variable_explanations_["hopcroft-min"] = "use hopcroft's minimization algorithm";
    variable_explanations_["lexc-minimize-flags"] = "if 'lexc-with-flags' == ON, minimize number of flags";
    variable_explanations_["lexc-rename-flags"] = "if 'lexc-minimize-flags' == ON, rename flags";
    variable_explanations_["lexc-with-flags"] = "use flags to hyperminimize result from lexc files";
    variable_explanations_["maximum-weight"] = "maximum weight of paths printed in apply";
    variable_explanations_["minimal"] = "minimize networks after operations";
    variable_explanations_["name-nets"] = "stores the name of the network when using 'define'";
    variable_explanations_["obey-flags"] = "obey flag diacritic constraints";
    variable_explanations_["precision"] = "todo: precision to use when printing weights";
    variable_explanations_["print-foma-sigma"] = "print identities as '@'";
    variable_explanations_["print-pairs"] = "show both sides (upper and lower) of labels";
    variable_explanations_["print-sigma"] = "show sigma when printing a network";
    variable_explanations_["print-space"] = "insert a space between symbols when printing words";
    variable_explanations_["print-weight"] = "show weights when printing words or networks";
    variable_explanations_["quit-on-fail"] = "quit the application if a command cannot be executed";
    variable_explanations_["quote-special"] = "enclose special characters in double quotes";
    variable_explanations_["random-seed"] = "<EXPLANATION MISSING>";
    variable_explanations_["recode-cp1252"] = "<NOT SUPPORTED>";
    variable_explanations_["recursive-define"] = "<EXPLANATION MISSING>";
    variable_explanations_["retokenize"] = "retokenize regular expressions in 'compile-replace'";
    variable_explanations_["show-flags"] = "show flag diacritics when printing";
    variable_explanations_["sort-arcs"] = "<NOT IMPLEMENTED>";
    variable_explanations_["use-timer"] = "<NOT IMPLEMENTED>";
    variable_explanations_["verbose"] = "print more information";
    variable_explanations_["xerox-composition"] = "treat flag diacritics as ordinary symbols in composition";
  }

  static const char * APPLY_END_STRING = "<ctrl-d>";
  
    XfstCompiler::XfstCompiler() :
        use_readline_(false),
        read_interactive_text_from_stdin_(false),
        output_to_console_(false),
        xre_(hfst::TROPICAL_OPENFST_TYPE),
        lexc_(hfst::TROPICAL_OPENFST_TYPE),
        format_(hfst::TROPICAL_OPENFST_TYPE),
        verbose_(false),
        verbose_prompt_(false),
        latest_regex_compiled(NULL),
        quit_requested_(false),
        fail_flag_(false),
        output_(&std::cout),
        error_(&std::cerr),
#ifdef WINDOWS
        winoss_stderr_(std::ostringstream()),
        winoss_stdout_(std::ostringstream()),
#endif
	restricted_mode_(false),
	inspect_net_supported_(true)
    {
        xre_.set_expand_definitions(true);
        xre_.set_verbosity(this->verbose_);
        xre_.set_flag_harmonization(false);
        xre_.set_error_stream(this->error_);
        lexc_.setVerbosity(this->verbose_ ? 2 : 0);
        lexc_.set_error_stream(this->error_);
        hfst::set_xerox_composition(true);
        variables_["assert"] = "OFF";
        variables_["att-epsilon"] = "@0@ | @_EPSILON_SYMBOL_@";
        variables_["char-encoding"] = "UTF-8";
        variables_["copyright-owner"] = "Copyleft (c) University of Helsinki";
        variables_["directory"] = "OFF";
        variables_["encode-weights"] = "OFF";
        variables_["flag-is-epsilon"] = "OFF";
        variables_["harmonize-flags"] = "OFF";
        variables_["hopcroft-min"] = "ON";
        variables_["lexc-minimize-flags"] = "OFF";
        variables_["lexc-rename-flags"] = "OFF";
        variables_["lexc-with-flags"] = "OFF";
        variables_["lookup-cycle-cutoff"] = LOOKUP_CYCLE_CUTOFF;
        variables_["maximum-weight"] = "OFF";
        variables_["minimal"] = "ON";
        variables_["name-nets"] = "OFF";
        variables_["obey-flags"] = "ON";
        variables_["precision"] = WEIGHT_PRECISION;
        variables_["print-foma-sigma"] = "OFF";
        variables_["print-pairs"] = "OFF";
        variables_["print-sigma"] = "OFF";
        variables_["print-space"] = "OFF";
        variables_["print-weight"] = "OFF";
        variables_["print-words-cycle-cutoff"] = PRINT_WORDS_CYCLE_CUTOFF;
        variables_["quit-on-fail"] = "OFF";
        variables_["quote-special"] = "OFF";
        variables_["random-seed"] = "ON";
        variables_["recode-cp1252"] = "NEVER";
        variables_["recursive-define"] = "OFF";
        variables_["retokenize"] = "ON";
        variables_["show-flags"] = "OFF";
        variables_["sort-arcs"] = "MAYBE";
        variables_["use-timer"] = "OFF";
        variables_["verbose"] = "OFF";
        variables_["xerox-composition"] = "ON";
        initialize_variable_explanations();
        prompt();
      }

    XfstCompiler::XfstCompiler(ImplementationType impl) :
        use_readline_(false),
        read_interactive_text_from_stdin_(false),
        output_to_console_(false),
        xre_(impl),
        lexc_(impl),
        format_(impl),
        verbose_(false),
        verbose_prompt_(false),
        latest_regex_compiled(NULL),
        quit_requested_(false),
        fail_flag_(false),
        output_(&std::cout),
        error_(&std::cerr),
#ifdef WINDOWS
        winoss_stderr_(std::ostringstream()),
        winoss_stdout_(std::ostringstream()),
#endif
	restricted_mode_(false),
	inspect_net_supported_(true)
    {
        xre_.set_expand_definitions(true);
        xre_.set_verbosity(this->verbose_);
        xre_.set_flag_harmonization(false);
        xre_.set_error_stream(this->error_);
        lexc_.setVerbosity(this->verbose_ ? 2 : 0);
        lexc_.set_error_stream(this->error_);
        hfst::set_xerox_composition(true);
        variables_["assert"] = "OFF";
        variables_["att-epsilon"] = "@0@ | @_EPSILON_SYMBOL_@";
        variables_["char-encoding"] = "UTF-8";
        variables_["copyright-owner"] = "Copyleft (c) University of Helsinki";
        variables_["directory"] = "OFF";
        variables_["encode-weights"] = "OFF";
        variables_["flag-is-epsilon"] = "OFF";
        variables_["harmonize-flags"] = "OFF";
        variables_["hopcroft-min"] = "ON";
        variables_["lexc-minimize-flags"] = "OFF";
        variables_["lexc-rename-flags"] = "OFF";
        variables_["lexc-with-flags"] = "OFF";
        variables_["lookup-cycle-cutoff"] = LOOKUP_CYCLE_CUTOFF;
        variables_["maximum-weight"] = "OFF";
        variables_["minimal"] = "ON";
        variables_["name-nets"] = "OFF";
        variables_["obey-flags"] = "ON";
        variables_["precision"] = WEIGHT_PRECISION;
        variables_["print-foma-sigma"] = "OFF";
        variables_["print-pairs"] = "OFF";
        variables_["print-sigma"] = "OFF";
        variables_["print-space"] = "OFF";
        variables_["print-weight"] = "OFF";
        variables_["print-words-cycle-cutoff"] = PRINT_WORDS_CYCLE_CUTOFF;
        variables_["quit-on-fail"] = "OFF";
        variables_["quote-special"] = "OFF";
        variables_["random-seed"] = "ON";
        variables_["recode-cp1252"] = "NEVER";
        variables_["recursive-define"] = "OFF";
        variables_["retokenize"] = "ON";
        variables_["show-flags"] = "OFF";
        variables_["sort-arcs"] = "MAYBE";
        variables_["use-timer"] = "OFF";
        variables_["verbose"] = "OFF";
        variables_["xerox-composition"] = "ON";
        initialize_variable_explanations();
        prompt();
      }

  XfstCompiler::~XfstCompiler()
  {
    while(!stack_.empty())
      {
        delete(stack_.top());
        stack_.pop();
      }
    for (std::map<std::string, hfst::HfstTransducer*>::const_iterator it
           = definitions_.begin(); it != definitions_.end(); it++)
      {
        delete it->second;
      }
    if (latest_regex_compiled != NULL)
      {
        delete latest_regex_compiled;
      }
  }

  int XfstCompiler::xfst_fclose(FILE * f, const char * name)
  {
    int retval = fclose(f);
    if (retval != 0)
      {
	ERROR(<< "could not close file " << name);
        xfst_fail();
      }
    return retval;
  }

  FILE * XfstCompiler::xfst_fopen(const char* path, const char* mode)
  {
    FILE * f = hfst::hfst_fopen(path, mode);
    if (f == NULL)
      {
        ERROR(<< "could not open file " << path);
        xfst_fail();
      }
    return f;
  }

  void XfstCompiler::set_error_stream(std::ostream & os)
  {
    error_ = &os;
    //this->xre_.set_error_stream(this->error_);
    this->lexc_.set_error_stream(this->error_);
  }

  std::ostream & XfstCompiler::get_error_stream()
  {
    return *error_;
  }

  void XfstCompiler::set_output_stream(std::ostream & os)
  {
    output_ = &os;
  }

  std::ostream & XfstCompiler::get_output_stream()
  {
    return *output_;
  }

  int XfstCompiler::get_precision()
  {
    std::istringstream iss(variables_["precision"]);
    int retval;
    iss >> retval;
    return retval;
  }

  void XfstCompiler::xfst_fail()
  {
    if (variables_["quit-on-fail"] == "ON")
      {
        this->fail_flag_ = true;
      }
  }

  void XfstCompiler::xfst_lesser_fail()
  {
    if (variables_["quit-on-fail"] == "ON" && !read_interactive_text_from_stdin_)
      {
        this->fail_flag_ = true;
      }
  }


    XfstCompiler&
    XfstCompiler::add_prop_line(char* line)
      {
        char* name = static_cast<char*>(malloc(sizeof(char)*strlen(line)));
        char* p = line;
        char* n = name;
        while ((*p != '\0') && (*p != ':'))
          {
            *n = *p;
            n++;
            p++;
          }
        *n = '\0';
        if (*p == '\0')
          {
            assert(*p != '\0');
            //hfst_fprintf(errorstream_, "no colon in line\n");
	    ERROR(<< "no colon in line");
          }
        p++;
        while (isspace(*p))
          {
            p++;
          }
        char* value = strdup(p);
        properties_[name] =  value;
        return *this;
      }

  const char *
  XfstCompiler::get_print_symbol(const char* symbol)
  {
    if (variables_["show-flags"] == "OFF" &&  // show no flags
        FdOperation::is_diacritic(symbol))    // symbol is flag
      {
        return "";  // print nothing
      }
    if (strcmp(hfst::internal_epsilon.c_str(), symbol) == 0)
      {
        return "";
      }
    if (strcmp(hfst::internal_unknown.c_str(), symbol) == 0 ||
        strcmp(hfst::internal_identity.c_str(), symbol) == 0)
      {
        return "?";
      }
    return symbol;
  }

  static HfstOneLevelPaths extract_output_paths(const HfstTwoLevelPaths & paths)
  {
    HfstOneLevelPaths retval;
    for (hfst::HfstTwoLevelPaths::const_iterator it = paths.begin();
         it != paths.end(); it++)
      {
        hfst::StringVector new_path;
        hfst::StringPairVector path = it->second;
        for (hfst::StringPairVector::const_iterator p = path.begin();
             p != path.end(); p++)
          {
            if (p->second != "@0@" &&
                !hfst::is_epsilon(p->second))
              {
                if (hfst::is_unknown(p->second))
                  new_path.push_back("?");
                else
                  new_path.push_back(p->second);
              }
          }
        retval.insert(std::pair<float, StringVector>(it->first, new_path));
      }
    return retval;
  }

  bool is_valid_string(const StringVector & sv)
  {
    // map features to latest values
    std::map<std::string, std::string> values;
    // and keep track of features whose values have been negatively set
    std::set<std::string> negative_values;

    for (StringVector::const_iterator it = sv.begin(); it != sv.end(); it++)
      {
        if (FdOperation::is_diacritic(*it))
          {
            std::string opstr = FdOperation::get_operator(*it);
            assert(opstr.size() == 1);
            char op = opstr[0];
            std::string feat = FdOperation::get_feature(*it);
            std::string val = FdOperation::get_value(*it);

            bool is_negatively_set = (negative_values.find(feat) != negative_values.end());
            
            switch(op) {
            case 'P': // positive set
              values[feat] = val;
              break;
            case 'N': // negative set
              values[feat] = val;
              negative_values.insert(feat);
              break;
            case 'R': // require
              if (val == "") { // empty require
                if (values[feat] == "")
                  { return false; }
                else { // nonempty require
                  if (is_negatively_set || (values[feat] != val)) { return false; }
                }
              }
              break;
            case 'D': // disallow
              if (val == "") { // empty disallow
                if (values[feat] != "") {
                  return false;
                }
              }
              else {
                if ((!is_negatively_set) && (values[feat] == val)) { // nonempty disallow
                  return false;
                }
              }
              break;
            case 'C': // clear
              values[feat] = "";
              break;
            case 'U': // unification
              if(values[feat] == "" || /* if the feature is unset or */
                 ((!is_negatively_set) && (values[feat] == val)) || /* the feature is at
                                                                     this value already
                                                                     or */
                 (is_negatively_set &&
                  (values[feat] != val)) /* the feature is
                                            negatively set
                                            to something
                                            else */
                 )
                {
                  values[feat] = val;
                }
              else
                {
                  return false;
                }
              break;
            default:
              throw "Error in is_valid_string"; // for the compiler's peace of mind
              break;
            }
          }
      }
    return true;
  }

  // LEAVE
  bool
  XfstCompiler::print_paths
  (const hfst::HfstOneLevelPaths &paths,
   std::ostream * oss /* =cout */,
   int n /* = -1*/)
  {
    bool retval = false; // if anything was printed
    oss->precision(get_precision());

    // go through at most n paths
    for (hfst::HfstOneLevelPaths::const_iterator it = paths.begin();
         n != 0 && it != paths.end(); it++)
      {
        hfst::StringVector path = it->second;
        bool something_printed = false;  // to control printing spaces

        if ((variables_["obey-flags"] == "ON") && !is_valid_string(path))
          continue;

        retval = true; // something will be printed

        // go through the path
        for (hfst::StringVector::const_iterator p = path.begin();
             p != path.end(); p++)
          {
            const char * print_symbol = get_print_symbol(p->c_str());

            // see if symbol separator (space) is needed
            if (variables_["print-space"] == "ON" &&  // print space required
                something_printed &&                  // not first symbol shown
                strcmp(print_symbol, "") != 0)        // something to show
              {
                *oss << " "; // hfst_fprintf(outfile, " ");
              }

            *oss << std::string(print_symbol); // hfst_fprintf(outfile, "%s", print_symbol);

            if (strcmp(print_symbol, "") != 0) {
              something_printed = true;
            }

          } // path gone through
        
        // if needed, print the weight
        if (variables_["print-weight"] == "ON")
          {
            //hfst_fprintf(outfile, "\t");
            //hfst_print_weight(outfile, it->first);
            *oss << "\t" << std::fixed << it->first;
          }

        *oss << std::endl; // hfst_fprintf(outfile, "\n");
        --n;

      } // at most n paths gone through

    //hfst_fprintf(outfile, oss->str().c_str()); // TESTING

    return retval;
  }

  // LEAVE
  bool
  XfstCompiler::print_paths
  (const hfst::HfstTwoLevelPaths &paths,
   std::ostream * oss /* =cout */,
   int n /* = -1*/)
  {
    bool retval = false; // if anything was printed
    oss->precision(get_precision());

    // go through at most n paths
    for (hfst::HfstTwoLevelPaths::const_iterator it = paths.begin();
         n != 0 && it != paths.end(); it++)
      {
        hfst::StringPairVector path = it->second;
        bool something_printed = false;  // to control printing spaces

        if (variables_["obey-flags"] == "ON")
          {
            StringVector path_input = hfst::symbols::to_string_vector(path, true /*input side*/);
            if (!is_valid_string(path_input))
              continue;
          }

        retval = true; // something will be printed

        // go through the path
        for (hfst::StringPairVector::const_iterator p = path.begin();
             p != path.end(); p++)
          {
            const char * print_symbol = get_print_symbol(p->first.c_str());

            // see if symbol separator (space) is needed
            if (variables_["print-space"] == "ON" &&  // print space required
                something_printed &&                  // not first symbol shown
                strcmp(print_symbol, "") != 0)        // something to show
              {
                *oss << " "; // hfst_fprintf(outfile, " ");
              }

            *oss << std::string(print_symbol); // hfst_fprintf(outfile, "%s", print_symbol);

            if (strcmp(print_symbol, "") != 0) {
              something_printed = true;
            }

            print_symbol = get_print_symbol(p->second.c_str());
            
            // see if output symbol is needed
            if (strcmp(print_symbol, "") != 0 &&   // something to show
                p->first != p->second)             // input and output symbols differ
              {
                *oss << ":" << std::string(print_symbol); // hfst_fprintf(outfile, ":%s", print_symbol);
              }

          } // path gone through

        // if needed, print the weight
        if (variables_["print-weight"] == "ON")
          {
            // hfst_fprintf(outfile, "\t");
            // hfst_print_weight(outfile, it->first);
            *oss << "\t" << std::fixed << it->first;
          }

        *oss << std::endl; // hfst_fprintf(outfile, "\n");
        --n;

      } // at most n paths gone through

    //hfst_fprintf(outfile, oss->str().c_str()); // TESTING

    return retval;
  }

  static float string_to_float(const std::string & str)
  {
    std::istringstream iss(str);
    float f;
    iss >> f;
    return f;
  }

  static size_t string_to_size_t(const std::string & str)
  {
    std::istringstream iss(str);
    size_t size;
    iss >> size;
    return size;
  }

    XfstCompiler&
    XfstCompiler::lookup(char* line, HfstIterableTransducer * t)
      {
        char* token = strstrip(line);
        StringSet alpha = t->get_input_symbols();
        HfstTokenizer tok;
        for (StringSet::const_iterator it = alpha.begin();
             it != alpha.end(); it++)
          {
            tok.add_multichar_symbol(*it);
          }
        StringVector lookup_path = tok.tokenize_one_level(std::string(token));
        free(token);

        size_t cutoff = -1;
        if (t->is_lookup_infinitely_ambiguous(lookup_path, variables_["obey-flags"] == "ON"))
          {
            cutoff = string_to_size_t(variables_["lookup-cycle-cutoff"]);
            if (verbose_)
              {
                ERROR(<< "warning: lookup is infinitely ambiguous, limiting the number of cycles to " << cutoff);
              }
          }

        HfstTwoLevelPaths results;

        if (variables_["maximum-weight"] == "OFF")
          t->lookup(lookup_path, results, &cutoff, NULL, -1 /*max_number*/, (variables_["obey-flags"] == "ON"));
        else
          {
            float max_weight = string_to_float(variables_["maximum-weight"]);
            t->lookup(lookup_path, results, &cutoff, &max_weight, -1 /*max_number*/, (variables_["obey-flags"] == "ON"));
          }

        bool printed = false; // if anything was printed

        if (variables_["print-pairs"] == "OFF")
          {
            HfstOneLevelPaths paths = extract_output_paths(results);
            printed = this->print_paths(paths, &output());
          }
        else
          {
            printed = this->print_paths(results, &output());
          }

        if (!printed)
          {
            OUTPUT(<< "???");
          }
        return *this;
      }

  std::ostream & XfstCompiler::output()
  {
    return *output_;
  }

  std::ostream & XfstCompiler::error()
  {
    return *error_;
  }


    XfstCompiler&
    XfstCompiler::lookup(char* line, const HfstTransducer * t, size_t cutoff)
      {
        char* token = strstrip(line);
        HfstOneLevelPaths * paths = NULL;

        if (variables_["obey-flags"] == "ON") {
          paths = t->lookup_fd(std::string(token), cutoff);
        }
        else {
          paths = t->lookup(std::string(token), cutoff);
        }

        bool printed = this->print_paths(*paths, &output());
        if (!printed)
          {
            OUTPUT(<< "???");
          }

        delete paths;
        return *this;
      }

  XfstCompiler&
  XfstCompiler::lookup_optimize()
  {
    if (stack_.size() < 1)
      {
        EMPTY_STACK;
        xfst_lesser_fail();
        prompt();
        return *this;
      }

    HfstTransducer * t = stack_.top();

    hfst::ImplementationType to_format;
    if (t->get_type() == hfst::HFST_OL_TYPE ||
        t->get_type() == hfst::HFST_OLW_TYPE)
      {
	ERROR(<< "Network is already optimized for lookup.");
        //hfst_fprintf(warnstream_, "Network is already optimized for lookup.\n");
        prompt();
        return *this;
      }
    else if (t->get_type() == hfst::TROPICAL_OPENFST_TYPE ||
             t->get_type() == hfst::LOG_OPENFST_TYPE)
      {
        to_format = hfst::HFST_OLW_TYPE;
      }
    else
      {
        to_format = hfst::HFST_OL_TYPE;
      }

    if (verbose_)
      {
        ERROR( << "converting transducer type from " << hfst::implementation_type_to_format(t->get_type()) << " to "
	       << hfst::implementation_type_to_format(to_format) << ", this might take a while...");
    //hfst_fprintf(warnstream_, "converting transducer type from %s to %s, this might take a while...\n",
    //                hfst::implementation_type_to_format(t->get_type()),
    //                hfst::implementation_type_to_format(to_format));
      }

    std::stack<HfstTransducer*> temp;
    while(!stack_.empty())
      {
        stack_.top()->convert(to_format);
        temp.push(stack_.top());
        stack_.pop();
      }
    while(!temp.empty())
      {
        stack_.push(temp.top());
        temp.pop();
      }

    prompt();
    return *this;
  }

  XfstCompiler&
  XfstCompiler::remove_optimization()
  {
    if (stack_.size() < 1)
      {
        EMPTY_STACK;
        xfst_lesser_fail();
        prompt();
        return *this;
      }
    HfstTransducer * t = stack_.top();

    if (t->get_type() != hfst::HFST_OL_TYPE &&
        t->get_type() != hfst::HFST_OLW_TYPE)
      {
	ERROR(<< "Network is already in ordinary format.");
        //hfst_fprintf(warnstream_, "Network is already in ordinary format.\n");
        prompt();
        return *this;
      }

    if (verbose_)
      {
        ERROR(<< "converting transducer type from " << hfst::implementation_type_to_format(t->get_type()) << " to "
	      << hfst::implementation_type_to_format(format_) << ", this might take a while...");
        //hfst_fprintf(warnstream_, "converting transducer type from %s to %s, this might take a while...\n",
        //             hfst::implementation_type_to_format(t->get_type()),
        //             hfst::implementation_type_to_format(format_));
        if (! hfst::HfstTransducer::is_safe_conversion(t->get_type(), format_))
          {
	    ERROR(<< "warning: converting from weighted to unweighted format, loss of information is possible");
            //hfst_fprintf(warnstream_, "warning: converting from weighted to unweighted format, loss of information is possible\n");
          }
      }

    std::stack<HfstTransducer*> temp;
    while(!stack_.empty())
      {
        stack_.top()->convert(format_);
        temp.push(stack_.top());
        stack_.pop();
      }
    while(!temp.empty())
      {
        stack_.push(temp.top());
        temp.pop();
      }

    prompt();
    return *this;
  }


    XfstCompiler&
    XfstCompiler::apply_down_line(char* line) // apply_up_line -> apply_down_line
      {
        if (stack_.size() < 1)
          {
            EMPTY_STACK;
            xfst_lesser_fail();
            prompt();
            return *this;
          }
        HfstTransducer * t = stack_.top();
        if (t->get_type() != hfst::HFST_OL_TYPE && t->get_type() != hfst::HFST_OLW_TYPE)
          {
            //hfst_fprintf(warnstream_, "lookup might be slow, consider 'convert net'\n");
            HfstIterableTransducer fsm(*t);
            return this->lookup(line, &fsm);
          }

        size_t ol_cutoff = string_to_size_t(variables_["lookup-cycle-cutoff"]); // -1; fix this
        StringVector foo; // this gets ignored by ol transducer's is_lookup_infinitely_ambiguous
        if (t->is_lookup_infinitely_ambiguous(foo))
          {
            ol_cutoff = string_to_size_t(variables_["lookup-cycle-cutoff"]);;
            if (verbose_)
              {
                ERROR(<< "warning: transducer is infinitely ambiguous, limiting number of cycles to " << ol_cutoff);
            //hfst_fprintf(warnstream_,
            //                "warning: transducer is infinitely ambiguous, limiting number of cycles to " SIZE_T_SPECIFIER "\n", ol_cutoff);
              }
          }
        
        return this->lookup(line, t, ol_cutoff);
      }

    XfstCompiler&
    XfstCompiler::apply_up_line(char* line) // apply_down_line -> apply_up_line
      {
        GET_TOP(t);
        // lookdown not yet implemented in HFST
        if (verbose_)
          {
	    ERROR(<< "warning: apply up not implemented, inverting transducer and performing apply down");
	    ERROR(<< "for faster performance, invert and minimize top network and do apply down instead");
          }
        t = new HfstTransducer(*(stack_.top()));
        t->invert().minimize(); // the user has been warned for possible slow performance
        HfstIterableTransducer fsm(*t);
        this->lookup(line, &fsm);
        delete t;
        return *this;
      }

    XfstCompiler&
    XfstCompiler::apply_med_line(char* /*line*/)
      {
	ERROR(<< "Missing apply med");
        //hfst_fprintf(errorstream_, "Missing apply med %s:%d\n", __FILE__, __LINE__);
#if 0
        char* token = strstrip(line);
        HfstTransducer top = stack_.top();
        top.lookmed(token);
        for (each result)
          {
            print result;
          }
#endif
        return *this;
      }

    XfstCompiler&
    XfstCompiler::read_prop_line(char* line)
      {
        char* name = static_cast<char*>(malloc(sizeof(char)*strlen(line)));
        char* p = line;
        char* n = name;
        while ((*p != '\0') && (*p != ':'))
          {
            *n = *p;
            n++;
            p++;
          }
        *n = '\0';
        if (*p == '\0')
          {
            assert(*p != '\0');
            ERROR(<< "no colon in line");
            //hfst_fprintf(errorstream_, "no colon in line\n");
          }
        p++;
        while (isspace(*p))
          {
            p++;
          }
        char* value = strdup(p);
        properties_[name] = value;
        return *this;
      }
    
    XfstCompiler&
    XfstCompiler::add_props(FILE* infile)
      {
        char* line = 0;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, infile)) != -1)
          {
            add_prop_line(line);
          }
        free(line);
        PROMPT_AND_RETURN_THIS;
      }

    XfstCompiler&
    XfstCompiler::add_props(const char* indata)
      {
        char* s = strdup(indata);
        char* line = strtok(s, "\n");
        while (line != NULL)
          {
            add_prop_line(line);
            line = strtok(NULL, "\n");
          }
        free(s);
        PROMPT_AND_RETURN_THIS;
      }

  const char *
  XfstCompiler::get_apply_prompt(ApplyDirection direction)
  {
    if (! verbose_) {
      return "";
    }
    if (direction == APPLY_UP_DIRECTION) {
      return "apply up> ";
    }
    else if (direction == APPLY_DOWN_DIRECTION) {
      return "apply down> ";
    }
    return "";
  }

  int XfstCompiler::unknown_command(const char * s)
  {
    if (this->variables_["quit-on-fail"] == "ON")
      {
        if (verbose_)
          {
            ERROR(<< "Command " << std::string(s) << " is not recognised.");
            //fprintf(stderr, "Command %s is not recognised.\n", s);
          }
        return 1;
      }
    ERROR(<< "Command " << std::string(s) << " is not recognised.");
    //fprintf(stderr, "Command %s is not recognised.\n", s);
    this->prompt();
    return 0;
  }

  bool XfstCompiler::get_fail_flag() const
  {
    return this->fail_flag_;
  }

  bool XfstCompiler::quit_requested() const
  {
    return this->quit_requested_;
  }

  XfstCompiler&
  XfstCompiler::apply(FILE* infile, ApplyDirection direction)
      {
        if (stack_.size() < 1)
          {
            EMPTY_STACK;
            xfst_lesser_fail();
            prompt();
            return *this;
          }
        HfstTransducer * t = stack_.top();
        size_t ol_cutoff = string_to_size_t(variables_["lookup-cycle-cutoff"]); ; // -1; fix this // number of cycles needs to be limited for an infinitely ambiguous ol transducer
                               // because it doesn't support is_lookup_infinitely_ambiguous(const string &)

        HfstIterableTransducer * fsm = NULL;

        if (direction == APPLY_UP_DIRECTION)
          {
            if (t->get_type() == hfst::HFST_OL_TYPE ||
                t->get_type() == hfst::HFST_OLW_TYPE)
              {
                ERROR(<< "Operation not supported for optimized lookup format. Consider 'remove-optimization' to convert into ordinary format.");
                prompt();
                return *this;
              }

            // lookdown not yet implemented in HFST
            if (verbose_)
              {
                ERROR(<< "warning: apply up not implemented, inverting transducer and performing apply down");
                ERROR(<< "for faster performance, invert and minimize top network and do apply down instead");
              }
            t = new HfstTransducer(*(stack_.top()));
            t->invert().minimize(); // the user has been warned for possible slow performance
          }

        if (t->get_type() != hfst::HFST_OL_TYPE && t->get_type() != hfst::HFST_OLW_TYPE)
          {
            fsm = new HfstIterableTransducer(*t);
          }
        else
          {
            StringVector foo; // this gets ignored by ol transducer's is_lookup_infinitely_ambiguous
            if (t->is_lookup_infinitely_ambiguous(foo))
              {
                ol_cutoff = string_to_size_t(variables_["lookup-cycle-cutoff"]);
                if (verbose_)
                  {
                    ERROR(<< "warning: transducer is infinitely ambiguous, limiting number of cycles to " << ol_cutoff);
                  }
              }
          }

        char * line = NULL;
        // prompt is printed only when reading from the user
        const char * promptstr
          = ((infile == stdin) && verbose_)? get_apply_prompt(direction) : "";

        int ind = current_history_index();  // readline history to return to

        // get lines from infile..
        while (true)
          {
            line = xfst_getline(infile, promptstr);
            // .. until end of file...
            if (line == NULL)
              {
                // the next command must start on a fresh line
                if (infile == stdin) {
                  OUTPUT(<< "");
                  //hfst_fprintf(outstream_, "\n");
                }
                break;
              }
            // .. or until special end string
            if (strcmp(remove_newline(line), APPLY_END_STRING) == 0)
              {
                break;
              }

            // perform lookup/lookdown
            if (fsm != NULL)
              lookup(line, fsm);
            else
              lookup(line, t, ol_cutoff);
            free(line);
          }

        // ignore all readline history given to the apply command
        ignore_history_after_index(ind);

        if (direction == APPLY_UP_DIRECTION)
          delete t;
        if (fsm != NULL)
          delete fsm;

        PROMPT_AND_RETURN_THIS;
      }


    XfstCompiler&
    XfstCompiler::apply_up(FILE* infile)
      {
        return this->apply(infile, APPLY_UP_DIRECTION);
      }

    XfstCompiler&
    XfstCompiler::apply_up(const char* indata)
      {
        char* s = strdup(indata);
        char* line = strtok(s, "\n");
        while (line != NULL && (strcmp(line, APPLY_END_STRING) != 0))
          {
            apply_up_line(line);
            line = strtok(NULL, "\n");
          }
        free(s);
        PROMPT_AND_RETURN_THIS;
      }

    XfstCompiler&
    XfstCompiler::apply_down(FILE* infile)
      {
        return this->apply(infile, APPLY_DOWN_DIRECTION);
      }

    XfstCompiler&
    XfstCompiler::apply_down(const char* indata)
      {
        char* s = strdup(indata);
        char* line = strtok(s, "\n");
        
        while (line != NULL && (strcmp(line, APPLY_END_STRING) != 0))
          {
            apply_down_line(line);
            line = strtok(NULL, "\n");
          }
        free(s);
        PROMPT_AND_RETURN_THIS;
      }
    XfstCompiler&
    XfstCompiler::apply_med(FILE* infile)
      {
        char* line = 0;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, infile)) != -1)
          {
            apply_med_line(line);
          }
        return *this;
      }

    XfstCompiler&
    XfstCompiler::apply_med(const char* indata)
      {
        char* s = strdup(indata);
        char* line = strtok(s, "\n");
        while (line != NULL)
          {
            apply_med_line(line);
            line = strtok(NULL, "\n");
          }
        free(s);
        PROMPT_AND_RETURN_THIS;
      }

    XfstCompiler&
    XfstCompiler::define_alias(const char* name, const char* commands)
      {
        aliases_[name] = commands;
        PROMPT_AND_RETURN_THIS;
      }

    XfstCompiler&
    XfstCompiler::define_list(const char* name, const char* start,
                              const char* end)
      {
        if ((strlen(start) > 1) || (strlen(end) > 1))
          {
            ERROR(<< "unsupported unicode range " << std::string(start) << "-" << std::string(end));
        //hfst_fprintf(errorstream_, "unsupported unicode range %s-%s\n", start, end);
          }
        std::set<string> l;
        for (char c = *start; c < *end; c++)
          {
            char *s = static_cast<char*>(malloc(sizeof(char)*2));
            *s = c;
            *(s+1) = '\0';
            l.insert(s);
          }
        lists_[name] = l;
        return *this;
      }

    XfstCompiler&
    XfstCompiler::define_list(const char* name, const char* values)
      {
        if (definitions_.find(name) != definitions_.end())
          {
            ERROR(<< "Error: '" << std::string(name) << "' has already been defined as a transducer variable." << std::endl
		  << "It cannot have an incompatible definition as a list." << std::endl
		  << "Please undefine the definition first.");
            //fprintf(warnstream_, "Error: '%s' has already been defined as a transducer variable.\n"
            //        "It cannot have an incompatible definition as a list.\n"
            //        "Please undefine the definition first.\n", name);
            MAYBE_QUIT;
            PROMPT_AND_RETURN_THIS;
          }
        std::set<string> l;
        char* p = strdup(values);
        char* token = strtok(p, " ");
        while (token != NULL)
          {
            l.insert(token);
            token = strtok(NULL, " ");
          }
        free(p);
        lists_[name] = l;
        xre_.define_list(name, l); // XRE
        PROMPT_AND_RETURN_THIS;
      }

  XfstCompiler&
  XfstCompiler::define(const char * name)
  {
    GET_TOP(top);
    bool was_defined = xre_.is_definition(name);
    //if (!was_defined) {
    xre_.define(name, *top); // XRE
      //}
    HfstTransducer * nu = new HfstTransducer(*top);
    stack_.pop(); // todo: delete?
    if (variables_["name-nets"] == "ON") {
      nu->set_name(name);
    }
    definitions_[name] = nu;

    if (verbose_)
      {
        OUTPUT(<< (was_defined ? "Redefined" : "Defined") << " '" << std::string(name) << "'"); // hfst_fprintf(outstream_, " '%s'\n", name);
      }
    original_definitions_[name] = "<net taken from stack>";
    PROMPT_AND_RETURN_THIS;
  }

  XfstCompiler&
  XfstCompiler::define(const char* name, const char* xre)
    {
      /*if (hfst::xfst::nametoken_to_number(name) >= 0)
        {
          hfst_fprintf(errorstream_, "Could not define variable, '%s' is not a valid name\n",
                  name);
                  }*/

      // When calling this function, the regex \a indata should already have
      // been compiled into a transducer which should have been stored to
      // the variable latest_regex_compiled.

        if (lists_.find(name) != lists_.end())
          {
            ERROR(<< "Error: '" << std::string(name) << "' has already been defined as a list variable." << std::endl
		  << "It cannot have an incompatible definition as a transducer." << std::endl
		  << "Please undefine the variable first.");
            //fprintf(warnstream_, "Error: '%s' has already been defined as a list variable.\n"
            //        "It cannot have an incompatible definition as a transducer.\n"
            //        "Please undefine the variable first.\n", name);
            MAYBE_QUIT;
            PROMPT_AND_RETURN_THIS;
          }

      /*else*/ if (latest_regex_compiled != NULL)
        {
          bool was_defined = xre_.is_definition(name);
          //if (!was_defined)
          if (! xre_.define(name, xre))  // todo: equal to latest_regex_compiled ?  XRE
            {
              ERROR(<< "Could not define variable '" << std::string(name) << "'");
                //        << xre_.get_error_message() << std::endl;
              //hfst_fprintf(errorstream_, "Could not define variable %s:\n%s\n",
              //             name, xre_.get_error_message().c_str());
              xfst_fail();
            }
          HfstTransducer * nu = new HfstTransducer(*latest_regex_compiled);
          if (variables_["name-nets"] == "ON")
            nu->set_name(name);
          definitions_[name] = nu;

          if (verbose_)
            {
              OUTPUT(<< (was_defined ? "Redefined" : "Defined") << " '" << std::string(name) << "'"); // hfst_fprintf(outstream_, " '%s'\n", name);
            }
          original_definitions_[name] = xre;
        }
      else
        {
          ERROR(<< "Could not define variable '" << std::string(name) << "'");
          //        << xre_.get_error_message() << std::endl;
          //hfst_fprintf(errorstream_, "Could not define variable %s:\n%s\n",
          //        name, xre_.get_error_message().c_str());
          xfst_fail();
        }
      PROMPT_AND_RETURN_THIS;
    }

  
  // Store function name in \a prototype to \a name.
  // Return whether extraction succeeded.
  // \a prototype must be of format "functionname(arg1, arg2, ... argN)"
  static bool extract_function_name
  (const char* prototype, std::string& name)
  {
    for (unsigned int i=0; prototype[i] != 0; i++)
      {
        name = name + prototype[i];
        if (prototype[i] == '(') {
          return true;
        }
      }
    return false; // no starting parenthesis found
  }

  // Store names of function arguments in \a prototype to \a args.
  // Return whether extraction succeeded.
  // \a prototype must be of format "functionname(arg1, arg2, ... argN)"
  static bool extract_function_arguments
  (const char * prototype, std::vector<std::string>& args)
  {
    // skip the function name
    unsigned int i=0;
    while(prototype[i] != '(')
      {
        if (prototype[i] == '\0') {
          return false; // function name ended too early
        }
        ++i;
      }
    ++i; // skip the "(" in function name

    // start scanning the argument list "arg1, arg2, ... argN )"
    std::string arg = "";
    for ( ; prototype[i] != ')'; i++)
      {
        if (prototype[i] == '\0') { // no closing parenthesis found
          return false;
        }
        else if (prototype[i] == ' ') { // skip whitespace
        }
        else if (prototype[i] == ',') { // end of argument
          args.push_back(arg);
          arg = "";
        }
        else {
          arg += prototype[i];
        }
      }
    // last argument
    args.push_back(arg);

    return true;
  }

  /* Convert each argument name in \a arguments in \a xre with a special symbol
     "@function_name(argno@" where argno is a number defining which argument
     of the function is being replaced.

     For example, if we have a function Foo(x, y, z) defined with regex [ x y+ z x* ],
     the regex is converted into ["@Foo(1@" "@Foo(2@"+ "@Foo(3@" "@Foo(1@"* ].
  */
  std::string convert_argument_symbols
  (const std::vector<std::string> & arguments,
   const std::string & xre,
   const std::string & function_name,
   hfst::xre::XreCompiler & xre_,
   bool user_friendly_argument_names=false)
  {
    std::string retval(xre);
    unsigned int arg_number = 1;

    for (std::vector<std::string>::const_iterator arg
           = arguments.begin(); arg != arguments.end(); arg++)
      {
        std::set<unsigned int> arg_positions;
        if (! xre_.get_positions_of_symbol_in_xre
            (*arg, retval, arg_positions) )  // XRE
          {
            return std::string("");
          }

        std::string new_retval = std::string("");
        std::string substituting_argument;
        std::ostringstream os;

        if (user_friendly_argument_names)
          {
            os << arg_number;
            substituting_argument = "ARGUMENT" + os.str();
            /*(static_cast<ostringstream*>
              ( &(ostringstream() << arg_number) )->str());*/
          }
        else
          {
            os << arg_number;
            substituting_argument = "\"@" + function_name + os.str() + "@\"";
              /*              (static_cast<ostringstream*>
                              ( &(ostringstream() << arg_number) )->str()) + "@\"";*/
          }
     
        // go through retval
        for (unsigned int i=0; i < retval.length(); i++)
          {
            // argument to be replaced begins at this position
            if (arg_positions.find(i) != arg_positions.end())
              {
                arg_positions.erase(i); // case will not be handled again

                new_retval.append(substituting_argument);
                // skip rest of the original symbol by advancing i to
                // point to the last char in the original symbol
                for (unsigned int offset=1;
                     offset < arg->length(); offset++)
                  {
                    ++i;
                  }
              }
            // else, just copy
            else
              {
                new_retval += retval[i];
              }
          }

        retval = new_retval;
        ++arg_number;
      }

    return retval;
  }

  XfstCompiler&
  XfstCompiler::define_function(const char* prototype,
                                const char* xre)
    {
      std::string name = "";
      std::vector<std::string> arguments;

      if (! extract_function_name(prototype, name))
        {
          ERROR(<< "Error extracting function name from prototype '" << std::string(prototype) << "'");
          //hfst_fprintf(errorstream_, "Error extracting function name "
          //        "from prototype '%s'\n", prototype);
          xfst_fail();
          PROMPT_AND_RETURN_THIS;
        }

      if (! extract_function_arguments(prototype, arguments))
        {
          ERROR(<< "Error extracting function arguments from prototype '" << std::string(prototype) << "'");
          //hfst_fprintf(errorstream_, "Error extracting function arguments "
          //        "from prototype '%s'\n", prototype);
          xfst_fail();
          PROMPT_AND_RETURN_THIS;
        }

      std::string xre_converted
        = convert_argument_symbols(arguments, xre, name, xre_);
      if (xre_converted == std::string(""))
        {
          ERROR(<< "Error parsing function definition '" << std::string(xre) << "'");
          //hfst_fprintf(errorstream_, "Error parsing function definition '%s'\n", xre);
          xfst_fail();
          PROMPT_AND_RETURN_THIS;
        }

      bool was_defined = xre_.is_function_definition(name);

      if (! xre_.define_function(name, hfst::size_t_to_uint(arguments.size()), xre_converted)) // XRE
        {
          ERROR(<< "Error when defining function");
          //hfst_fprintf(errorstream_, "Error when defining function\n");
          xfst_fail();
          PROMPT_AND_RETURN_THIS;
        }

      if (verbose_)
        {
          OUTPUT(<< (was_defined ? "Redefined" : "Defined") << " function '" << name << "@" << (int)arguments.size() << ")");
        }

      function_arguments_[name] = hfst::size_t_to_uint(arguments.size());
      function_definitions_[std::string(name)]
        = convert_argument_symbols(arguments, xre, "", xre_, true);
      original_function_definitions_[prototype] = xre;

      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::undefine(const char* name_list)
    {
      char* s = strdup(name_list);
      char* name = strtok(s, " ");
      while (name != NULL)
        {
          if (definitions_.find(name) != definitions_.end())
            {
              definitions_.erase(definitions_.find(name));
              xre_.undefine(name);  // XRE
            }
          name = strtok(NULL, " ");
        }
      free(s);
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::unlist(const char* name)
    {
      if (lists_.find(name) != lists_.end()) {
        lists_.erase(lists_.find(name));
      }
      PROMPT_AND_RETURN_THIS;
    }

  /*XfstCompiler&
  XfstCompiler::name(const char* name)
    {
      names_[name] = stack_.top();
      PROMPT_AND_RETURN_THIS;
      }*/

  XfstCompiler&
  XfstCompiler::load_definitions(const char * infilename)
    {
      CHECK_FILENAME(infilename);
      return this->load_stack_or_definitions
        (infilename, true /* definitions*/ );
    }

#ifdef FOO
  // Convert \a str to upper case.
  static std::string to_upper_case(const std::string & str)
  {
    std::string retval;
    for (unsigned int i=0; i<str.length(); i++)
      {
        if (str[i] >= 97 && str[i] <= 122)
          {
            retval.append(1, str[i] - 32);
          }
        else
          {
            retval.append(1, str[i]);
          }
      }
    return retval;
  }

  // Whether \a c is allowed before or after a word when
  // searching for the word in text.
  static bool allow_char(char c)
  {
    std::string allowed_chars = " \n\t.,;:?!-/'\"<>()|";
    for (size_t i=0; i < allowed_chars.size(); i++)
      {
        if (allowed_chars[i] == c)
          {
            return true;
          }
      }
    return false;
  }

  // Whether word \a str_ is found in text \a text_.
  // Punctuation characters and upper/lower case are handled in this function.
  static bool string_found(const std::string & str_, const std::string & text_)
  {
    std::string str = to_upper_case(str_);
    std::string text = to_upper_case(text_);
    std::size_t pos = text.find(str);
    if (pos == std::string::npos)
      {
        return false;
      }
    if (pos == 0 || allow_char(text[pos-1]))
      {
        if (pos+str.length() == text.length() ||
            allow_char(text[pos+str.length()]))
          {
            return true;
          }
      }
    return false;
  }
#endif // FOO

  XfstCompiler&
  XfstCompiler::apropos(const char* text )
    {
      std::string message;
      if (!get_help_message(text, message, HELP_MODE_APROPOS))
        {
          OUTPUT(<< "nothing found for '" << std::string(text) << "'");
          //hfst_fprintf(outstream_, "nothing found for '%s'\n", text);
        }
      else
        {
          OUTPUT(<< message);
          //hfst_fprintf(outstream_, "%s", message.c_str());
        }
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::describe(const char* text)
    {
      int help_mode = (strcmp(text, "") == 0)?
        HELP_MODE_ALL_COMMANDS : HELP_MODE_ONE_COMMAND;
      std::string message;
      if (!get_help_message(text, message, help_mode))
        {
          OUTPUT(<< "no help found for '" << std::string(text) << "'");
          //hfst_fprintf(outstream_, "no help found for '%s'\n", text);
        }
      else
        {
          OUTPUT(<< message);
          //hfst_fprintf(outstream_, "%s", message.c_str());
        }
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::clear()
    {
      while(!stack_.empty())
        {
          delete(stack_.top());
          stack_.pop();
        }
      if (latest_regex_compiled != NULL)
        {
          delete latest_regex_compiled;
          latest_regex_compiled = NULL;
        }
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::pop()
    {
      if (stack_.empty()) {
        OUTPUT(<< "Stack is empty."); //hfst_fprintf(outstream_, "Stack is empty.\n");
      }
      else // todo: delete if not definition?
        stack_.pop();
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::push(const char* name)
    {
      if (definitions_.find(name) == definitions_.end())
        {
          OUTPUT(<< "no such defined network: '" << std::string(name) << "'");
          //hfst_fprintf(outstream_, "no such defined network: '%s'\n", name);
          PROMPT_AND_RETURN_THIS;
        }

      stack_.push(new HfstTransducer(*(definitions_[name])));
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::push()
    {
      for (map<string,HfstTransducer*>::const_iterator def
             = definitions_.begin(); def != definitions_.end();
           ++def)
        {
          stack_.push(new HfstTransducer(*(def->second)));
        }

      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::turn()
    {
      queue<HfstTransducer*> tmp;
      while (!stack_.empty())
        {
          tmp.push(stack_.top());
          stack_.pop();
        }
      while (!tmp.empty())
        {
          stack_.push(tmp.front());
          tmp.pop();
        }
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::rotate()
    {
      if (stack_.empty())
        {
          PROMPT_AND_RETURN_THIS;
        }
        
      stack<HfstTransducer*> tmp;
      while (!stack_.empty())
        {
          tmp.push(stack_.top());
          stack_.pop();
        }
      stack_ = tmp;

      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }

  static const char * to_filename(const char * file)
  {
    if (file == 0) {
      return "<stdin>";
    }
    else {
      return file;
    }
  }

  XfstCompiler&
  XfstCompiler::add_loaded_definition(HfstTransducer * t)
  {
    std::string def_name = t->get_name();
    if (def_name == "")
      {
        ERROR(<< "warning: loaded transducer definition has no name, skipping it");
        //hfst_fprintf(warnstream_, "warning: loaded transducer definition "
        //        "has no name, skipping it\n");
        // xfst_lesser_fail(); ???
        return *this;
      }
    std::map<std::string, HfstTransducer*>::const_iterator it
      = definitions_.find(def_name);
    if (it != definitions_.end())
      {
        ERROR(<< "warning: a definition named '" << def_name << "' already exists, overwriting it");
        //hfst_fprintf(warnstream_, "warning: a definition named '%s' "
        //        "already exists, overwriting it\n", def_name.c_str());
        // xfst_lesser_fail(); ???
        definitions_.erase(def_name);
      }
    definitions_[def_name] = t;
    return *this;
  }

  void
  XfstCompiler::convert_to_common_format
  (HfstTransducer * t, const char * filename /*=NULL*/)
  {
    if (! this->check_filename(filename)) { return; }

    if (t->get_type() != format_)
      {
        if (t->get_type() == hfst::HFST_OL_TYPE ||
            t->get_type() == hfst::HFST_OLW_TYPE)
          {
            if (verbose_)
              {
                ERROR(<< "warning: transducer is in optimized lookup format, 'apply up' is the only operation it supports");
                //hfst_fprintf(warnstream_, "warning: transducer is in optimized lookup format, 'apply up' is the only operation it supports\n");
              }
            return;
          }

        if (verbose_)
          {
	    std::ostringstream oss("");
            oss << "warning: converting transducer type from " << hfst::implementation_type_to_format(t->get_type()) << " to " << hfst::implementation_type_to_format(format_);
            //hfst_fprintf(warnstream_, "warning: converting transducer type from %s to %s",
            //        hfst::implementation_type_to_format(t->get_type()),
            //        hfst::implementation_type_to_format(format_));
            if (filename != NULL)
              {
                oss << " when reading from file '" << to_filename(filename) << "'";
                //hfst_fprintf(warnstream_, " when reading from file '%s'",
                //        to_filename(filename));
              }
            if (! hfst::HfstTransducer::is_safe_conversion(t->get_type(), format_))
              {
                oss << " (loss of information is possible)";
                //hfst_fprintf(warnstream_, " (loss of information is possible)");
              }
	    ERROR(<< oss.str());
            //hfst_fprintf(warnstream_, "\n");
          }
        t->convert(format_);
      }
  }

  HfstInputStream *
  XfstCompiler::open_hfst_input_stream(const char * infilename)
  {
    assert(infilename != NULL);
    if (! this->check_filename(infilename)) { return NULL; }
    
    FILE * infile = hfst::hfst_fopen(infilename, "r");
    if (infile == NULL)
      {
        ERROR(<< "Could not open file " << infilename);
        xfst_fail();
        return NULL;
      }
    if (fclose(infile) != 0)
      {
        ERROR(<< "Could not close file " << infilename);
        xfst_fail();
        return NULL;
      }

    HfstInputStream* instream = 0;
    try
      {
        instream = (infilename != 0) ?
          new HfstInputStream(infilename):
          new HfstInputStream();
      }
    catch (NotTransducerStreamException ntse)
      {
        ERROR(<< "Unable to read transducers from " << to_filename(infilename));
        //hfst_fprintf(errorstream_, "Unable to read transducers from %s\n",
        //        to_filename(infilename));
        xfst_fail();
        return NULL;
      }
    return instream;
  }

  XfstCompiler&
  XfstCompiler::load_stack_or_definitions
  (const char* infilename, bool load_definitions)
  {
    CHECK_FILENAME(infilename);
    // Try to open the stream to file infilename
    HfstInputStream * instream = open_hfst_input_stream(infilename);
    IF_NULL_PROMPT_AND_RETURN_THIS(instream);

    // Read transducers from stream
    while (instream->is_good())
      {
        HfstTransducer* t = new HfstTransducer(*instream);
        
        // Convert transducer format, if needed
        convert_to_common_format(t, infilename);
        
        // Add transducer as definition..
        if (load_definitions)
          {
            if (t->get_type() == hfst::HFST_OL_TYPE ||
                t->get_type() == hfst::HFST_OLW_TYPE)
              {
                ERROR(<< "cannot load optimized lookup transducers as definitions");
                //hfst_fprintf(errorstream_, "cannot load optimized lookup transducers as definitions\n");
                break;
              }
            add_loaded_definition(t);
          }
        // ..or push it to stack.
        else
          {
            stack_.push(t);
            print_transducer_info();
          }
      }
    
    instream->close();
    delete instream;
    PROMPT_AND_RETURN_THIS;
  }

  XfstCompiler&
  XfstCompiler::load_stack(const char* infilename)
    {
      CHECK_FILENAME(infilename);
      return this->load_stack_or_definitions(infilename, false);
    }

  XfstCompiler&
  XfstCompiler::collect_epsilon_loops()
    {
      ERROR(<< "cannot collect epsilon loops");
      //hfst_fprintf(stderr, "cannot collect epsilon loops %s:%d\n", __FILE__,
      //        __LINE__);
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::compact_sigma()
    {
      GET_TOP(top);
      top->prune_alphabet();
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::eliminate_flag(const char* name)
    {
      GET_TOP(tmp);
      std::string name_(name);
      try
        {
          tmp->eliminate_flag(name);
        }
      catch (HfstException & e)
        {
          ERROR(<< "error: could not eliminate flag '" << name << "': " << e.name);
          MAYBE_QUIT;
        }
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::eliminate_flags()
    {
      GET_TOP(tmp);
      tmp->eliminate_flags();
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::twosided_flags()
  {
      GET_TOP(tmp);
      tmp->twosided_flag_diacritics();
      PROMPT_AND_RETURN_THIS;
  }

  XfstCompiler&
  XfstCompiler::echo(const char* text)
    {
      OUTPUT(<< text);
      //hfst_fprintf(outstream_, "%s\n", text);
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::quit(const char* message)
    {
      if (verbose_ && (strcmp(message, "dodongo") == 0))
        {
          OUTPUT(<< "dislikes smoke.");
          //hfst_fprintf(outstream_, "dislikes smoke.\n");
        }
      else if (verbose_)
        {
          OUTPUT(<< message << ".");
          //hfst_fprintf(outstream_, "%s.\n", message);
        }
      else
        ;
      this->quit_requested_ = true;
      return *this;
    }

  XfstCompiler&
  XfstCompiler::system(const char* command)
    {
      if (restricted_mode_)
	{
          ERROR(<< "Restricted mode (--restricted-mode) is in use, system calls are disabled");
          xfst_lesser_fail();
          PROMPT_AND_RETURN_THIS;
	}
      int rv = ::system(command);
      if (rv != 0)
        {
          ERROR(<< "system " << command << " returned " << rv);
          //hfst_fprintf(stderr, "system %s returned %d\n", command, rv);
        }
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::set(const char* name, const char* text)
    {
      if (variables_.find(name) == variables_.end())
        {
          if (strcmp(name, "compose-flag-as-special") == 0)
            {
              ERROR(<< "variable compose-flag-as-special not found, using flag-is-epsilon instead");
              //hfst_fprintf(warnstream_, "variable compose-flag-as-special not found, using flag-is-epsilon instead\n");
              variables_["flag-is-epsilon"] = text;
              if (verbose_)
                {
                  OUTPUT(<< "variable flag-is-epsilon = " << text);
                  //hfst_fprintf(outstream_, "variable %s = %s\n", "flag-is-epsilon", text);
                }
              PROMPT_AND_RETURN_THIS;
            }
          else
            {
              ERROR(<< "no such variable: '" << name << "'");
              //hfst_fprintf(warnstream_, "no such variable: '%s'\n", name);
              // xfst_lesser_fail(); ???
              PROMPT_AND_RETURN_THIS;
            }
        }
      variables_[name] = text;
      if (strcmp(name, "hopcroft-min") == 0)
        {
          if (strcmp(text, "ON") == 0)
            hfst::set_minimization_algorithm(hfst::HOPCROFT);
          if (strcmp(text, "OFF") == 0)
            hfst::set_minimization_algorithm(hfst::BRZOZOWSKI);
        }
      if (strcmp(name, "encode-weights") == 0)
        {
          if (strcmp(text, "ON") == 0)
            hfst::set_encode_weights(true);
          if (strcmp(text, "OFF") == 0)
            hfst::set_encode_weights(false);
        }
      if (strcmp(name, "harmonize-flags") == 0)
        {
          if (strcmp(text, "ON") == 0)
            xre_.set_flag_harmonization(true);
          if (strcmp(text, "OFF") == 0)
            xre_.set_flag_harmonization(false);
        }
      if (strcmp(name, "xerox-composition") == 0)
        {
          if (strcmp(text, "ON") == 0)
            hfst::set_xerox_composition(true);
          if (strcmp(text, "OFF") == 0)
            hfst::set_xerox_composition(false);
        }
      if (strcmp(name, "flag-is-epsilon") == 0)
        {
          if (strcmp(text, "ON") == 0)
            hfst::set_flag_is_epsilon_in_composition(true);
          if (strcmp(text, "OFF") == 0)
            hfst::set_flag_is_epsilon_in_composition(false);
        }
      if (strcmp(name, "minimal") == 0)
        {
          if (strcmp(text, "ON") == 0)
            hfst::set_minimization(true);
          if (strcmp(text, "OFF") == 0)
            hfst::set_minimization(false);
        }

      if (verbose_)
        {
          OUTPUT(<< "variable " << name << " = " << text);
          //hfst_fprintf(outstream_, "variable %s = %s\n", name, text);
        }

      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::set(const char* name, unsigned int number)
    {
      if (variables_.find(name) == variables_.end())
        {
          ERROR(<< "no such variable: '" << name << "'");
          //hfst_fprintf(warnstream_, "no such variable: '%s'\n", name);
          // xfst_lesser_fail(); ???
          PROMPT_AND_RETURN_THIS;
        }
      char* num = static_cast<char*>(malloc(sizeof(char)*31));
      sprintf(num, "%u", number);
      variables_[name] = std::string(num);
      free(num);
      PROMPT_AND_RETURN_THIS;
    }

  std::string
  XfstCompiler::get(const char* name)
  {
    if (variables_.find(name) == variables_.end())
      return std::string("");
    return std::string(variables_[name]);
  }

  XfstCompiler&
  XfstCompiler::show(const char* name)
    {
      if (variables_.find(name) == variables_.end())
        {
          ERROR(<< "no such variable: '" << name << "'");
          //hfst_fprintf(warnstream_, "no such variable: '%s'\n", name);
          // xfst_lesser_fail(); ???
          PROMPT_AND_RETURN_THIS;
        }
      OUTPUT(<< "variable " << name << " = " << variables_[name]);
      //hfst_fprintf(outstream_, "variable %s = %s\n", name, variables_[name].c_str());
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::show()
    {
      std::ostringstream oss("");
      for (map<string,string>::const_iterator var = variables_.begin();
           var != variables_.end();
           var++)
        {
	  if (var->first == "copyright-owner")
            {
              oss.width(20);
              oss << var->first << ": " << var->second << std::endl;
              //hfst_fprintf(stderr, "%20s:        %s\n", var->first.c_str(),
              //          var->second.c_str());
            }
          else
            {
              oss.width(20);
              oss << var->first << ": ";
              oss.width(6);
              oss << var->second << ": " << variable_explanations_[var->first] << std::endl;
              //  hfst_fprintf(stderr, "%20s:%6s: %s\n",
              //          var->first.c_str(), var->second.c_str(),
              //          variable_explanations_[var->first].c_str());
            }
        }
      OUTPUT(<< oss.str());
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::test_eq(bool assertion)
    {
      if (stack_.size() < 2)
        {
          ERROR(<< "Not enough networks on stack.");
	  ERROR(<< "Operation requires at least 2.");
          //hfst_fprintf(stderr, "Not enough networks on stack. "
          //        "Operation requires at least 2.\n");
          xfst_lesser_fail();
          return *this;
        }
      HfstTransducer* first = stack_.top();
      stack_.pop();
      HfstTransducer* second = stack_.top();
      stack_.pop();
      bool result = first->compare(*second, false);
      print_bool(result);
      stack_.push(second);
      stack_.push(first);
      MAYBE_ASSERT(assertion, result);
      return *this;
    }

  XfstCompiler&
  XfstCompiler::print_bool(bool value)
  {
    int printval = (value)? 1 : 0;
    OUTPUT(<< printval << ", (1 = TRUE, 0 = FALSE)");
    //hfst_fprintf(outstream_, "%i, (1 = TRUE, 0 = FALSE)\n", printval);
    return *this;
  }
  HfstTransducer *
  XfstCompiler::top(bool silent/*=false*/)
  {
    if (stack_.size() < 1)
      {
	if (!silent)
	  {
	    EMPTY_STACK;
	  }
        xfst_lesser_fail();
	if (!silent)
	  {
	    prompt();
	  }
        return NULL;
      }
    HfstTransducer * retval = stack_.top();
    if (retval->get_type() == hfst::HFST_OL_TYPE ||
        retval->get_type() == hfst::HFST_OLW_TYPE)
      {
	if (!silent)
	  {
	    ERROR(<< "Operation not supported for optimized lookup format. Consider 'remove-optimization' to convert into ordinary format.");
	    prompt();
	  }
        return NULL;
      }
    return retval;
  }

  XfstCompiler&
  XfstCompiler::test_funct(bool assertion)
    {
      ERROR(<< "test funct missing");
      //hfst_fprintf(stderr, "test funct missing %s:%d\n", __FILE__, __LINE__);
      //MAYBE_ASSERT(assertion, result);
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::test_id(bool assertion)
    {
      HfstTransducer * tmp = this->top();
      if (NULL == tmp) {
        return *this;
      }

      HfstTransducer tmp_input(*tmp);
      tmp_input.input_project();
      HfstTransducer tmp_output(*tmp);
      tmp_output.output_project();

      bool result = tmp_input.compare(tmp_output, false);
      this->print_bool(result);
      MAYBE_ASSERT(assertion, result);
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::test_upper_bounded(bool assertion)
    {
      HfstTransducer * temp = this->top();
      if (NULL == temp) {
        return *this;
      }
      
      HfstTransducer tmp(*temp);
      tmp.output_project();
      tmp.remove_epsilons(); // needed for testing cyclicity
      
      bool result = ! tmp.is_cyclic();
      this->print_bool(result);
      MAYBE_ASSERT(assertion, result);
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::test_uni(Level level, bool assertion)
    {
      HfstTransducer * temp = this->top();
      if (NULL == temp) {
        return *this;
      }

      HfstTransducer tmp(*temp);
      tmp.input_project();
      HfstTransducer id(hfst::internal_identity, tmp.get_type());
      bool value = false;

      if (level == UPPER_LEVEL)
        value = id.compare(tmp, false);
      else if (level == LOWER_LEVEL)
        value = ! id.compare(tmp, false);
      else {
        ERROR(<< "ERROR: argument given to function 'test_uni' not recognized");
      }
      //hfst_fprintf(errorstream_, "ERROR: argument given to function 'test_uni'\n"
      //          "not recognized\n");

      this->print_bool(value);
      MAYBE_ASSERT(assertion, value);
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::test_upper_uni(bool assertion)
    {
      return this->test_uni(UPPER_LEVEL, assertion);
    }
  XfstCompiler&
  XfstCompiler::test_lower_bounded(bool assertion)
    {
      HfstTransducer * temp = this->top();
      if (NULL == temp) {
        return *this;
      }
      
      HfstTransducer tmp(*temp);
      tmp.input_project();
      tmp.remove_epsilons(); // needed for testing cyclicity
      
      bool result = ! tmp.is_cyclic();
      this->print_bool(result);
      MAYBE_ASSERT(assertion, result);
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::test_lower_uni(bool assertion)
    {
      return this->test_uni(LOWER_LEVEL, assertion);
    }
  XfstCompiler&
  XfstCompiler::test_nonnull(bool assertion)
    {
      return this->test_null(true, assertion);
    }
  XfstCompiler&
  XfstCompiler::test_null(bool invert_test_result, bool assertion)
    {
      HfstTransducer * tmp = this->top();
      if (NULL == tmp) {
        return *this;
      }

      HfstTransducer empty(tmp->get_type());
      bool value = empty.compare(*tmp, false);
      if (invert_test_result)
        value = !value;
      this->print_bool(value);

      MAYBE_ASSERT(assertion, value);
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::test_operation(TestOperation operation, bool assertion)
  {
    if (stack_.size() < 2)
      {
        ERROR(<< "Not enough networks on stack. Operation requires at least 2.");
        //hfst_fprintf(stderr, "Not enough networks on stack. "
        //        "Operation requires at least 2.\n");
        xfst_lesser_fail();
        PROMPT_AND_RETURN_THIS;
      }
      std::stack<HfstTransducer*> copied_stack(stack_);

      HfstTransducer topmost_transducer(*(copied_stack.top()));
      copied_stack.pop();

      HfstTransducer empty(topmost_transducer.get_type());

      while (!copied_stack.empty())
      {
        HfstTransducer next_transducer(*(copied_stack.top()));
        copied_stack.pop();

        switch(operation)
          {
          case TEST_OVERLAP_:
            topmost_transducer.intersect(next_transducer);
            if(topmost_transducer.compare(empty))
              {
                this->print_bool(false);
                MAYBE_ASSERT(assertion, false);
                PROMPT_AND_RETURN_THIS;
              }
            break;
          case TEST_SUBLANGUAGE_:
            {
              HfstTransducer intersection(topmost_transducer);
              intersection.intersect(next_transducer);
              if(! intersection.compare(topmost_transducer))
                {
                  this->print_bool(false);
                  MAYBE_ASSERT(assertion, false);
                  PROMPT_AND_RETURN_THIS;
                }
              topmost_transducer = next_transducer;
              break;
            }
          default:
            ERROR(<< "ERROR: unknown test operation");
            //hfst_fprintf(errorstream_, "ERROR: unknown test operation\n");
            break;
          }
      }
      this->print_bool(true);
      MAYBE_ASSERT(assertion, true);
      PROMPT_AND_RETURN_THIS;
  }

  XfstCompiler&
  XfstCompiler::test_overlap(bool assertion)
    {
      return this->test_operation(TEST_OVERLAP_, assertion);
    }
  XfstCompiler&
  XfstCompiler::test_sublanguage(bool assertion)
    {
      return this->test_operation(TEST_SUBLANGUAGE_, assertion);
    }
  XfstCompiler&
  XfstCompiler::test_unambiguous(bool assertion)
    {
      ERROR(<< "test unambiguous missing");
      //hfst_fprintf(stderr, "test unambiguous missing %s:%d\n", __FILE__, __LINE__);
      //MAYBE_ASSERT(assertion, result);
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::test_infinitely_ambiguous(bool assertion)
    {
      HfstTransducer * tmp = this->top();
      if (NULL == tmp) {
        return *this;
      }
      bool value = tmp->is_infinitely_ambiguous();
      this->print_bool(value);
      MAYBE_ASSERT(assertion, value);
      PROMPT_AND_RETURN_THIS;
    }

  // Tokenize string \a s using \a c as separator.
  static StringVector tokenize_string(const char * s, char c)
  {
    StringVector retval;
    std::string str(s);
    size_t pos = 0;
    for (size_t i=0; i < str.size(); i++)
      {
        if (str[i] == c)
          {
            retval.push_back(std::string(str, pos, i-pos));
            pos = i+1;
          }
      }
    retval.push_back(std::string(str, pos));
    return retval;
  }

  // Convert StringVector \a sv into StringPair.
  static StringPair symbol_vector_to_symbol_pair(const StringVector & sv)
  {
    StringPair sp;
    if (sv.size() == 2)
      {
        if (sv[0] == "?") {
          sp.first = hfst::internal_unknown; }
        else if (sv[0] == "0") {
          sp.first = hfst::internal_epsilon; }
        else {
          sp.first = sv[0]; }

        if (sv[1] == "?") {
          sp.second = hfst::internal_unknown; }
        else if (sv[1] == "0") {
          sp.second = hfst::internal_epsilon; }
        else {
          sp.second = sv[1]; }
      }
    else if (sv.size() == 1)
      {
        if (sv[0] == "?") { // special case "?"
          sp.first = hfst::internal_identity; }
        else if (sv[0] == "0") {
          sp.first = hfst::internal_epsilon; }
        else {
          sp.first = sv[0]; }
        sp.second = sp.first;
      }
    else
      {
        throw "error: symbol vector cannot be converted into symbol pair";
      }
    return sp;
  }

  XfstCompiler&
  XfstCompiler::substitute_named(const char* variable, const char* label)
    {
      GET_TOP(top);

      std::map<std::string, HfstTransducer*>::const_iterator it
        = definitions_.find(variable);
      if (it == definitions_.end())
        {
          ERROR(<< "no such definition '" << variable << "', cannot substitute");
          //hfst_fprintf(errorstream_, "no such definition '%s', cannot substitute\n",
          //        variable);
          MAYBE_QUIT;
          PROMPT_AND_RETURN_THIS;
        }
      
      std::string labelstr(label);
      if (labelstr == "?")
        labelstr = hfst::internal_identity;
      if (labelstr == "0")
        labelstr = hfst::internal_epsilon;

      StringSet alpha = top->get_alphabet();
      if (alpha.find(labelstr) == alpha.end())
        {
          ERROR(<< "no occurrences of label '" << label << "', cannot substitute");
          //hfst_fprintf(errorstream_, "no occurrences of label '%s', cannot substitute\n",
          //        label);
          MAYBE_QUIT;
          PROMPT_AND_RETURN_THIS;
        }

      HfstIterableTransducer fsm(*top);
      
      for (HfstIterableTransducer::const_iterator it = fsm.begin();
           it != fsm.end(); it++ )
        {
          for (hfst::implementations::HfstTransitions::const_iterator tr_it
                 = it->begin(); tr_it != it->end(); tr_it++)
            {
              std::string isymbol = tr_it->get_input_symbol();
              std::string osymbol = tr_it->get_output_symbol();
              if (isymbol != osymbol &&
                  (isymbol == labelstr || osymbol == labelstr))
                {
                  ERROR(<< "label '" << label << "' is used as a symbol on one side of an arc, cannot substitute");
                  //hfst_fprintf(errorstream_, "label '%s' is used as a symbol on one "
                  //        "side of an arc, cannot substitute\n", label);
                  MAYBE_QUIT;
                  PROMPT_AND_RETURN_THIS;
                }
            }
        }

      StringPair labelpair(labelstr, labelstr);
      alpha = it->second->get_alphabet();
      top->substitute(labelpair, *(it->second), false);

      if (!hfst::is_epsilon(labelstr) && !hfst::is_identity(labelstr) &&
          alpha.find(labelstr) == alpha.end())
        {
          top->remove_from_alphabet(labelstr);
        }
      
      MAYBE_MINIMIZE(top);
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::substitute_label(const char* list, const char* target)
    {
      GET_TOP(top);

      // tokenize list into labels
      StringPairSet symbol_pairs;

      if (strcmp("NOTHING", list) != 0)
        {
          StringVector labels = tokenize_string(list, ' ');
          for (StringVector::const_iterator it = labels.begin();
               it != labels.end(); it++)
            {
              // tokenize labels into string pairs
              StringVector sv = tokenize_string(it->c_str(), ':');
              try
                {
                  StringPair sp = symbol_vector_to_symbol_pair(sv);
                  symbol_pairs.insert(sp);
                }
              catch (const char * msg)
                {
                  (void)msg;
                  ERROR(<< "error: could not substitute with '" << list << "'");
                  //hfst_fprintf(errorstream_, "error: could not substitute with '%s'\n", list);
                  MAYBE_QUIT;
                  PROMPT_AND_RETURN_THIS;
                }
            }
        }

      // tokenize target label into string pair
      StringVector target_vector = tokenize_string(target, ':');
      try
        {
          StringPair target_label = symbol_vector_to_symbol_pair(target_vector);

          HfstIterableTransducer fsm(*top);
          bool target_label_found = false;

          for (HfstIterableTransducer::const_iterator it = fsm.begin();
               it != fsm.end() && !target_label_found; it++ )
            {
              for (hfst::implementations::HfstTransitions::const_iterator tr_it
                     = it->begin(); tr_it != it->end(); tr_it++)
                {
                  if (target_label.first == tr_it->get_input_symbol() &&
                      target_label.second == tr_it->get_output_symbol())
                    {
                      target_label_found = true;
                      break;
                    }
                }
            }
          if (!target_label_found)
            {
              ERROR(<< "no occurrences of '" << target_label.first << ":" << target_label.second << "', cannot substitute");
              //hfst_fprintf(errorstream_, "no occurrences of '%s:%s', cannot substitute\n",
              //        target_label.first.c_str(), target_label.second.c_str());
              PROMPT_AND_RETURN_THIS;
            }

          top->substitute(target_label, symbol_pairs);
        }
      catch (const char * msg)
        {
          (void)msg;
          ERROR(<< "error: could not substitute '" << target << "'");
          //hfst_fprintf(errorstream_, "error: could not substitute '%s'\n", target);
          MAYBE_QUIT;
        }

      MAYBE_MINIMIZE(top);
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::substitute_symbol(const char* list, const char* target)
    {
      GET_TOP(top);

      StringSet alpha = top->get_alphabet();
      if (alpha.find(target) == alpha.end())
        {
          ERROR(<< "no occurrences of symbol '" << target << "', cannot substitute");
          //hfst_fprintf(errorstream_, "no occurrences of symbol '%s', cannot substitute\n", target);
          MAYBE_QUIT;
          PROMPT_AND_RETURN_THIS;
        }

      stack_.pop();

      std::string liststr(list);
      if (liststr == "\"NOTHING\"") // list is given in quoted format: "foo" "bar" ...
        liststr = "";

      // use regex parser:  `[ [TR] , "s" , L ]
      xre_.define("TempXfstTransducerName", *top);  // XRE
      std::string subst_regex("`[ [TempXfstTransducerName] , ");
      subst_regex += "\"" + std::string(target) + "\" , " + liststr + " ]";
      HfstTransducer * substituted = xre_.compile(subst_regex);  // XRE
      xre_.undefine("TempXfstTransducerName");  // XRE

      if (substituted != NULL)
        {
          MAYBE_MINIMIZE(substituted);
          stack_.push(substituted);
          print_transducer_info();
        }
      else
        {
          ERROR(<< "fatal error in substitution");
          this->fail_flag_ = true;
        }
      PROMPT_AND_RETURN_THIS;
    }
  
  XfstCompiler&
  XfstCompiler::print_aliases(std::ostream * oss)
    {
      for (map<string,string>::const_iterator alias = aliases_.begin();
           alias != aliases_.end();
           ++alias)
        {
          oss->width(10);
          *oss << "alias " << alias->first << " " << alias->second;
        }
      PROMPT_AND_RETURN_THIS;
    }
          
  XfstCompiler&
  XfstCompiler::print_arc_count(const char* level, std::ostream * oss)
    {
      ERROR(<< "missing " << level << " arc count");
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::print_arc_count(std::ostream * oss)
    {
      ERROR(<< "missing arc count");
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::print_defined(std::ostream * oss)
    {
      bool definitions = false;
      for (map<string,string>::const_iterator def
             = original_definitions_.begin(); def != original_definitions_.end();
           ++def)
        {
          definitions = true;
          oss->width(10);
          *oss << def->first << " " << def->second << std::endl;
        }
      if (!definitions)
        *oss << "No defined symbols." << std::endl;

      definitions = false;
      for (map<string,string>::const_iterator func = original_function_definitions_.begin();
           func != original_function_definitions_.end(); func++)
        {
          definitions = true;
          oss->width(10);
          *oss << func->first << " " << func->second << std::endl;
        }
      if (!definitions)
        *oss << "No function definitions." << std::endl;

      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::print_dir(const char* globdata, std::ostream * oss)
    {
#ifndef _WIN32
      glob_t globbuf;

      int rv = glob(globdata, 0, NULL, &globbuf);
      if (rv == 0)
        {
          for (unsigned int i = 0; i < globbuf.gl_pathc; i++)
            {
              *oss << globbuf.gl_pathv[i] << std::endl;
            }
        }
      else
        {
          *oss << "glob(" << globdata << ") = " << rv << std::endl;
        }
#else
      ERROR(<< "print dir not implemented for windows");
#endif // WINDOWS
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::print_flags(std::ostream * oss)
    {
      ERROR(<< "missing print flags");
      PROMPT_AND_RETURN_THIS;
    }
  
  XfstCompiler&
  XfstCompiler::print_labels(std::ostream * oss)
  {
    GET_TOP(topmost);
    return this->print_labels(oss, topmost);
  }

  XfstCompiler&
  XfstCompiler::print_labels(const char* name, std::ostream * oss)
    {
      std::map<std::string, HfstTransducer*>::const_iterator it
        = definitions_.find(name);
      if (it == definitions_.end())
        {
          *oss << "no such definition '" << name << "'" << std::endl;
        }
      else
        {
          return this->print_labels(oss, it->second);
        }
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::print_labels(std::ostream * oss, HfstTransducer* tr)
    {
      std::set<std::pair<std::string, std::string> > label_set;
      HfstIterableTransducer fsm(*tr);
      
      for (HfstIterableTransducer::const_iterator it = fsm.begin();
           it != fsm.end(); it++ )
        {
          for (hfst::implementations::HfstTransitions::const_iterator tr_it
                 = it->begin(); tr_it != it->end(); tr_it++)
            {
              std::pair<std::string, std::string> label_pair
                (tr_it->get_input_symbol(), tr_it->get_output_symbol());
              label_set.insert(label_pair);
            }
        }
        
      *oss << "Labels: ";

      for(std::set<std::pair<std::string, std::string> >::const_iterator it
            = label_set.begin(); it != label_set.end(); it++)
        {
          if (it != label_set.begin())
            *oss << ", ";
          *oss << it->first;
          if (it->first != it->second)
            *oss << ":" << it->second;
        }
      *oss << std::endl;
      *oss << "Size: " << (int)label_set.size() << std::endl;

      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::print_labelmaps(std::ostream * oss)
    {
      ERROR(<< "missing label-maps");
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::print_label_count(std::ostream * oss)
    {
    GET_TOP(topmost);

    std::map<std::pair<std::string, std::string>, unsigned int > label_map;
    HfstIterableTransducer fsm(*topmost);
      
      for (HfstIterableTransducer::const_iterator it = fsm.begin();
           it != fsm.end(); it++ )
        {
          for (hfst::implementations::HfstTransitions::const_iterator tr_it
                 = it->begin(); tr_it != it->end(); tr_it++)
            {
              std::pair<std::string, std::string> label_pair
                (tr_it->get_input_symbol(), tr_it->get_output_symbol());
              (label_map[label_pair])++;
            }
        }

      unsigned int index=1;
      for(std::map<std::pair<std::string, std::string>, unsigned int >::const_iterator
            it= label_map.begin(); it != label_map.end(); it++)
        {
          if (it != label_map.begin())
            *oss << "   ";
          *oss << index << ". ";
          *oss << it->first.first;
          if (it->first.first != it->first.second)
            *oss << ":" << it->first.second;
          *oss << " " << it->second;
          index++;
        }
      *oss << std::endl;
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::print_list(const char* name, std::ostream * oss)
    {
      if (lists_.find(name) == lists_.end())
        {
          *oss << "No such list defined: " << name << std::endl;
          PROMPT_AND_RETURN_THIS;
        }
      std::set<string> l = lists_[name];
      oss->width(10);
      *oss << name << ": ";

      for (std::set<string>::const_iterator s = l.begin();
           s != l.end();
           ++s)
        {
          *oss << *s << " ";
        }
      *oss << std::endl;
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::print_list(std::ostream * oss)
    {
      if (lists_.size() == 0)
        {
          *oss << "No lists defined." << std::endl;
          PROMPT_AND_RETURN_THIS;
        }
      for (map<string,std::set<string> >::const_iterator l = lists_.begin();
           l != lists_.end();
           ++l)
        {
          oss->width(10);
          *oss << l->first << " ";

          for (std::set<string>::const_iterator s = l->second.begin();
               s != l->second.end();
               ++s)
            {
              *oss << *s << " ";
            }
          *oss << std::endl;
        }
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::shortest_string
  (const hfst::HfstTransducer * transducer,
   hfst::HfstTwoLevelPaths & paths)
  {
    transducer->extract_shortest_paths(paths);
    return *this;
  }

  XfstCompiler&
  XfstCompiler::print_shortest_string(std::ostream * oss)
    {
      GET_TOP(topmost);

      HfstTwoLevelPaths paths;
      this->shortest_string(topmost, paths);

      if (paths.size() == 0)
        {
          OUTPUT(<< "transducer is empty");
        }
      else
        {
          print_paths(paths, oss);
        }
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::print_shortest_string_size(std::ostream * oss)
    {
      GET_TOP(topmost);

      HfstTwoLevelPaths paths;
      this->shortest_string(topmost, paths);

      if (paths.size() == 0) {
        OUTPUT(<< "transducer is empty");
      }
      else {
        *oss << (int)(paths.begin()->second.size()) << std::endl;
      }
      PROMPT_AND_RETURN_THIS;
    }

  // LEAVE
  XfstCompiler&
  XfstCompiler::print_one_string_or_its_size
  (std::ostream * oss, const HfstTwoLevelPaths & paths, const char * level, bool print_size)
  {
    assert(level != NULL);
    *oss << std::string(level) << ": ";

    if (print_size)
      {
        *oss << (int)paths.begin()->second.size() << std::endl;
      }
    else
      {
        print_paths(paths, oss, 1);
      }
    return *this;
  }

  // LEAVE
  XfstCompiler&
  XfstCompiler::print_longest_string_or_its_size(std::ostream * oss, bool print_size)
  {
    GET_TOP(topmost);

    // Variables needed to find out some properties about the transducer
    HfstTransducer tmp_lower(*topmost);
    HfstTransducer tmp_upper(*topmost);
    tmp_lower.output_project().remove_epsilons();
    tmp_upper.input_project().remove_epsilons();

    HfstTwoLevelPaths paths_upper;
    HfstTwoLevelPaths paths_lower;
    bool upper_is_cyclic = false;
    bool lower_is_cyclic = false;
    bool transducer_is_empty = false;

    try { // Transducer is empty if neither upper..
      transducer_is_empty =
        ! tmp_upper.extract_longest_paths(paths_upper,
                                          variables_["obey-flags"] == "ON");
    }
    catch (const TransducerIsCyclicException & e) {
      (void)e;
      upper_is_cyclic = true;
    }
    
    try { // ..nor lower paths can be extracted.
      transducer_is_empty =
        ! tmp_lower.extract_longest_paths(paths_lower,
                                          variables_["obey-flags"] == "ON");
    }
    catch (const TransducerIsCyclicException & e) {
      (void)e;
      lower_is_cyclic = true;
    }

    // Print the results:
    // first, the special cases,
    if (upper_is_cyclic && lower_is_cyclic) {
      OUTPUT(<< "transducer is cyclic");
    }
    else if (transducer_is_empty) {
      OUTPUT(<< "transducer is empty");
    }
    // then the usual:
    else {
      // warn about flag diacritics
      if (variables_["show-flags"] == "OFF" &&
          (tmp_upper.has_flag_diacritics() ||
           tmp_lower.has_flag_diacritics()) )
        {
          ERROR(<< "warning: longest string may have flag diacritics that are not shown");
          ERROR(<< "         but are used in calculating its length (use 'eliminate flags')");
        }
      
      // print one longest string of the upper level, if not cyclic
      if (upper_is_cyclic) {
        *oss << "Upper level is cyclic." << std::endl; }
      else {
        print_one_string_or_its_size(oss, paths_upper, "Upper", print_size); }
      
      // print one longest string of the lower level, if not cyclic
        if (lower_is_cyclic) {
          *oss << "Lower level is cyclic." << std::endl; }
        else {
          print_one_string_or_its_size(oss, paths_lower, "Lower", print_size); }
    }

    PROMPT_AND_RETURN_THIS;
  }

  XfstCompiler&
  XfstCompiler::print_longest_string(std::ostream * oss)
    {
      return print_longest_string_or_its_size(oss, false);
    }

  XfstCompiler&
  XfstCompiler::print_longest_string_size(std::ostream * oss)
    {
      return print_longest_string_or_its_size(oss, true);
    }

  XfstCompiler&
  XfstCompiler::print_lower_words(const char * name, unsigned int number,
                                  std::ostream * oss)
    {
      return print_words(name, number, oss, LOWER_LEVEL);
    }
  XfstCompiler&
  XfstCompiler::print_random_lower(const char * name, unsigned int number, std::ostream * oss)
    {
      hfst::HfstTwoLevelPaths paths;

      HfstTransducer tmp(format_);
      if (name == NULL)
        {
          HfstTransducer * temp = this->top();
          if (temp == NULL)
            { return *this; }
          tmp = *temp;
        }
      else
        {
          std::map<std::string, HfstTransducer*>::const_iterator it
            = definitions_.find(name);
          if (it == definitions_.end())
            {
              *oss << "no such definition '" << std::string(name) << "'" << std::endl;
              prompt();
              return *this;
            }
          else
            {
              tmp = *(it->second);
            }
        }

      tmp.output_project();
      tmp.extract_random_paths(paths, number);
      print_paths(paths, oss);
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::print_upper_words(const char * name, unsigned int number,
                                  std::ostream * oss)
    {
      return print_words(name, number, oss, UPPER_LEVEL);
    }
  XfstCompiler&
  XfstCompiler::print_random_upper(const char * name, unsigned int number, std::ostream * oss)
    {
      hfst::HfstTwoLevelPaths paths;

      HfstTransducer tmp(format_);
      if (name == NULL)
        {
          HfstTransducer * temp = this->top();
          if (temp == NULL)
            { return *this; }
          tmp = *temp;
        }
      else
        {
          std::map<std::string, HfstTransducer*>::const_iterator it
            = definitions_.find(name);
          if (it == definitions_.end())
            {
              *oss << "no such definition '" << std::string(name) << std::endl;
              prompt();
              return *this;
            }
          else
            {
              tmp = *(it->second);
            }
        }

      tmp.input_project();
      tmp.extract_random_paths(paths, number);
      print_paths(paths, oss);
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::print_words(const char * name, unsigned int number,
                            std::ostream * oss)
  {
    return print_words(name, number, oss, BOTH_LEVELS);
  }

  // LEAVE
  XfstCompiler&
  XfstCompiler::print_words(const char * name, unsigned int number,
                            std::ostream * oss, Level level)
    {
      HfstTransducer temp(format_);
      if (name == NULL)
        {
          HfstTransducer * tmp = this->top();
          if (tmp == NULL)
            { return *this; }
          temp = *tmp;
        }
      else
        {
          std::map<std::string, HfstTransducer*>::const_iterator it
            = definitions_.find(name);
          if (it == definitions_.end())
            {
              *oss << "no such definition '" << std::string(name) << "'" << std::endl;
              prompt();
              return *this;
            }
          else
            {
              temp = *(it->second);
            }
        }

      switch (level)
        {
        case UPPER_LEVEL:
          temp.input_project();
          break;
        case LOWER_LEVEL:
          temp.output_project();
          break;
        case BOTH_LEVELS:
          break;
        default:
          ERROR(<< "ERROR: argument given to function 'print_words' not recognized");
          PROMPT_AND_RETURN_THIS;
        }

      HfstTwoLevelPaths results;

      try
        {
          if (variables_["obey-flags"] == "OFF")
            temp.extract_paths(results, number, -1);
          else
            temp.extract_paths_fd(results, number, -1);
        }
      catch (const TransducerIsCyclicException & e)
        {
          (void)e;
          int cutoff = hfst::size_t_to_uint(string_to_size_t(variables_["print-words-cycle-cutoff"]));
          ERROR(<< "warning: transducer is cyclic, limiting the number of cycles to " << cutoff);

          if (variables_["obey-flags"] == "OFF")
            temp.extract_paths(results, number, cutoff);
          else
            temp.extract_paths_fd(results, number, cutoff);
        }

      print_paths(results, oss);

      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::print_random_words(const char * name, unsigned int number, std::ostream * oss)
    {
      const HfstTransducer * tmp = NULL;
      if (name == NULL)
        {
          tmp = this->top();
          if (tmp == NULL)
            { return *this; }
        }
      else
        {
          std::map<std::string, HfstTransducer*>::const_iterator it
            = definitions_.find(name);
          if (it == definitions_.end())
            {
              *oss << "no such definition '" << std::string(name) << "'" << std::endl;
              prompt();
              return *this;
            }
          else
            {
              tmp = it->second;
            }
        }

      hfst::HfstTwoLevelPaths paths;
      tmp->extract_random_paths(paths, number);
      print_paths(paths, oss);
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::print_name(std::ostream * oss)
    {
      GET_TOP(tmp);

      for (std::map<std::string, HfstTransducer*>::const_iterator it
             = names_.begin(); it != names_.end(); it++)
        {
          if (tmp == it->second)
            {
              *oss << "Name " << it->first << std::endl;
              PROMPT_AND_RETURN_THIS;
            }
        }

      *oss << "No name." << std::endl;
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::view_net()
  {
#ifdef WINDOWS
    ERROR(<< "view net not implemented for windows");
    PROMPT_AND_RETURN_THIS;
#else

    GET_TOP(tmp);

    // command display found
    if (::system("which display > /dev/null 2> /dev/null") == 0)
      {
	char * dotfilename = tempnam(NULL, "hfst");
	if (verbose_)
	  {
	    ERROR(<< "Writing net in dot format to temporary file '" << dotfilename << "'.");
	  }
	FILE * dotfile = hfst::hfst_fopen(dotfilename, "wb");
	hfst::print_dot(dotfile, *tmp);
	fclose(dotfile);

	if (verbose_)
	  {
	    ERROR(<< "Viewing the graph.");
	  }
	std::string command = std::string("display ") + std::string(dotfilename);
	if (::system(command.c_str()) != 0)
	  {
	    ERROR(<< "Viewing failed.");
	    xfst_lesser_fail();
	  }
	PROMPT_AND_RETURN_THIS;
      }

    // dot and xdg-open must be used instead
    if (::system("which dot > /dev/null 2> /dev/null") != 0)
      {
	ERROR(<< "Missing commands 'display' and 'dot'.");
	xfst_lesser_fail();
	PROMPT_AND_RETURN_THIS;
      }
    if (::system("which xdg-open > /dev/null 2> /dev/null") != 0)
      {
	ERROR(<< "Missing commands 'display' and 'xdg-open'.");
	xfst_lesser_fail();
	PROMPT_AND_RETURN_THIS;
      }
    char * dotfilename = tempnam(NULL, "hfst");
    char * pngfilename = tempnam(NULL, "hfst");
    if (verbose_)
      {
        ERROR(<< "Writing net in dot format to temporary file '" << dotfilename << "'.");
      }
    FILE * dotfile = hfst::hfst_fopen(dotfilename, "wb");
    hfst::print_dot(dotfile, *tmp);
    fclose(dotfile);
    if (verbose_)
      {
        ERROR(<< "Converting into png format.");
      }
    std::string command = std::string("dot -Tpng ") + std::string(dotfilename) + std::string(" > ") + std::string(pngfilename) + std::string(" 2> /dev/null");
    if (::system(command.c_str()) != 0)
      {
        ERROR(<< "Converting failed.");
        xfst_lesser_fail();
      }
    if (verbose_)
      {
        ERROR(<< "Viewing the graph.");
      }
    command = std::string("xdg-open ") + std::string(pngfilename);
    if (::system(command.c_str()) != 0)
      {
        ERROR(<< "Viewing failed.");
        xfst_lesser_fail();
      }
    PROMPT_AND_RETURN_THIS;
#endif // WINDOWS
  }

  XfstCompiler&
  XfstCompiler::print_net(std::ostream * oss)
    {
      if (variables_["print-sigma"] == "ON")
        {
          this->print_sigma(oss, false /*do not prompt*/);
        }
      GET_TOP(tmp);
      HfstIterableTransducer basic(*tmp);
      basic.write_in_xfst_format(*oss, variables_["print-weight"] == "ON");
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::print_net(const char* name, std::ostream * oss)
    {
      std::map<std::string,hfst::HfstTransducer*>::const_iterator it =
        definitions_.find(name);
      if (it == definitions_.end())
        {
          ERROR(<< "no such defined network: '" << name << "'");
          PROMPT_AND_RETURN_THIS;
        }
      else
        {
          if (variables_["print-sigma"] == "ON")
            {
              stack_.push(it->second);
              this->print_sigma(oss, false /*do not prompt*/);
              stack_.pop();
            }
          HfstIterableTransducer basic(*(it->second));
          basic.write_in_xfst_format(*oss, variables_["print-weight"] == "ON");
          PROMPT_AND_RETURN_THIS;
        }
    }

  static bool is_special_symbol(const std::string &s)
  {
    if (s == hfst::internal_epsilon || s == hfst::internal_unknown ||
        s == hfst::internal_identity ) {
      return true;
    }
    return false;
  }

  // LEAVE
  void XfstCompiler::print_alphabet
  (const StringSet & alpha, bool unknown, bool identity, std::ostream * oss)
  {
    unsigned int sigma_count=0;
    *oss << "Sigma: ";

    if (variables_["print-foma-sigma"] == "ON")
      {
        if (unknown)
          *oss << "?";
        if (identity)
          {
            if (unknown)
              *oss << ", ";
            *oss << "@";
          }
      }
    else // xfst-style sigma print
      {
        if (unknown || identity)
          *oss << "?";
      }

    bool first_symbol = true;
    for (StringSet::const_iterator it = alpha.begin(); it != alpha.end(); it++)
      {
        if (! is_special_symbol(*it))
          {
            if (!first_symbol || unknown || identity)
              *oss << ", ";
            if (*it == "?")
              *oss << "\"?\"";
            else if (*it == "@" && variables_["print-foma-sigma"] == "ON")
              *oss << "\"@\"";
            else
              *oss << *it;
            sigma_count++;
            first_symbol = false;
          }
      }
    *oss << std::endl;
    *oss << "Size: " << sigma_count << "." << std::endl;
  }

  static bool is_unknown_or_identity_used_in_transducer
  (HfstTransducer * t, bool & unknown, bool & identity)
  {
    unknown = false;
    identity = false;

    HfstIterableTransducer fsm(*t);
    for (HfstIterableTransducer::const_iterator it = fsm.begin();
         it != fsm.end(); it++ )
      {
        for (hfst::implementations::HfstTransitions::const_iterator tr_it
               = it->begin(); tr_it != it->end(); tr_it++)
          {
            std::string istr = tr_it->get_input_symbol();
            std::string ostr = tr_it->get_input_symbol();
            if (istr == hfst::internal_unknown ||
                ostr == hfst::internal_unknown)
              unknown = true;
            else if (istr == hfst::internal_identity ||
                     ostr == hfst::internal_identity) // should not happen
              identity = true;
            else
              ;
            if (unknown == true && identity == true)
              return true;
          }
      }
    if (unknown == true || identity == true)
      return true;
    else
      return false;
  }

  // todo: flags?
  XfstCompiler&
  XfstCompiler::print_sigma(std::ostream * oss, bool prompt)
    {
      GET_TOP(t);
      hfst::StringSet alpha = t->get_alphabet();

      // find out whether unknown or identity is used in transitions
      bool unknown = false;
      bool identity = false;
      (void)is_unknown_or_identity_used_in_transducer(t, unknown, identity);

      print_alphabet(alpha, unknown, identity, oss);
      if (prompt) {
        this->prompt(); }
      return *this;
    }

  XfstCompiler&
  XfstCompiler::print_sigma(const char* /*name*/, std::ostream * oss)
    {
      ERROR(<< "missing print sigma");
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::print_sigma_count(std::ostream * oss)
    {
      ERROR(<< "missing print sigma count");
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::print_sigma_word_count(const char* level, std::ostream * oss)
    {
      ERROR(<< "missing " << level << " sigma word count");
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::print_sigma_word_count(std::ostream * oss)
    {
      ERROR(<< "missing sigma word count");
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::print_size(const char* name, std::ostream * oss)
    {
      oss->width(10);
      *oss << name << ": " << "? bytes. ? states, ? arcs, ? paths." << std::endl;
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::print_size(std::ostream * oss)
    {
      *oss << "? bytes. ? states, ? arcs, ? paths." << std::endl;
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::print_stack(std::ostream * oss)
    {
      stack<HfstTransducer*> tmp;
      int i = 0;
      while (!stack_.empty())
        {
          oss->width(10);
          *oss << i << ": ? bytes. ? states, ? arcs, ? paths." << std::endl;
          tmp.push(stack_.top());
          stack_.pop();
          i++;
        }
      while (!tmp.empty())
        {
          stack_.push(tmp.top());
          tmp.pop();
        }
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::write_dot(std::ostream * oss)
    {
      if (stack_.size() < 1)
        {
          EMPTY_STACK;
          xfst_lesser_fail();
          PROMPT_AND_RETURN_THIS;
        }
      GET_TOP(tmp);
      hfst::print_dot(*oss, *tmp);
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::write_dot(const char* name, std::ostream * oss_)
  {
    if (stack_.size() < 1)
      {
        EMPTY_STACK;
        xfst_lesser_fail();
        PROMPT_AND_RETURN_THIS;
      }
    FILE * outfile = hfst::hfst_fopen(name, "wb");
    if (outfile == NULL)
      {
        ERROR(<< "Could not open file " << name);
        xfst_fail();
        PROMPT_AND_RETURN_THIS;
      }
    GET_TOP(tmp);
    hfst::print_dot(outfile, *tmp);
    PROMPT_AND_RETURN_THIS;
  }
  XfstCompiler&
  XfstCompiler::write_prolog(std::ostream * oss)
    {
      if (stack_.size() < 1)
        {
          EMPTY_STACK;
          xfst_lesser_fail();
          PROMPT_AND_RETURN_THIS;
        }
      std::stack<hfst::HfstTransducer*> reverse_stack;
      while (stack_.size() != 0)
        {
          HfstTransducer * tr = stack_.top();
          std::string name = tr->get_name();
          if (name == "")
            name = "NO_NAME";
          HfstIterableTransducer fsm(*tr);
          fsm.write_in_prolog_format(*oss, name, variables_["print-weight"] == "ON");
          if (stack_.size() != 1) // separator
            *oss << std::endl;
          reverse_stack.push(tr);
          stack_.pop();
        }
      while (reverse_stack.size() != 0)
        {
          HfstTransducer * tr = reverse_stack.top();
          stack_.push(tr);
          reverse_stack.pop();
        }
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::write_spaced(std::ostream * oss)
    {
      ERROR(<< "missing write spaced");
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::write_text(std::ostream * oss)
    {
      ERROR(<< "missing write text");
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::write_function(const char* name, const char* outfile)
    {
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::write_definition(const char* name, const char* outfile)
    {
      if (definitions_.find(name) == definitions_.end())
        {
          ERROR(<< "no such defined network: '" << name << "'");
          PROMPT_AND_RETURN_THIS;
        }

      HfstOutputStream* outstream = (outfile != 0) ?
        new HfstOutputStream(outfile, format_):
        new HfstOutputStream(format_);
      HfstTransducer tmp(*(definitions_[name]));
      if (variables_["name-nets"] == "ON")
        tmp.set_name(name);
      *outstream << tmp;
      outstream->close();
      delete outstream;
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::write_definitions(const char* outfile)
    {
      if (definitions_.empty())
        {
          ERROR(<< "no defined networks");
          PROMPT_AND_RETURN_THIS;
        }

      HfstOutputStream* outstream = (outfile != 0) ?
        new HfstOutputStream(outfile, format_):
        new HfstOutputStream(format_);
      for (map<string,HfstTransducer*>::iterator def = definitions_.begin();
           def != definitions_.end();
           ++def)
        {
          HfstTransducer tmp(*(def->second));
          tmp.set_name(def->first);
          *outstream << tmp;
        }
      outstream->close();
      delete outstream;
      PROMPT_AND_RETURN_THIS;
    }

  bool
  XfstCompiler::check_filename(const char * filename)
  {
    if (restricted_mode_)
      {
	std::string fn(filename);
	if ((fn.find('/') != std::string::npos) || (fn.find('\\') != std::string::npos))
	  {
	    ERROR(<< "Restricted mode (--restricted-mode) is in use, write and read operations are allowed");
	    ERROR(<< "only in current directory (i.e. filenames cannot contain '/' or '\\')");
	    xfst_lesser_fail();
	    prompt();
	    return false;
	  }
      }
    prompt();
    return true;
  }
  
  XfstCompiler&
  XfstCompiler::write_stack(const char* filename)
    {
    if (stack_.size() < 1)
      {
        EMPTY_STACK;
        xfst_lesser_fail();
        return *this;
      }

    CHECK_FILENAME(filename);
        
      HfstOutputStream* outstream = (filename != 0)?
        new HfstOutputStream(filename, stack_.top()->get_type()):
        new HfstOutputStream(stack_.top()->get_type());
      stack<HfstTransducer*> tmp;
      while (!stack_.empty())
        {
          tmp.push(stack_.top());
          stack_.pop();
        }
      while (!tmp.empty())
        {
          *outstream << *(tmp.top());
          stack_.push(tmp.top());
          tmp.pop();
        }
      outstream->close();
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::read_props(FILE* infile)
    {
        char* line = 0;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, infile)) != -1)
          {
            add_prop_line(line);
          }
        free(line);
        PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::read_props(const char* indata)
    {
        char* s = strdup(indata);
        char* line = strtok(s, "\n");
        while (line != NULL)
          {
            add_prop_line(line);
            line = strtok(NULL, "\n");
          }
        free(s);
        PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::read_regex(FILE* infile)
    {
#define MAX_FILE_SIZE 10000000
      HfstTransducer* compiled = NULL;
      size_t read = 0;
      char* file_data = static_cast<char*>
        (malloc(sizeof(char)*MAX_FILE_SIZE+1));
      read = fread(file_data, sizeof(char), MAX_FILE_SIZE, infile);
      if ((read > 0) && (read < MAX_FILE_SIZE) && (feof(infile)))
        {
          compiled = xre_.compile(file_data);  // XRE
          if (compiled != NULL)
            {
              MAYBE_MINIMIZE(compiled);
              stack_.push(compiled);
            }
          else
            {
              *error_<< "Error when compiling regex file." << std::endl;
              xfst_fail();
            }
        }
      else if (!feof(infile))
        {
          ERROR(<< "regex file longer than buffer :-(");
        }
      if (compiled != NULL)
        {
          print_transducer_info();
        }
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::compile_regex(const char* indata, unsigned int & chars_read)
  {
    if (latest_regex_compiled != NULL)
      {
        delete latest_regex_compiled;
        latest_regex_compiled = NULL;
      }
    latest_regex_compiled = xre_.compile_first(indata, chars_read);  // XRE
    return *this;
  }

  XfstCompiler&
  XfstCompiler::read_regex(const char* indata)
    {
      // When calling this function, the regex \a indata should already have
      // been compiled into a transducer which should have been stored to
      // the variable latest_regex_compiled.
      HfstTransducer* compiled = latest_regex_compiled;
      if (compiled != NULL)
        {
          stack_.push(new HfstTransducer(*compiled));
          MAYBE_MINIMIZE(stack_.top());
          print_transducer_info();
        }
      else
        {
          ERROR(<< "Error reading regex '" << indata << "'.");
          xfst_fail();
        }
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::read_prolog(FILE* infile)
    {
      try {
        unsigned int linecount=0;
        HfstIterableTransducer tr =
          HfstIterableTransducer::read_in_prolog_format(infile, linecount);
        stack_.push(new HfstTransducer(tr, format_));
        MAYBE_MINIMIZE(stack_.top());
        PRINT_INFO_PROMPT_AND_RETURN_THIS;
      }
      catch (const NotValidPrologFormatException & e)
        {
          ERROR(<< e());
          PROMPT_AND_RETURN_THIS;
        }
    }
  XfstCompiler&
  XfstCompiler::read_prolog(const char* /* indata */)
    {
      ERROR(<< "missing read prolog");
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::read_spaced_from_file(const char * filename)
    {
      CHECK_FILENAME(filename);
      return this->read_text_or_spaced(filename, true); // spaces are used
    }
  XfstCompiler&
  XfstCompiler::read_spaced(const char* /* indata */)
    {
      ERROR(<< "missing read spaced");
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::read_text_or_spaced(const char * filename, bool spaces)
  {
    CHECK_FILENAME(filename);
    FILE * infile = hfst::hfst_fopen(filename, "r");
    if (infile == NULL)
      {
        ERROR(<< "Could not open file " << filename);
        xfst_fail();
        prompt();
        return *this;
      }

    HfstTransducer * tmp = new HfstTransducer(format_);
    StringVector mcs; // no multichar symbols
    HfstStrings2FstTokenizer tok(mcs, hfst::internal_epsilon);
    char * line;
    
    while( (line = xfst_getline(infile)) != NULL )
      {
        line = remove_newline(line);
        StringPairVector spv = tok.tokenize_pair_string(std::string(line), spaces);
        HfstTransducer line_tr(spv, format_);
        tmp->disjunct(line_tr);
        free(line);
      }
    
    if (0 != fclose(infile))
      {
        ERROR(<< "Could not close file ");
        xfst_fail();
      }
      
    tmp->minimize(); // a trie should be easily minimizable
    stack_.push(tmp);
    PRINT_INFO_PROMPT_AND_RETURN_THIS;
  }
  XfstCompiler&
  XfstCompiler::read_text_from_file(const char * filename)
    {
      CHECK_FILENAME(filename);
      return this->read_text_or_spaced(filename, false); // spaces are not used
    }
  XfstCompiler&
  XfstCompiler::read_text(const char* /* indata */)
    {
      ERROR(<< "missing read text");
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::cleanup_net()
    {
      ERROR(<< "cannot cleanup net");

      if (stack_.size() < 1)
        {
          EMPTY_STACK;
          xfst_lesser_fail();
          return *this;
        }
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::complete_net()
    {
      GET_TOP(topmost);

      HfstIterableTransducer fsm(*topmost);
      fsm.complete();
      HfstTransducer * result = new HfstTransducer(fsm, topmost->get_type());
      stack_.pop();
      delete topmost;
      MAYBE_MINIMIZE(result);
      stack_.push(result);
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::compose_net()
    {
      return this->apply_binary_operation_iteratively(COMPOSE_NET);
    }

  XfstCompiler&
  XfstCompiler::concatenate_net()
    {
      return this->apply_binary_operation_iteratively(CONCATENATE_NET);
    }
  XfstCompiler&
  XfstCompiler::crossproduct_net()
    {
      return this->apply_binary_operation(CROSSPRODUCT_NET);
    }

  XfstCompiler&
  XfstCompiler::apply_unary_operation(UnaryOperation operation)
  {
    GET_TOP(result);
    this->pop();

    try
      {
        switch (operation)
          {
          case DETERMINIZE_NET:
            result->determinize();
            break;
          case EPSILON_REMOVE_NET:
            result->remove_epsilons();
            break;
          case INVERT_NET:
            result->invert();
            break;
          case LOWER_SIDE_NET:
            result->output_project();
            break;
          case UPPER_SIDE_NET:
            result->input_project();
            break;
          case ZERO_PLUS_NET:
            result->repeat_star();
            break;
          case ONE_PLUS_NET:
            result->repeat_plus();
            break;
          case OPTIONAL_NET:
            result->optionalize();
            break;
          case REVERSE_NET:
            result->reverse();
            break;
          case MINIMIZE_NET:
            result->minimize(); // implicit minimization requested, do not use optimize()
            break;
          case PRUNE_NET_:
            result->prune();
            break;
          default:
            ERROR(<< "ERROR: unknown unary operation");
            break;
          }

        if (operation != MINIMIZE_NET && operation != DETERMINIZE_NET && operation != EPSILON_REMOVE_NET)
          {
            MAYBE_MINIMIZE(result);
          }
        stack_.push(result);
        print_transducer_info();
      }
    catch (const FunctionNotImplementedException & e)
      {
        (void)e;
        ERROR(<< "function not available");
        xfst_fail();
        stack_.push(result);
      }

    PROMPT_AND_RETURN_THIS;
  }

  XfstCompiler&
  XfstCompiler::apply_binary_operation(BinaryOperation operation)
  {
      if (stack_.size() < 2)
        {
          ERROR(<< "Not enough networks on stack. Operation requires at least 2.");
          xfst_lesser_fail();
          return *this;
        }
      HfstTransducer * result = stack_.top();
      stack_.pop();
      HfstTransducer * another = stack_.top();
      stack_.pop();

      switch (operation)
        {
        case IGNORE_NET:
          result->insert_freely(*another);
          break;
        case MINUS_NET:
          result->subtract(*another);
          break;
        case (CROSSPRODUCT_NET):
          try
            {
              result->cross_product(*another);
              break;
            }
          catch (const TransducersAreNotAutomataException & e)
            {
              (void)e;
              ERROR(<< "transducers are not automata");
              xfst_fail();
              stack_.push(another);
              stack_.push(result);
              prompt();
              return *this;
              break;
            }
        default:
          ERROR(<< "ERROR: unknown binary operation");
          xfst_fail();
          break;
        }

      MAYBE_MINIMIZE(result);
      delete another;
      stack_.push(result);
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
  }


  XfstCompiler&
  XfstCompiler::apply_binary_operation_iteratively(BinaryOperation operation)
  {
    if (stack_.size() < 2)
      {
        ERROR(<< "Not enough networks on stack. Operation requires at least 2.");
        xfst_lesser_fail();
        return *this;
      }
    HfstTransducer* result = stack_.top();
    //HfstIterableTransducer fsm(*result);

    stack_.pop();
    while (!stack_.empty())
      {
        HfstTransducer* t = stack_.top();

        if (t->get_type() != result->get_type())
          {
            ERROR(<< "Stack contains transducers whose type differs.");
            xfst_lesser_fail();
            break;
          }

        switch (operation)
          {
          case INTERSECT_NET:
            result->intersect(*t);
            break;
          case IGNORE_NET:
            result->insert_freely(*t);
            break;
          case COMPOSE_NET:
            {
              if (result->has_flag_diacritics() && t->has_flag_diacritics())
                {
                  if (variables_["harmonize-flags"] == "OFF")
                    {
                      if (verbose_)
                        {
                          ERROR(<< "Both composition arguments contain flag diacritics. Set harmonize-flags ON to harmonize them.");
                        }
                    }
                  else
                    {
                      result->harmonize_flag_diacritics(*t);
                    }
                }

              try
                {
                  result->compose(*t);
                }
              catch (const FlagDiacriticsAreNotIdentitiesException & e)
                {
                  (void)e;
                  ERROR(<< "Error: flag diacritics must be identities in composition if flag-is-epsilon is ON.");
                  ERROR(<< "I.e. only FLAG:FLAG is allowed, not FLAG1:FLAG2, FLAG:bar or foo:FLAG");
                  ERROR(<< "Apply twosided flag-diacritics (tfd) before composition.");
                  xfst_lesser_fail();
                  prompt();
                  return *this;
                }

              break;
            }
          case CONCATENATE_NET:
            result->concatenate(*t);
            break;
          case UNION_NET:
            result->disjunct(*t);
            break;
          case SHUFFLE_NET:
            result->shuffle(*t);
            break;
          default:
            ERROR(<< "ERROR: unknown binary operation");
            break;
          }
        stack_.pop();
        delete t;
      }
    MAYBE_MINIMIZE(result);
    stack_.push(result);
    PRINT_INFO_PROMPT_AND_RETURN_THIS;
  }

  XfstCompiler&
  XfstCompiler::determinize_net()
    {
      return this->apply_unary_operation(DETERMINIZE_NET);
    }
  XfstCompiler&
  XfstCompiler::epsilon_remove_net()
    {
      return this->apply_unary_operation(EPSILON_REMOVE_NET);
    }
  XfstCompiler&
  XfstCompiler::ignore_net()
    {
      return this->apply_binary_operation(IGNORE_NET);
    }
  XfstCompiler&
  XfstCompiler::intersect_net()
    {
      return this->apply_binary_operation_iteratively(INTERSECT_NET);
    }
  XfstCompiler&
  XfstCompiler::invert_net()
    {
      return this->apply_unary_operation(INVERT_NET);
    }
  XfstCompiler&
  XfstCompiler::label_net()
    {
      GET_TOP(topmost);

      HfstTransducer * result = new HfstTransducer(topmost->get_type());
      
      std::set<std::pair<std::string, std::string> > label_set;
      HfstIterableTransducer fsm(*topmost);
      
      for (HfstIterableTransducer::const_iterator it = fsm.begin();
           it != fsm.end(); it++ )
        {
          for (hfst::implementations::HfstTransitions::const_iterator tr_it
                 = it->begin(); tr_it != it->end(); tr_it++)
            {
              std::pair<std::string, std::string> label_pair
                (tr_it->get_input_symbol(), tr_it->get_output_symbol());
              label_set.insert(label_pair);
            }
        }

      for (std::set<std::pair<std::string, std::string> >::const_iterator
             it = label_set.begin(); it != label_set.end(); it++)
        {
          HfstTransducer label_tr(it->first, it->second, result->get_type());
          result->disjunct(label_tr);
        }

      result->minimize(); // should be safe to minimize
      stack_.pop();
      delete topmost;
      stack_.push(result);

      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::lower_side_net()
    {
      return this->apply_unary_operation(LOWER_SIDE_NET);
    }
  XfstCompiler&
  XfstCompiler::upper_side_net()
    {
      return this->apply_unary_operation(UPPER_SIDE_NET);
    }
  XfstCompiler&
  XfstCompiler::minimize_net()
    {
      return this->apply_unary_operation(MINIMIZE_NET);
    }
  XfstCompiler&
  XfstCompiler::minus_net()
    {
      return this->apply_binary_operation(MINUS_NET);
    }
  XfstCompiler&
  XfstCompiler::name_net(const char* s)
    {
      if (stack_.size() < 1)
        {
          EMPTY_STACK;
          xfst_lesser_fail();
          return *this;
        }
      HfstTransducer* t = stack_.top();
      t->set_name(s);
      names_[s] = t;
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::negate_net()
    {
      if (stack_.size() < 1)
        {
          EMPTY_STACK;
          xfst_lesser_fail();
          return *this;
        }

      HfstTransducer* t = stack_.top();

      try
	{
	  t->negate();
	}
      catch (const TransducerIsNotAutomatonException & e)
	{
	  (void)e;
	  ERROR(<< "Error: Negation is defined only for automata.");
	  ERROR(<< "Use expression [[?:?]* - A] instead where A is the transducer to be negated.");
	  xfst_lesser_fail();
	  return *this;
	}
      
      MAYBE_MINIMIZE(t);
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::one_plus_net()
    {
      return this->apply_unary_operation(ONE_PLUS_NET);
    }
  XfstCompiler&
  XfstCompiler::zero_plus_net()
    {
      return this->apply_unary_operation(ZERO_PLUS_NET);
    }
  XfstCompiler&
  XfstCompiler::optional_net()
    {
      return this->apply_unary_operation(OPTIONAL_NET);
    }
  XfstCompiler&
  XfstCompiler::prune_net()
    {
      return this->apply_unary_operation(PRUNE_NET_);
    }
  XfstCompiler&
  XfstCompiler::reverse_net()
    {
      return this->apply_unary_operation(REVERSE_NET);
    }
  XfstCompiler&
  XfstCompiler::shuffle_net()
    {
      return this->apply_binary_operation_iteratively(SHUFFLE_NET);
    }
  XfstCompiler&
  XfstCompiler::sigma_net()
    {
      GET_TOP(tmp);

      StringSet alpha = tmp->get_alphabet();
      alpha.erase(hfst::internal_unknown);
      alpha.erase(hfst::internal_identity);
      alpha.erase(hfst::internal_epsilon);
      StringPairSet alpha_ = hfst::symbols::to_string_pair_set(alpha);
      HfstTransducer * sigma = new HfstTransducer(alpha_, format_);
      
      MAYBE_MINIMIZE(sigma);
      stack_.push(sigma);
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::sort_net()
    {
      ERROR(<< "missing sort net");
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::substring_net()
    {
      ERROR(<< "missing substring net");
      PRINT_INFO_PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::union_net()
    {
      return this->apply_binary_operation_iteratively(UNION_NET);
    }

  XfstCompiler&
  XfstCompiler::print_file_info(std::ostream * oss)
    {
      ERROR(<< "file info not implemented (cf. summarize)");
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::print_properties(const char* /* name */, std::ostream * oss)
    {
      ERROR(<< "missing print properties");
      PROMPT_AND_RETURN_THIS;
    }

  // For 'inspect_net': print to outstream_ all arcs in
  // \a transitions. Return the number of arcs.
  unsigned int XfstCompiler::print_arcs
  (const hfst::implementations::HfstTransitions & transitions)
  {
    bool first_loop = true;
    unsigned int arc_number = 1;
    for (hfst::implementations::HfstTransitions::const_iterator it
           = transitions.begin(); it != transitions.end(); it++)
      {
        if (first_loop)
          {
            OUTPUT_LINE(<< "Arcs:");
            first_loop = false;
          }
        else
          {
            OUTPUT_LINE(<< ", ");
          }

        std::string isymbol = it->get_input_symbol();
        std::string osymbol = it->get_output_symbol();

        if (isymbol == osymbol)
          {
            OUTPUT_LINE(<< " " << arc_number << ". " << isymbol);
          }
        else
          {
            OUTPUT_LINE(<< " " << arc_number << ". " << isymbol << ":" << osymbol);
          }
        arc_number++;
      }
    OUTPUT_END;

    return arc_number - 1;
  }

  // For 'inspect_net': print current level.
  void XfstCompiler::print_level
  (const std::vector<unsigned int> & whole_path,
   const std::vector<unsigned int> & shortest_path)
  {
    OUTPUT_LINE(<< "Level " << (int)whole_path.size());

    if (shortest_path.size() < whole_path.size())
      {
        OUTPUT_LINE(<< " (= " << (int)shortest_path.size() << ")");
      }
  }

  // For 'inspect_net': append state \a state to paths.
  static void append_state_to_paths
  (std::vector<unsigned int> & whole_path,
   std::vector<unsigned int> & shortest_path,
   unsigned int state)
  {
    whole_path.push_back(state);
    for (std::vector<unsigned int>::iterator it = shortest_path.begin();
         it != shortest_path.end(); it++)
      {
        if (*it == state)
          {
            shortest_path.erase(it, shortest_path.end());
            break;
          }
      }
    shortest_path.push_back(state);
  }

  // For 'inspect_net': return to level \a level.
  // Return whether the operation succeeded.
  static bool return_to_level
  (std::vector<unsigned int> & whole_path,
   std::vector<unsigned int> & shortest_path,
   unsigned int level)
  {
    if (whole_path.size() < level || level == 0)
      return false;

    whole_path.erase((whole_path.begin() + level), whole_path.end());
    unsigned int state = whole_path.back();
    for (std::vector<unsigned int>::iterator it = shortest_path.begin();
         it != shortest_path.end(); it++)
      {
        if (*it == state)
          {
            shortest_path.erase(it, shortest_path.end());
            break;
          }
      }
    shortest_path.push_back(state);
    return true;
  }

  char * XfstCompiler::remove_newline(char * line)
  {
    unsigned int i=0;
    while (line[i] != '\0')
      {
        if (line[i] == '\n' || line[i] == '\r')
          {
            line[i] = '\0';
          }
        ++i;
      }
    return line;
  }

  char * XfstCompiler::xfst_getline(FILE * file, const std::string & promptstr)
  {
#ifdef HAVE_READLINE
    if (use_readline_ && file == stdin)
      {
        char *buf = NULL;               // result from readline
        rl_bind_key('\t',rl_insert);     // disable auto-complet
        
        if((buf = readline(promptstr.c_str())) != NULL)
          {
            if (buf[0] != '\0')
              add_history(buf);
          }
        return buf;
      }
#endif

    OUTPUT(<< promptstr);


#ifdef WINDOWS
    // if we are reading directly from console
    if ((file == stdin) && read_interactive_text_from_stdin_)
      {
        std::string str("");
        size_t bufsize = 1000;
        if (hfst::get_line_from_console(str, bufsize))
          {
            return strdup(str.c_str());
          }
        else
          {
            return NULL;
          }
      }
#endif

    char* line_ = 0;
    size_t len = 1024;
    ssize_t read;

    read = getline(&line_, &len, file);
    if (read == -1)
      {
        return NULL;
      }
      return line_;
  }

  int XfstCompiler::current_history_index()
  {
#ifdef HAVE_READLINE
    return history_length;
#else
    return -1;
#endif
  }

  void XfstCompiler::ignore_history_after_index(int index)
  {
#ifdef HAVE_READLINE
    for (unsigned int i=(history_length - 1);
         i > (index - 1); i--)
      {
        remove_history(i);
      }
#endif
  }

  // whether arc \a number can be followed in a state
  // that has \a number_of_arcs arcs.
  bool XfstCompiler::can_arc_be_followed
  (int number, unsigned int number_of_arcs)
  {
    if (number == EOF || number == 0)
      {
        OUTPUT(<< "could not read arc number");
        return false;
      }
    else if (number < 1 || number > (int)number_of_arcs)
      {
        if (number_of_arcs < 1) {
          OUTPUT(<< "state has no arcs"); }
	else {
	  OUTPUT(<< "arc number must be between 1 and " << number_of_arcs); }

        return false;
      }
    return true;
  }

  bool XfstCompiler::can_level_be_reached
  (int level, size_t whole_path_length)
  {
    if (level == EOF || level == 0)
      {
        OUTPUT(<< "could not read level number (type '0' if you wish to exit program)");
        return false;
      }
    else if (level < 0 || level > (int)whole_path_length)
      {
        OUTPUT(<< "no such level: '" << level << "' (current lievel is " << (int)whole_path_length << ")");
        return false;
      }
    return true;
  }

  static const char * inspect_net_help_msg =
    "'N' transits arc N, '-N' returns to level N, '<' "
    "to previous level, '0' quits.\n";

  XfstCompiler&
  XfstCompiler::inspect_net()
    {
      if (!inspect_net_supported_)
	{
	  OUTPUT(<< "inspect net not supported");
	  PROMPT_AND_RETURN_THIS;
	}
      
      GET_TOP(t);

      HfstIterableTransducer net(*t);

      OUTPUT_LINE(<< inspect_net_help_msg);

      // path of states visited, can contain loops
      std::vector<unsigned int> whole_path;
      // shortest path of states to current state, no loops
      std::vector<unsigned int> shortest_path;

      append_state_to_paths(whole_path, shortest_path, 0);
      print_level(whole_path, shortest_path);

      if (net.is_final_state(0))
        OUTPUT_LINE(<< " (final)");
      
      OUTPUT_END;
      
      // transitions of current state
      hfst::implementations::HfstTransitions transitions = net[0];
      // number of arcs in current state
      unsigned int number_of_arcs = print_arcs(transitions);

      char * line;       // line from xfst_getline
      // index after which the history added during inspect_net is ignored
      int ind = current_history_index();

      // the while loop begins, keep on reading from user
      while ((line = xfst_getline(stdin)) != NULL)
        {
          // case (1): back to previous state
          if (strcmp(line, "<\n") == 0 || strcmp(line, "<") == 0)
            {
              if (whole_path.size() < 2)  // exit if already in the start state
                {
                  ignore_history_after_index(ind);
                  free(line);
                  PROMPT_AND_RETURN_THIS;
                }
              else if (! return_to_level(whole_path, shortest_path,
                                         hfst::size_t_to_uint(whole_path.size() - 1)))
                {
                  ERROR(<< "FATAL ERROR: could not return to level '" << (int)(whole_path.size() - 1) << "'");
                  ignore_history_after_index(ind);
                  free(line);
                  PROMPT_AND_RETURN_THIS;
                }
            }
          // case (2): back to state number N
          else if (line[0] == '-')
            {
              int level = atoi(line+1); // skip '-'
              if (! can_level_be_reached(level, whole_path.size()))
                {
                  free(line);
                  continue;
                }
              else if (! return_to_level(whole_path, shortest_path, level))
                {
                  ERROR(<< "FATAL ERROR: could not return to level '" << level << "'");
                  ignore_history_after_index(ind);
                  free(line);
                  PROMPT_AND_RETURN_THIS;
                }
            }
          // case (3): exit program
          else if (strcmp(line, "0\n") == 0 || strcmp(line, "0") == 0)
            {
              ignore_history_after_index(ind);
              free(line);
              PROMPT_AND_RETURN_THIS;
            }
          // case (4): follow arc
          else
            {
              int number = atoi(line); // FIX: atoi is not portable
              if (! can_arc_be_followed(number, number_of_arcs))
                {
                  continue;
                }
              else
                {
                  HfstTransition tr = transitions[number - 1];
                  OUTPUT_LINE(<< "  " << tr.get_input_symbol() << ":" << tr.get_output_symbol() << " --> ");
                  append_state_to_paths(whole_path, shortest_path, tr.get_target_state());
                }
            }

          // update transitions and number of arcs and print information about current level
          transitions = net[whole_path.back()];
          print_level(whole_path, shortest_path);
          if (net.is_final_state(whole_path.back()))
            {
              OUTPUT_LINE(<< " (final)");
            }
          OUTPUT_END;
          number_of_arcs = print_arcs(transitions);

          free(line);
        } // end of while loop

      ignore_history_after_index(ind);
      PROMPT_AND_RETURN_THIS;
    }

  // Returns an automaton that contains one ore more "^[" "^]" expressions.
  static HfstTransducer * contains_regexps(hfst::xre::XreCompiler & xre_)
  {
    HfstTransducer * not_bracket_star = xre_.compile("[? - \"^[\" - \"^]\"]* ;");  // XRE
    xre_.define("TempNotBracketStar", *not_bracket_star);  // XRE
    // all paths that contain one or more well-formed ^[ ^] expressions
    HfstTransducer * well_formed = xre_.compile("TempNotBracketStar \"^[\" TempNotBracketStar  [ \"^]\" TempNotBracketStar \"^[\"  TempNotBracketStar ]*  \"^]\" TempNotBracketStar ;");
    xre_.undefine("TempNotBracketStar");
    delete not_bracket_star;
    return well_formed;
  }

  // XRE
  static HfstTransducer * contains_regexp_markers_on_one_side(hfst::xre::XreCompiler & xre_, bool input_side)
  {
    HfstTransducer * retval = NULL;
    if (input_side)
      {
        retval = xre_.compile("[?:?|0:?|?:0]* [\"^[\":? | \"^]\":? | \"^[\":0 | \"^]\":0] [?:?|0:?|?:0]*");
      }
    else // output side
      {
        retval = xre_.compile("[?:?|0:?|?:0]* [?:\"^[\" | ?:\"^]\" | 0:\"^[\" | 0:\"^]\"] [?:?|0:?|?:0]*");
      }
    assert(retval != NULL);
    return retval;
  }

  // @pre \a t must be an automaton  XRE
  static bool is_well_formed_for_compile_replace(const HfstTransducer * t, hfst::xre::XreCompiler & xre_)
  {
    HfstTransducer * well_formed = contains_regexps(xre_);
    // subtract those paths from copy of t
    HfstTransducer tc(*t);
    tc.subtract(*well_formed);
    delete well_formed;
    // all paths that contain one or more ^[ or ^]
    HfstTransducer * brackets = xre_.compile("$[ \"^[\" | \"^]\" ] ;");

    // test if the result is empty
    tc.intersect(*brackets);
    delete(brackets);
    HfstTransducer empty(tc.get_type());
    bool value = empty.compare(tc, false);
    return value;
  }

  static std::string to_literal_regexp(const hfst::StringPairVector & path, bool input_side)
  {
    std::string pathstr("[");
    for (hfst::StringPairVector::const_iterator it = path.begin(); it != path.end(); it++)
      {
        std::string symbol = (input_side) ? it->first : it->second ;
        if (symbol != hfst::internal_epsilon)
          pathstr.append("\"").append(symbol).append("\" ");
      }
    pathstr.append("]");
    if (pathstr == "[]")
      pathstr = "[0]";
    return pathstr;
  }

  /*  static HfstTransducer * to_literal_transducer(const hfst::StringPairVector & path, hfst::xre::XreCompiler & xre_)
  {
    std::string pathstr("[");
    for (hfst::StringPairVector::const_iterator it = path.begin(); it != path.end(); it++)
      {
        std::string isymbol = it->first;
        if (isymbol == hfst::internal_epsilon)
          {
            pathstr.append("0");
          }
        else
          {
            pathstr.append("\"").append(isymbol).append("\"");
          }
        
        pathstr.append(":");

        std::string osymbol = it->second;
        if (osymbol == hfst::internal_epsilon)
          {
            pathstr.append("0 ");
          }
        else
          {
            pathstr.append("\"").append(osymbol).append("\" ");
          }
      }
    pathstr.append("];");
    if (pathstr == "[];")
      pathstr = "[0];";
    // debug
    //std::cerr << "to_literal_transducer: compiling expression: " << pathstr << std::endl;
    char * p = strdup(pathstr.c_str());
    HfstTransducer * retval = xre_.compile(p);  // XRE
    free(p);
    return retval;
    }*/

  static std::string to_regexp(const hfst::StringPairVector & path, bool input_side, bool retokenize)
  {
    std::string pathstr("[");
    for (hfst::StringPairVector::const_iterator it = path.begin(); it != path.end(); it++)
      {
        std::string symbol = (input_side) ? it->first : it->second ;
        // ignore "^[" and "^]"
        if (symbol != "^]" && symbol != "^[")
          {
            if (symbol != hfst::internal_epsilon)
	      {
		pathstr.append(symbol);
		if (!retokenize)
		  {
		    pathstr.append(" ");
		  }
	      }
          }
	else
	  {
	    // For better alignment
	    pathstr.append("\"@EPSILON_MARKER@\"");
	    if (!retokenize)
	      {
		pathstr.append(" ");
	      }
	  }
      }
    pathstr.append("]");
    if (pathstr == "[]")
      pathstr = "[0]";
    return pathstr;
  }

  XfstCompiler&
  XfstCompiler::compile_replace_net(Level level)
    {
      assert(level != BOTH_LEVELS);
      using hfst::implementations::HfstState;
      using hfst::implementations::HfstReplacement;
      using hfst::implementations::HfstReplacements;
      using hfst::implementations::HfstReplacementsMap;

      GET_TOP(tmp);
      HfstTransducer tmp_cp(*tmp);

      if (level == UPPER_LEVEL)
        {
          tmp_cp.input_project();
        }
      else // LOWER_LEVEL
        {
          tmp_cp.output_project();
        }

      if (is_well_formed_for_compile_replace(&tmp_cp, xre_))
        {
          if (verbose_) {
            ERROR(<< "Network is well-formed.");
	  }
        }
      else
        {
          if (verbose_) {
            ERROR(<< "Network is not well-formed.");
	  }
          xfst_lesser_fail();
          prompt();
          return *this;
        }

      HfstIterableTransducer fsm(*tmp);
      try
        {
          HfstReplacementsMap replacement_map = fsm.find_replacements((level == UPPER_LEVEL));
          
            for (HfstReplacementsMap::const_iterator it = replacement_map.begin();
                 it != replacement_map.end(); it++)
              {
                HfstState start_state = it->first;
                HfstReplacements replacements = it->second;
                for (HfstReplacements::const_iterator rit = replacements.begin();
                     rit != replacements.end(); rit++)
                  {
                   HfstState end_state = rit->first;

		   std::string regexp = to_regexp(rit->second, (level == UPPER_LEVEL), (variables_["retokenize"] == "ON"));
                   std::string literal_regexp = to_literal_regexp(rit->second, (level != UPPER_LEVEL));

                   std::string cross_product_regexp = "[ ";
                   if (level == UPPER_LEVEL)
                     {
                       cross_product_regexp.append(regexp).append(" ] .x. [ ").append(literal_regexp).append(" ]");
                     }
                   else
                     {
                       cross_product_regexp.append(literal_regexp).append(" ] .x. [ ").append(regexp).append(" ]");
                     }

                   HfstTransducer * replacement = xre_.compile(cross_product_regexp);
                   if (replacement == NULL)
                     {
                       ERROR(<< "Could not compile regular expression in compile-replace: " << cross_product_regexp << ".");
                       xfst_lesser_fail();
                       prompt();
                       return *this;
                     }

                   replacement->optimize();
                   HfstIterableTransducer repl(*replacement);
                   delete replacement;
                   fsm.insert_transducer(start_state, end_state, repl);
                  }
              }
        }
      catch(const char * msg)
        {
          ERROR(<< "compile_replace threw an error: '" << msg << "'");
        }
      HfstTransducer * result = new HfstTransducer(fsm, format_);

      // filter out regexps
      HfstTransducer * cr = contains_regexp_markers_on_one_side(xre_, (level == UPPER_LEVEL) /*input side*/);
      cr->optimize();

      result->subtract(*cr).optimize();
      result->substitute("@EPSILON_MARKER@", hfst::internal_epsilon);
      delete cr;
      stack_.pop();
      delete tmp;
      stack_.push(result);

      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::compile_replace_lower_net()
    {
      return compile_replace_net(LOWER_LEVEL);
    }

  XfstCompiler&
  XfstCompiler::compile_replace_upper_net()
    {
      return compile_replace_net(UPPER_LEVEL);
    }


  XfstCompiler&
  XfstCompiler::hfst(const char* text)
    {
      ERROR(<< "HFST: " << text);
      PROMPT_AND_RETURN_THIS;
    }

  XfstCompiler&
  XfstCompiler::read_lexc_from_file(const char * filename)
  {
    CHECK_FILENAME(filename);
    HfstTransducer * t = NULL;
    
    if (variables_["lexc-with-flags"] == "ON")
      {
        lexc_.setWithFlags(true);
        if (variables_["lexc-minimize-flags"] == "ON")
          {
            lexc_.setMinimizeFlags(true);
            if (variables_["lexc-rename-flags"] == "ON")
              {
                lexc_.setRenameFlags(true);
              }
          }
      }

    FILE * infile = hfst::hfst_fopen(filename, "r");
    if (infile == NULL)
      {
        ERROR(<< "could not read lexc file");
        xfst_fail();
        PROMPT_AND_RETURN_THIS;
      }

    if (has_lexc_been_read_)
      lexc_.reset();
    else
      has_lexc_been_read_ = true;

    lexc_.parse(infile);
    t = lexc_.compileLexical();

    if (0 != fclose(infile))
      {
        ERROR(<< "Could not close file " << filename);
        xfst_fail();
      }

    // using foma's lexc implementation
    /*if (! HfstTransducer::is_implementation_type_available(hfst::FOMA_TYPE))
      {
        hfst_fprintf(errorstream_, "foma back-end not enabled, cannot read lexc files\n");
        xfst_fail();
        PROMPT_AND_RETURN_THIS;
      }

    t = HfstTransducer::read_lexc_ptr(std::string(filename), hfst::FOMA_TYPE, verbose_);
    if (t != NULL)
      t->convert(format_);
    */
    
    if (t == NULL)
      {
        ERROR(<< "error compiling file in lexc format");
        xfst_fail();
        PROMPT_AND_RETURN_THIS;
      }

    MAYBE_MINIMIZE(t);
    stack_.push(t);
    print_transducer_info();
    PROMPT_AND_RETURN_THIS;
  }

  XfstCompiler&
  XfstCompiler::read_att_from_file(const char * filename)
    {
      CHECK_FILENAME(filename);
      FILE * infile = hfst::hfst_fopen(filename, "r");
      if (infile == NULL)
        {
          ERROR(<< "could not read att file " << filename);
          xfst_fail();
          PROMPT_AND_RETURN_THIS;
        }

      try
        {
          HfstTransducer * tmp = NULL;
          if (variables_["att-epsilon"].compare("@0@ | @_EPSILON_SYMBOL_@") == 0)
            tmp = new HfstTransducer(infile, format_);
          else
            tmp = new HfstTransducer(infile, format_, variables_["att-epsilon"]);
          MAYBE_MINIMIZE(tmp);
          stack_.push(tmp);
          print_transducer_info();
        }
      catch (const HfstException & e)
        {
          (void)e;
          if (0 != fclose(infile)) {
            ERROR(<< "Could not close file " << filename);
	  }
          ERROR(<< "error reading in att format");
          xfst_fail();
        }
      if (0 != fclose(infile))
        {
          ERROR(<< "could not close att file " << filename);
          xfst_fail();
        }
      PROMPT_AND_RETURN_THIS;
    }
  XfstCompiler&
  XfstCompiler::write_att(std::ostream * oss)
    {
      GET_TOP(tmp);
      HfstIterableTransducer fsm(*tmp);
      fsm.write_in_att_format(*oss, variables_["print-weight"] == "ON");
      PROMPT_AND_RETURN_THIS;
    }
  const std::stack<HfstTransducer*>&
  XfstCompiler::get_stack() const
    {
      return stack_;
    }
  int
  XfstCompiler::parse(FILE* infile)
    {
      hxfstin = infile;
      xfst_ = this;
      int rv = hxfstparse();
      return rv;
    }
  int
  XfstCompiler::parse(const char* filename)
    {
      if (! this->check_filename(filename)) { return -1; }

      hxfstin = hfst::hfst_fopen(filename, "r");
      if (hxfstin == NULL)
        {
          ERROR(<< "could not open " << filename << " for reading");
          return -1;
        }
      xfst_ = this;
      int rv = hxfstparse();
      fclose(hxfstin);
      return rv;
    }
  int
  XfstCompiler::parse_line(char line[])
  {
    hxfstin = NULL;
    xfst_ = this;
    YY_BUFFER_STATE bs = hxfst_scan_string(line);
    int rv = hxfstparse();
    hxfst_delete_buffer(bs);
    /*if (rv != 0)
      {
        prompt();
        }*/
    return rv;
  }
  int
  XfstCompiler::parse_line(std::string line)
  {
    hxfstin = NULL;
    xfst_ = this;
    char * line_ = strdup(line.c_str());
    YY_BUFFER_STATE bs = hxfst_scan_string(line_);
    int rv = hxfstparse();
    hxfst_delete_buffer(bs);
    free(line_);
    return rv;
  }

  XfstCompiler&
  XfstCompiler::setRestrictedMode(bool value)
  {
    restricted_mode_ = value;
    return *this;
  }

  bool
  XfstCompiler::getRestrictedMode() const
  {
    return restricted_mode_;
  }

  XfstCompiler&
  XfstCompiler::setInspectNetSupported(bool value)
  {
    inspect_net_supported_ = value;
    return *this;
  }

  bool
  XfstCompiler::getInspectNetSupported() const
  {
    return inspect_net_supported_;
  }
  
  XfstCompiler&
  XfstCompiler::print_properties(std::ostream * oss)
    {
      ERROR(<< "missing print properties");
      return *this;
    }
  XfstCompiler&
  XfstCompiler::setReadline(bool readline)
  {
    use_readline_ = readline;
    return *this;
  }
  XfstCompiler&
  XfstCompiler::setReadInteractiveTextFromStdin(bool value)
  {
    read_interactive_text_from_stdin_ = value;
    return *this;
  }
  XfstCompiler&
  XfstCompiler::setOutputToConsole(bool value)
  {
    output_to_console_ = value;
    return *this;
  }
  bool
  XfstCompiler::getReadline()
  {
    return use_readline_;
  }
  bool
  XfstCompiler::getReadInteractiveTextFromStdin()
  {
    return read_interactive_text_from_stdin_;
  }
  bool
  XfstCompiler::getOutputToConsole()
  {
    return output_to_console_;
  }
  XfstCompiler&
  XfstCompiler::setVerbosity(bool verbosity)
    {
      verbose_ = verbosity;
      xre_.set_verbosity(verbosity);
      lexc_.setVerbosity(this->verbose_ ? 2 : 0);
      return *this;
    }
  XfstCompiler&
  XfstCompiler::setPromptVerbosity(bool verbosity)
  {
    verbose_prompt_ = verbosity;
    return *this;
  }
  
  // CONSOLE
  const XfstCompiler&
  XfstCompiler::prompt()
    {
      if (verbose_prompt_ && verbose_)
        {
          OUTPUT_LINE(<< "hfst[" << stack_.size() << "]: ");
        }
      return *this;
    }

  char*
  XfstCompiler::get_prompt() const
  {
    char p[256];
    sprintf(p, "hfst[" SIZE_T_SPECIFIER "]: ", stack_.size());
    return strdup(p);
  }
  
  XfstCompiler&
  XfstCompiler::print_transducer_info()
    {
      if (verbose_ && !stack_.empty())
        {
          HfstTransducer* top = stack_.top();
          if (top->get_type() != format_)
            {
              return *this;
            }
          OUTPUT(<< "? bytes. " << top->number_of_states() << " states, " << top->number_of_arcs() << " arcs, ? paths");
          std::map<std::string,std::string>::const_iterator it = variables_.find("print-sigma");
          if (it != variables_.end() && it->second == "ON")
            {
              (const_cast<XfstCompiler*>(this))->print_sigma(output_, false /* no prompt*/);
            }
        }
      return *this;
    }

// silly globls
XfstCompiler* xfst_ = 0;

}}

#else
#include <cstdlib>
#include <cassert>

using namespace hfst;
using namespace hfst::xfst;

int
main(int argc, char** argv)
  {
    std::cout << "Unit tests for " __FILE__ ":";
    std::cout << std::endl << "constructors:";
    std::cout << " (default)";
    XfstCompiler defaultXfst();
#if HAVE_SFST
    std::cout << " (SFST)";
    XfstCompiler sfstXfst(SFST_TYPE);
#endif
#if HAVE_OPENFST
    std::cout << " (OpenFst)";
    XfstCompiler ofstXfst(TROPICAL_OPENFST_TYPE);
#endif
#if HAVE_SFST
    std::cout << " (foma)";
    XfstCompiler fomaXfst(FOMA_TYPE);
#endif
    // FIXME: test as implemented.
    return EXIT_SUCCESS;
  }
#endif
