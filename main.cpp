/*main.cpp*/

/**
  * @brief simple debugger for nuPython programs.
  *
  * To run:
  *
  *   ./a.out filename.py
  *
  * @author original template by Prof. Joe Hummel
  * @author ?
  * @author ?
  * @author ?  
  * @note Northwestern University
  */

#include <iostream>
#include <string>
#include <set>

using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::getline;
using std::stoi;

#include "pgutil.h"

//
// Since we are working in C++, we have to tell the
// C++ compiler that the functions in the provided
// nuPython code are C functions:
//
extern "C" {
  #include "tokenqueue.h"
  #include "parser.h"
  #include "programgraph.h"
  #include "ram.h"
  #include "execute.h"
}


/**
  * @brief help command
  *
  * @return void
  */
void help_command()
{
  cout << "Available commands:" << endl;
  cout << "h -> help with list of commands" << endl;
  cout << "q -> quit the debugger" << endl;
  cout << "b n -> set breakpoint at line n" << endl;
  cout << "rb n -> remove breakpoint at line n" << endl;
  cout << "cb -> clear all breakpoints" << endl;
  cout << "lb -> list all breakpoints" << endl;
  cout << "pg -> print entire program graph" << endl;
  cout << "r -> run the program from the current stmt" << endl;
  cout << "s -> step by executing the current stmt" << endl;
  cout << "sm -> show memory contents" << endl;
  cout << "w -> what line will execute next?" << endl;
}

void where_command(struct STMT* cur)
{
    if (cur == nullptr)
    {
        cout << "execution has completed" << endl;
        return;
    }
    // break the graph 
    struct PG_LINKS links = break_pg(cur);
    programgraph_print(cur);
    repair_pg(cur, links);
}

struct STMT* get_next_stmt(struct STMT* cur)
{
  if (cur == nullptr) return nullptr;

  if (cur->stmt_type == STMT_ASSIGNMENT) {
    return cur->types.assignment->next_stmt;
  }
  else if (cur->stmt_type == STMT_FUNCTION_CALL) {
    return cur->types.function_call->next_stmt;
  }
  else if (cur->stmt_type == STMT_PASS) {
    return cur->types.pass->next_stmt;
  }

  return nullptr;
}


void step_command(struct STMT*& cur, struct RAM* memory)
{
  if (cur == nullptr) {
    cout << "execution has completed" << endl;
    return;
  }

  // if and while handled later
  if (cur->stmt_type == STMT_IF_THEN_ELSE ||
      cur->stmt_type == STMT_WHILE_LOOP) {
    return;
  }

  struct STMT* next = get_next_stmt(cur);
  struct PG_LINKS links = break_pg(cur);

  string line;
  getline(cin, line);

  struct ExecuteResult result = execute(cur, memory);
  repair_pg(cur, links);

  if (result.Success) {
    cur = next;
    if (cur == nullptr) {
      cout << "program completed" << endl;
    }
  }
}



/**
  * @brief main
  *
  * usage: ./a.out filename.py
  * 
  * If a filename is given, the file is opened and serves as
  * input to the debugger.
  *
  * @param argc # of arguments to program
  * @param argv Array of actual arguments
  * @return 0 Success
  */
int main(int argc, char* argv[])
{
  FILE* input = nullptr;

  cout << "** Simple gdb for nuPython **" << endl;

  if (argc < 2) {
    //
    // missing filename:
    //
    cout << "usage: ./a.out filename.py" << endl;
    return 0;
  }

  //
  // assume 2nd arg is a nuPython file, try to open:
  //
  char* filename = argv[1];

  input = fopen(filename, "r");

  if (input == nullptr) // unable to open:
  {
    cout << "**ERROR: unable to open input file "
         << "'" << filename << "' for input."
         << endl;

    return 0;
  }

  //
  // Build the program graph:
  //
  struct TokenQueue* tokens = parser_parse(input);
  if (tokens == nullptr) {
    fclose(input);
    return 0;
  }

  struct STMT* program = programgraph_build(tokens);
  if (program == nullptr) {
    tokenqueue_destroy(tokens);
    fclose(input);
    return 0;
  }

// current statement, starts at the first one
  struct STMT* cur = program;
  //
  // Initialize program memory:
  //
  struct RAM* memory = ram_init();

  //
  // command loop:
  //
  //
  // Store breakpoint line numbers:
  //
  std::set<int> breakpoints;
  string cmd;

  while (true) {

    cout << endl;
    cout << "Enter a command, h for help >" << endl;
    cin >> cmd;

    if (cmd == "q") {
      //
      // quit:
      //
      break;
    }
    else if (cmd == "h") {
      help_command();
    }
    else if (cmd == "w") {
      where_command(cur);
    }
    else if (cmd == "s") {
      step_command(cur, memory);
    }
    else if (cmd == "pg") {
      programgraph_print(program);
    }
    else if (cmd == "sm") {
      ram_print(memory);
    }
    else if (cmd == "b") {
      int line;
      cin >> line;

      if (breakpoints.count(line) > 0) {
        cout << "breakpoint exists" << endl;
      }
      else {
        breakpoints.insert(line);
        cout << "breakpoint set" << endl;
      }
    }
    else if (cmd == "rb") {
      int line;
      cin >> line;

      if (breakpoints.erase(line) > 0) {
        cout << "breakpoint removed" << endl;
      }
      else {
        cout << "no such breakpoint" << endl;
      }
    }
    else if (cmd == "cb") {
      breakpoints.clear();
      cout << "all breakpoints removed" << endl;
    }
    else if (cmd == "lb") {
      if (breakpoints.empty()) {
        cout << "breakpoints: none" << endl;
      }
      else {
        cout << "breakpoints: ";

        bool first = true;

        for (int line : breakpoints) {
          if (!first) {
            cout << ", ";
          }

          cout << line;
          first = false;
        }

        cout << endl;
      }
}
else {
  cout << "unknown command" << endl;
}
  }//while

  //
  // done: free data structures, close file, and return:
  //
  cout << endl;
  cout << "** Done **" << endl;

  ram_destroy(memory);
  programgraph_destroy(program);
  tokenqueue_destroy(tokens);

  fclose(input);

  return 0;
}
