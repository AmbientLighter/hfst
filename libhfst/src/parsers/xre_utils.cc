/**
 * @file xre.cc
 *
 * @brief implements xre routines.
 */

#include <cassert>
#include <cstdlib>
#include <cstring>

#include "xre_utils.h"
#include "HfstTransducer.h"

using std::string;
using std::map;

extern char* xretext;
extern int xreparse();
extern int xrenerrs;

int
xreerror(const char *msg)
{
#ifndef NDEBUG
    fprintf(stderr, "*** xre parsing failed: %s\n", msg);
    if (strlen(hfst::xre::data) < 60)
    {
        fprintf(stderr, "***    parsing %s [near %s]\n", hfst::xre::data,
                xretext);
    }
    else
    {
        fprintf(stderr, "***    parsing %60s [near %s]...\n", 
                hfst::xre::data, xretext);
    }
#endif
    return 0;
}

namespace hfst 
{ 
namespace xre 
{

char* data;
std::map<std::string,hfst::HfstTransducer*> definitions;
char* startptr;
hfst::HfstTransducer* last_compiled;
hfst::ImplementationType format;
size_t len;

  bool expand_definitions=false;
  bool harmonize_=true;
  bool verbose_=false;
  FILE * warning_stream=NULL;

  std::string substitution_function_symbol;

void set_substitution_function_symbol(const std::string &symbol)
{
  substitution_function_symbol=symbol;
}

bool substitution_function(const hfst::StringPair &p, hfst::StringPairSet &sps)
{
  if (p.first == substitution_function_symbol ||
      p.second == substitution_function_symbol)
    {
      sps.insert(hfst::StringPair(substitution_function_symbol, 
                                  substitution_function_symbol));
      return true;
    }
  return false;
}


int*
get_n_to_k(const char* s)
{
    int* rv = static_cast<int*>(malloc(sizeof(int)*2));
    char *endptr;
    char *finalptr;
    if (*(s + 1) == '{')
      {
        rv[0] = strtol(s + 2, &endptr, 10);
        rv[1] = strtol(endptr + 1, &finalptr, 10);
        assert(*finalptr == '}');
      }
    else
      {
        rv[0] = strtol(s + 1, &endptr, 10);
        rv[1] = strtol(endptr + 1, &finalptr, 10);
        assert(*finalptr == '\0');
      }
    return rv;
}



int
getinput(char *buf, int maxlen)
{
    int retval = 0;
    if ( maxlen > len ) {
        maxlen = len;
    }
    memcpy(buf, data, maxlen);
    data += maxlen;
    len -= maxlen;
    retval = maxlen;
    return retval;
}

char*
strip_curly(const char *s)
{
    const char *c = s;
    char *stripped = (char*)calloc(sizeof(char),strlen(s)+1);
    int i = 0;
    while (*c != '\0')
    {
        // If first char is { or last one }, then skip it
        if ( ( *c == '{' && i == 0 ) || ( *c == '}' && *(c + 1) == '\0' ) )
                {
                if (*(c + 1) == '\0')
                        {
                                break;
                        }
                        else
                        {
                                stripped[i] = *(c + 1);
                                i++;
                                c += 2;
                        }
                }
        else
        {
                stripped[i] = *c;
            i++;
            c++;
        }

    }
    stripped[i] = '\0';
    return stripped;
}


char*
strip_percents(const char *s)
{
    const char *c = s;
    char *stripped = (char*)calloc(sizeof(char),strlen(s)+1);
    size_t i = 0;
    while (*c != '\0')
    {
        if (*c == '%')
        {
            if (*(c + 1) == '\0')
            {
                break;
            }
            else
            {
                stripped[i] = *(c + 1);
                i++;
                c += 2;
            }
        }
        else
        {
            stripped[i] = *c;
            i++;
            c++;
        }
    }
    stripped[i] = '\0';
    return stripped;
}

char*
add_percents(const char *s)
  {
    char* ns = static_cast<char*>(malloc(sizeof(char)*strlen(s)*2+1));
    char* p = ns;
    while (*s != '\0')
      {
        if ((*s == '@') || (*s == '-') || (*s == ' ') || (*s == '|') ||
            (*s == '!') || (*s == ':') || (*s == ';') || (*s == '0') ||
            (*s == '\\') || (*s == '&') || (*s == '?') || (*s == '$') ||
            (*s == '+') || (*s == '*') || (*s == '/') || (*s == '/') ||
            (*s == '_') || (*s == '(') || (*s == ')') || (*s == '{') ||
            (*s == '}') || (*s == '[') || (*s == ']'))
          {
            *p = '%';
            p++;
          }
        *p = *s;
        p++;
        s++;
      }
    *p = '\0';
    return ns;
  }

char *
get_quoted(const char *s)
{
    const char *qstart = strchr((char*) s, '"') + 1;
    const char *qend = strrchr((char*) s, '"');
    char* qpart = strdup(qstart);
    *(qpart+ (size_t) (qend - qstart)) = '\0';
    return qpart;
}

char*
parse_quoted(const char *s)
{
    char* quoted = get_quoted(s);
    char* rv = static_cast<char*>(malloc(sizeof(char)*strlen(quoted)));
    char* p = quoted;
    char* r = rv;
    while (*p != '\0')
      {
        if (*p != '\\')
          {
            *r = *p;
            r++;
            p++;
          }
        else if (*p == '\\')
          {
            switch (*(p + 1))
              {
              case '0':
              case '1':
              case '2':
              case '3':
              case '4':
              case '5':
              case '6':
              case '7':
                fprintf(stderr, "*** XRE unimplemented: "
                        "parse octal escape in %s", p);
                *r = '\0';
                p = p + 5;
                break;
              case 'a':
                *r = '\a';
                r++;
                p = p + 2;
                break;
              case 'b':
                *r = '\b';
                r++;
                p = p + 2;
                break;
              case 'f':
                *r = '\f';
                r++;
                p = p + 2;
                break;
              case 'n':
                *r = '\n';
                r++;
                p = p + 2;
                break;
              case 'r':
                *r = '\r';
                r++;
                p = p + 2;
                break;
              case 't':
                *r = '\t';
                r++;
                p = p + 2;
                break;
              case 'u':
                fprintf(stderr, "Unimplemented: parse unicode escapes in %s", p);
                *r = '\0';
                r++;
                p = p + 6;
                break;
              case 'v':
                *r = '\v';
                r++;
                p = p + 2;
                break;
              case 'x':
                  {
                    char* endp;
                    int i = strtol(p + 2, &endp, 10);
                    if ( 0 < i && i <= 127)
                      {
                        *r = static_cast<char>(i);
                      }
                    else
                      {
                        fprintf(stderr, "*** XRE unimplemented: "
                                "parse \\x%d\n", i);
                        *r = '\0';
                      }
                    r++;
                    assert(endp != p);
                    p = endp;
                   break;
                }
              case '\0':
                fprintf(stderr, "End of line after \\ escape\n");
                *r = '\0';
                r++;
                p++;
                break;
              default:
                *r = *(p + 1);
                r++;
                p += 2;
                break;
              }
          }
      }
    *r = '\0';
    free(quoted);
    return rv;
}

double
get_weight(const char *s)
{
    double rv = -3.1415;
    const char* weightstart = s;
    while ((*weightstart != '\0') && 
           ((*weightstart == ' ') || (*weightstart == '\t') ||
            (*weightstart == ';')))
    {
        weightstart++;
    }
    char* endp;
    rv = strtod(weightstart, &endp);
    assert(endp != weightstart);
    return rv;
}

HfstTransducer*
compile(const string& xre, map<string,HfstTransducer*>& defs,
        ImplementationType impl)
{
    // lock here?
    data = strdup(xre.c_str());
    startptr = data;
    len = strlen(data);
    definitions = defs;
    format = impl;
    xreparse();
    free(startptr);
    data = 0;
    len = 0;
    if (xrenerrs == 0)
      {
        HfstTransducer* rv = new HfstTransducer(*last_compiled);
        delete last_compiled;
        return rv;
      }
    else
      {
        return new HfstTransducer(impl);
      }
}

bool is_definition(const char* symbol)
{
  std::string symbol_(symbol);
  if (definitions.find(symbol_) == definitions.end())
    return false;
  return true;
}

HfstTransducer*
expand_definition(HfstTransducer* tr, const char* symbol)
{
  if (expand_definitions)
    {
      for (std::map<std::string,hfst::HfstTransducer*>::const_iterator it
             = definitions.begin(); it != definitions.end(); it++) 
        {
          if (strcmp(it->first.c_str(), symbol) == 0)
            {
              tr->substitute(hfst::StringPair(symbol,symbol), *(it->second));
              tr->remove_from_alphabet(symbol);
              break;
            }
        }
    }
  return tr;
}

static const char * get_print_format(const char * symbol)
{
  if (strcmp(hfst::internal_identity.c_str(), symbol) == 0)
    return "?";
  if (strcmp(hfst::internal_unknown.c_str(), symbol) == 0)
    return "?";
  if (strcmp(hfst::internal_epsilon.c_str(), symbol) == 0)
    return "0";
  return symbol;
}

HfstTransducer*
xfst_label_to_transducer(const char* input, const char* output)
{
  HfstTransducer * retval = NULL;

  // non-matching definitions
  if ( (is_definition(input) || is_definition(output)) && 
       strcmp(input, output) != 0 )
    {
      char msg[256];
      sprintf(msg, "invalid use of definitions in label %s:%s", 
              get_print_format(input), get_print_format(output));
      xreerror(msg);
    }
  if  (strcmp(input, hfst::internal_unknown.c_str()) == 0 && 
       strcmp(output, hfst::internal_unknown.c_str()) == 0)
    {
      retval = new HfstTransducer(hfst::internal_unknown, hfst::internal_unknown, hfst::xre::format);
      HfstTransducer id(hfst::internal_identity, hfst::internal_identity, hfst::xre::format);
      retval->disjunct(id).minimize();
    }
  else if (strcmp(input, hfst::internal_unknown.c_str()) == 0)
    {
      retval = new HfstTransducer(hfst::internal_unknown, output, hfst::xre::format);
      HfstTransducer output_tr(output, output, hfst::xre::format);
      retval->disjunct(output_tr).minimize();
    }
  else if (strcmp(output, hfst::internal_unknown.c_str()) == 0)
    {
      retval = new HfstTransducer(input, hfst::internal_unknown, hfst::xre::format);
      HfstTransducer input_tr(input, input, hfst::xre::format);
      retval->disjunct(input_tr).minimize();
    }
  else
    {
      retval = new HfstTransducer(input, output, hfst::xre::format);
    }

  if (is_definition(input))
    retval = expand_definition(retval, input);

  return retval;
}

void warn_about_special_symbols_in_replace(HfstTransducer * t)
{
  if (!verbose_)
    return;

  StringSet alphabet = t->get_alphabet();
  for (StringSet::const_iterator it = alphabet.begin(); 
       it != alphabet.end(); it++)
    {
      if (HfstTransducer::is_special_symbol(*it) && 
          *it != hfst::internal_epsilon &&
          *it != hfst::internal_unknown &&
          *it != hfst::internal_identity)
        {         
          fprintf(warning_stream, "warning: using special symbol '%s' in replace rule, "
                  "use substitute instead\n", it->c_str());
        }
    }
}

} }


