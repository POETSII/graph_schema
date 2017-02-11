#ifndef device_dsl_DRIVER_HH
# define device_dsl_DRIVER_HH

#include "device_dsl_ast.hpp"



# include "device_dsl_parser.tab.hpp"
/*
Then comes the declaration of the scanning function. Flex expects the signature of yylex to be defined in the macro YY_DECL, and the C++ parser expects it to be declared. We can factor both as follows.
*/

// Tell Flex the lexer's prototype ...
# define YY_DECL \
  yy::device_dsl_parser::symbol_type yylex (device_dsl_driver& driver)
// ... and declare it for the parser's sake.
YY_DECL;


/*
The device_dsl_driver class is then declared with its most obvious members.
*/

// Conducting the whole scanning and parsing of Calc++.
class device_dsl_driver
{
public:
  device_dsl_driver ();
  virtual ~device_dsl_driver ();

  GraphTypePtr graph;
  int result;
/*
To encapsulate the coordination with the Flex scanner, it is useful to have member functions to open and close the scanning phase.
*/

  // Handling the scanner.
  void scan_begin ();
  void scan_end ();
  bool trace_scanning;
/*
Similarly for the parser itself.
*/

  // Run the parser on file F.
  // Return 0 on success.
  int parse (const std::string& f);
  // The name of the file being parsed.
  // Used later to pass the file name to the location tracker.
  std::string file;
  // Whether parser traces should be generated.
  bool trace_parsing;
/*
To demonstrate pure handling of parse errors, instead of simply dumping them on the standard error output, we will pass them to the compiler driver using the following two member functions. Finally, we close the class declaration and CPP guard.
*/

  // Error handling.
  void error (const yy::location& l, const std::string& m);
  void error (const std::string& m);
};
#endif // ! device_dsl_DRIVER_HH
