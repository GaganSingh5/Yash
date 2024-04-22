# YASH - Yet Another Shell
YASH is a simple custom shell implementation written in C. It provides basic shell functionalities and supports features like executing commands, handling special characters for redirection and sequencing, running commands in the background, and more.

## Features
- Command Execution: Execute commands entered by the user in the shell.
- Redirection: Redirect input and output using special characters like >, >>, and <.
- Piping: Connect the output of one command as input to another using the pipe (|) operator.
- Background Execution: Run commands in the background using the & operator.
- Special Characters Handling: Properly handle special characters such as #, |, ;, &&, and ||.
- Foreground Execution: Bring background processes to the foreground using the fg command.
- Error Handling: Provide informative error messages for invalid commands or operations.
