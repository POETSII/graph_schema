%{ /* -*- C++ -*- */
    # include <cerrno>
    # include <climits>
    # include <cstdlib>
    # include <string>
    # include "device_dsl_driver.hpp"
    # include "device_dsl_parser.tab.hpp"

    // Work around an incompatibility in flex (at least versions
    // 2.5.31 through 2.5.33): it generates code that does
    // not conform to C89.  See Debian bug 333231
    // <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.
    # undef yywrap
    # define yywrap() 1

    // The location of the current token.
    static yy::location loc;
%}
/*
Because there is no #include-like feature we don’t need yywrap, we don’t need unput either, and we parse an actual file, this is not an interactive session with the user. Finally, we enable scanner tracing.
*/

%s CEMBED
%s CEMBEDR

%option noyywrap nounput batch debug noinput
/*
Abbreviations allow for more readable rules.
*/

id    [a-zA-Z][a-zA-Z_0-9]*
int   [0-9]+
blank [ \t]

/*
The following paragraph suffices to track locations accurately. Each time yylex is invoked, the begin position is moved onto the end position. Then when a pattern is matched, its width is added to the end column. When matching ends of lines, the end cursor is adjusted, and each time blanks are matched, the begin cursor is moved onto the end cursor to effectively ignore the blanks preceding tokens. Comments would be treated equally.
*/

%{
  // Code run each time a pattern is matched.
  # define YY_USER_ACTION  loc.columns (yyleng);
%}
%%
%{
  // Code run each time yylex is called.
  loc.step ();
%}

{blank}+   loc.step ();
[\n]+      loc.lines (yyleng); loc.step ();


"{{"     { BEGIN(CEMBED); return yy::device_dsl_parser::make_CSTART(loc); }
<CEMBED>([^}]|([}][^}]))* { BEGIN(0); return yy::device_dsl_parser::make_CCODE(yytext,loc); }
"}}"     { return yy::device_dsl_parser::make_CEND(loc); }
"(("     { BEGIN(CEMBEDR); return yy::device_dsl_parser::make_CSTARTR(loc); }
<CEMBEDR>([^)]|([)][^)]))* { BEGIN(0); return yy::device_dsl_parser::make_CCODE(yytext,loc); }
"))"     { return yy::device_dsl_parser::make_CENDR(loc); }

"["      return yy::device_dsl_parser::make_LSQUARE(loc);
"]"      return yy::device_dsl_parser::make_RSQUARE(loc);
":"      return yy::device_dsl_parser::make_COLON(loc);
";"      return yy::device_dsl_parser::make_SEMICOLON(loc);
"/"      return yy::device_dsl_parser::make_FSLASH(loc);
"device"      return yy::device_dsl_parser::make_DEVICE(loc);
"graph"      return yy::device_dsl_parser::make_GRAPH(loc);
"input"      return yy::device_dsl_parser::make_INPUT(loc);
"output"      return yy::device_dsl_parser::make_OUTPUT(loc);
"property"      return yy::device_dsl_parser::make_PROPERTY(loc);
"state"      return yy::device_dsl_parser::make_STATE(loc);
"send"      return yy::device_dsl_parser::make_SEND(loc);
"on_recv"      return yy::device_dsl_parser::make_ON_RECV(loc);
"recv"      return yy::device_dsl_parser::make_RECV(loc);
"enable"      return yy::device_dsl_parser::make_ENABLE(loc);
"disable"      return yy::device_dsl_parser::make_DISABLE(loc);
"wait"      return yy::device_dsl_parser::make_WAIT(loc);
"int32_t"      return yy::device_dsl_parser::make_INT32_t(loc);
"float"      return yy::device_dsl_parser::make_FLOAT(loc);

{id}       { return yy::device_dsl_parser::make_ID(yytext, loc); }

[\r]        {}

.          { driver.error (loc, "invalid character"); }

<<EOF>>    { return yy::device_dsl_parser::make_END(loc); }
%%

/*
Finally, because the scanner-related driver’s member-functions depend on the scanner’s data, it is simpler to implement them in this file.
*/

void device_dsl_driver::scan_begin ()
{
  yy_flex_debug = trace_scanning;
  if (file.empty () || file == "-")
    yyin = stdin;
  else if (!(yyin = fopen (file.c_str (), "r")))
    {
      error ("cannot open " + file + ": " + strerror(errno));
      exit (EXIT_FAILURE);
    }
}

void device_dsl_driver::scan_end ()
{
  fclose (yyin);
}
