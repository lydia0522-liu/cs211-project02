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


bool step_if(struct STMT*& cur, struct RAM* memory)
{
    // Evaluate the condition and choose the correct branch.
    struct EXPR* condition = cur->types.if_then_else->condition;
    struct RAM_VALUE* result = execute_expr(cur, memory, condition);

    if (result == nullptr){
        return false;
    }
    // Enter the true_path if true; otherwise false_path.
    if (result->types.i != 0){
        cur = cur->types.if_then_else->true_path;
    }else{
        cur = cur->types.if_then_else->false_path;
    }
    ram_free_value(result);
    return true;
}

bool step_while(struct STMT*& cur, struct RAM* memory)
{
    // Evaluate the condition and choose the correct branch.
    struct EXPR* condition = cur->types.while_loop->condition;
    struct RAM_VALUE* result = execute_expr(cur, memory, condition);
    if (result == nullptr){
        return false;
    }
    // Enter the loop body if true; otherwise exit the loop.
    if (result->types.i != 0){
        cur = cur->types.while_loop->loop_body;
    }else{
        cur = cur->types.while_loop->next_stmt;
    }
    ram_free_value(result);
    return true;
}

enum class StepStatus
{
    Advanced,
    BreakpointHit,
    Error,
    Completed
};

StepStatus step_command(struct STMT*& cur, struct RAM* memory, const std::set<int>& breakpoints, bool& pausedAtBreakpoint)
{
  if (cur == nullptr) {
    cout << "execution has completed" << endl;
    return StepStatus::Completed;
  }
  // Stop before executing a breakpoint; the next step resumes execution.
  if (breakpoints.count(cur->line) > 0 && !pausedAtBreakpoint) {
      cout << "breakpoint hit at line " << cur->line << endl;
      pausedAtBreakpoint = true;
      return StepStatus::BreakpointHit;
  }

  if (cur->stmt_type == STMT_IF_THEN_ELSE){
      bool success = step_if(cur, memory);
      if (!success){
          return StepStatus::Error;
      }
      pausedAtBreakpoint = false;
      if (cur == nullptr){
          cout << "program completed" << endl;
          return StepStatus::Completed;
      }
      return StepStatus::Advanced;
  }

  if (cur->stmt_type == STMT_WHILE_LOOP){
      bool success = step_while(cur, memory);
      if (!success){
          return StepStatus::Error;
      }
      pausedAtBreakpoint = false;
      if (cur == nullptr){
          cout << "program completed" << endl;
          return StepStatus::Completed;
      }
      return StepStatus::Advanced;
  }

  // Disconnect the next statement so execute() runs only one statement.
  struct STMT* next = get_next_stmt(cur);
  struct PG_LINKS links = break_pg(cur);

  // Always restore the graph, even if execution fails.
  struct ExecuteResult result = execute(cur, memory);
  repair_pg(cur, links);

  // Case 1: execution failed
  if (!result.Success)
  {
      return StepStatus::Error;
  }

  // Case 2: execution succeeded
  cur = next;
  pausedAtBreakpoint = false;// Allow breakpoints to be triggered again after advancing.

  // Case 3: program completed
  if (cur == nullptr)
  {
      cout << "program completed" << endl;
      return StepStatus::Completed;
  }

  // Case 4: successfully advanced
  return StepStatus::Advanced;
}

void run_command( struct STMT*& cur, struct RAM* memory, const std::set<int>& breakpoints, bool& pausedAtBreakpoint)
{
    if (cur == nullptr){
        cout << "execution has completed" << endl;
        return;
    }
    // Reuse step execution until a breakpoint, error, or completion.
    while (true){
        StepStatus result = step_command(
            cur, memory, breakpoints, pausedAtBreakpoint
        );
        if (result == StepStatus::BreakpointHit ||
            result == StepStatus::Error ||
            result == StepStatus::Completed) {
            return;
        }
    }
}

void print_graph_command(struct STMT* program)
{
    programgraph_print(program);
}

void show_memory_command(struct RAM* memory)
{
    ram_print(memory);
}

/**
 * @brief Sets a breakpoint at the given line.
 */
void set_breakpoint_command(std::set<int>& breakpoints, int line)
{
    if (breakpoints.count(line) > 0) {
        cout << "breakpoint exists" << endl;
    }
    else {
        breakpoints.insert(line);
        cout << "breakpoint set" << endl;
    }
}

/**
 * @brief Removes a breakpoint at the given line.
 */
void remove_breakpoint_command(std::set<int>& breakpoints, int line)
{
    if (breakpoints.erase(line) > 0) {
        cout << "breakpoint removed" << endl;
    }
    else {
        cout << "no such breakpoint" << endl;
    }
}

/**
 * @brief Removes all breakpoints.
 */
void clear_breakpoints_command(std::set<int>& breakpoints)
{
    breakpoints.clear();
    cout << "all breakpoints removed" << endl;
}

/**
 * @brief Lists all breakpoints in ascending order.
 */
void list_breakpoints_command(const std::set<int>& breakpoints)
{
    if (breakpoints.empty()) {
        cout << "breakpoints: none" << endl;
        return;
    }

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
  bool pausedAtBreakpoint = false;
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
        string line;
        getline(cin, line);
        step_command(cur, memory, breakpoints, pausedAtBreakpoint);
    }
    else if (cmd == "r") {
        string line;
        getline(cin, line);
        run_command(cur, memory, breakpoints, pausedAtBreakpoint);
    }
    else if (cmd == "pg") {
        print_graph_command(program);
    }
    else if (cmd == "sm") {
        show_memory_command(memory);
    }
    else if (cmd == "b") {
        int line;
        cin >> line;
        set_breakpoint_command(breakpoints, line);
    }
    else if (cmd == "rb") {
        int line;
        cin >> line;
        remove_breakpoint_command(breakpoints, line);
    }
    else if (cmd == "cb") {
        clear_breakpoints_command(breakpoints);
    }
    else if (cmd == "lb") {
        list_breakpoints_command(breakpoints);
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
