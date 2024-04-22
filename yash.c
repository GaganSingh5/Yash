

// all the imports required by the program
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <math.h>

// defined macro to store the buffer size and delimiters.
#define INPUT_BUFFER_SIZE 1024
#define PROMPT_BUFFER_SIZE 1024
#define PROMPT_DELIMITERS " \t\n\r\a"

// these are the varibales used to track background processIds.
// and names to be used by & and fg operators/commands.
int bgProcessListPointer = -1;
long bgProcessIds[50];
char *bgProcessNames[50];
int isFgProcess = 0;

// these are the variables used to store the user input and the tokenized version of this input.
char *userPrompt;
char **userPromptList;

// variables to store original stdin and stdout.
int original_stdin;
int original_stdout;

// this the function I am using to log messages to console.
// it uses the error stream to console on terminal
// even if other streams are redirected.
void yash_logMessage(char *message)
{
  fprintf(stderr, "%s \n", message);
}

/**
 * @brief Function to display the custom shell prompt.
 *
 * This function prints a custom shell prompt with colored characters and an emoji.
 * The prompt consists of the letters 'yash' in different colors followed by a bug emoji and a '$' sign.
 *
 * @note The function uses ANSI escape codes for color formatting.
 */
void yash_prompt()
{
  // Set color for 'y' to red
  printf("\033[0;31m");
  printf("y");

  // Set color for 'a' to green
  printf("\033[0;32m");
  printf("a");

  // Set color for 's' to yellow
  printf("\033[0;33m");
  printf("s");

  // Set color for 'h' to purple
  printf("\033[0;35m");
  printf("h");

  // Set color for the bug emoji and the '$' sign to white
  printf("\033[0;37m");
  printf("\U0001F4DF");
  printf(" $ ");

  // Reset color to default
  printf("\033[0m");
}

// this is the function which is used to get input from the terminal
// using the fgets function.
void yash_readPrompt(char **userPrompt)
{
  // I am getting the input from terminal and storing it in userPrompt char pointer
  fgets(*userPrompt, sizeof(char) * INPUT_BUFFER_SIZE, stdin);

  // here i am checking if the length of provided input is greater
  // than zero and replace the last newLine character by null character.
  long lengthOfPrompt = strlen(*userPrompt);
  if (lengthOfPrompt > 0 && (*userPrompt)[lengthOfPrompt - 1] == '\n')
  {
    (*userPrompt)[lengthOfPrompt - 1] = '\0';
  }
}

// this is the function which is used to process user input
// basically it converts the prompt string to tokens seperated by provided delimiter
// using strtok function.
void yash_processUserPrompt(char **prompt, char ***list)
{
  char *token;
  long position = 0;

  // get tokens based on delimiters.
  token = strtok(*prompt, PROMPT_DELIMITERS);

  // for each token store that in the provided list
  while (token != NULL)
  {
    (*list)[position] = token;
    position++;

    // here I am getting the next token
    token = strtok(NULL, PROMPT_DELIMITERS);
  }

  // NULL terminate the list to ensure end of list.
  (*list)[position] = NULL;
}

// this is the function which is used to execute a particular linux command
// it creates the args vector based on parameters passed and execute the command
// using execvp under a child process so the parent process does not terminate.
int yash_executeCommand(char *command, char **cmdArgs, int cmdArgsCount)
{

  // create the args vector of size arguments length + 1
  // extra length is for the NULL character required by execvp.
  char *argsVector[cmdArgsCount + 1];

  // iterate over the command arguments and store it in argsVector.
  for (int args = 0; args < cmdArgsCount; args++)
  {
    argsVector[args] = cmdArgs[args];
  }

  // add the NULL character at the end
  argsVector[cmdArgsCount] = NULL;

  // create a child using fork
  int child = fork();

  // check if there is some issue while creating the child
  if (child == -1)
  {
    yash_logMessage("Error while creating child to execute a command.");
    return -1;
  }

  // now in the child process execute the command using execvp
  if (child == 0)
  {
    // checking if the execvp failed and printing error message.
    if (execvp(command, argsVector) == -1)
    {
      yash_logMessage("Error while executing the command: Invalid command or arguments.");
      return -1;
    }
  }

  int status;

  // parent waiting for child to terminate after execution of command.
  waitpid(child, &status, 0);

  // returning zero if everything is successfull
  return 0;
}

// this is the function used to open a new session of the terminal.
// this uses x-terminal-emulator command to execute the shell file.
// internally its calling yash_executeCommand function
void yash_openNewSession()
{
  char *args[] = {"x-terminal-emulator", "-e", "./yash"};
  yash_executeCommand(args[0], args, 3);
}

// this is the function used to execute the command in background
// when & operator is used basically is creates a new child for the
// command with a different session and the parent is not waiting for it.
// also I am storing its pid for future use by other commands.
int yash_execute_in_bg(char *command, char **cmdArgs, int cmdArgsCount)
{
  // create the args vector of size arguments length + 1
  // extra length is for the NULL character required by execvp.
  char *argsVector[cmdArgsCount + 1];

  // iterate over the command arguments and store it in argsVector.
  for (int args = 0; args < cmdArgsCount; args++)
  {
    argsVector[args] = cmdArgs[args];
  }

  // add the NULL character at the end
  argsVector[cmdArgsCount] = NULL;

  // creating the child using fork
  int child = fork();
  if (child < 0)
  {
    yash_logMessage("Error creating child for bg process.");
    return -1;
  }

  // in the child process create a new session using setsid and
  //  then call execute with the particular command
  if (child == 0)
  {
    if (setsid() == -1)
    {
      yash_logMessage("Error starting a session for new child process for bg command.");
      return -1;
    }
    execvp(command, argsVector);
  }

  // now add the pid of bg process in the global array.
  bgProcessListPointer++;
  bgProcessIds[bgProcessListPointer] = child;

  // below code prints the process id with the process name for the process
  // which will be running in background. also store that string in global variable for future use.
  char childID[50];
  sprintf(childID, "%d", child);

  char *processDetails = malloc(sizeof(char) * 8024);
  processDetails = strcpy(processDetails, "[");
  processDetails = strcat(processDetails, childID);
  processDetails = strcat(processDetails, "] ");

  for (int args = 0; args < cmdArgsCount; args++)
  {
    processDetails = strcat(processDetails, cmdArgs[args]);
  }

  bgProcessNames[bgProcessListPointer] = processDetails;

  yash_logMessage("Background Process:");
  yash_logMessage(processDetails);

  return 0;
}

// this is the function I am using to validate the commands and the special characters
// as per the rules defined in the assignment.
int yash_validate_prompt(char ***prompList)
{
  // defining variables to track the count of arguments and
  // special characters.
  int counter = 0;
  int startPointer = 0;
  int endPointer = 0;
  int hashCount = 0;
  int pipeCount = 0;
  int conditionalCount = 0;
  int sequentialCount = 0;
  // now to each token I am checking for each special character
  while ((*prompList)[counter] != NULL)
  {
    // if character is # I am increasing the hash counter and checking how
    // many arguments are there in the command before the # and if its greater then 5 or less than 1
    // I am printing error.
    if (strcmp((*prompList)[counter], "#") == 0)
    {
      hashCount++;
      endPointer = counter;
      int offset = endPointer - startPointer;
      if (offset > 5 || offset < 1)
      {
        yash_logMessage("Error: Command arguments cannot be greater then 5 or less than 1");
        return -1;
      }
      startPointer = counter + 1;
    }
    // similar to # in | also I am storing the number of pipes and then checking if the
    // arguments count is valid for command before |
    else if (strcmp((*prompList)[counter], "|") == 0)
    {

      pipeCount++;
      endPointer = counter;
      int offset = endPointer - startPointer;
      if (offset > 5 || offset < 1)
      {
        yash_logMessage("Error: Command arguments cannot be greater then 5 or less than 1");
        return -1;
      }
      startPointer = counter + 1;
    }
    // for the && and || also I am storing the count of operator and checking the
    // number of arguments in the command if they are valid or not.
    else if (strcmp((*prompList)[counter], "&&") == 0 ||
             strcmp((*prompList)[counter], "||") == 0)
    {
      conditionalCount++;
      endPointer = counter;
      int offset = endPointer - startPointer;
      if (offset > 5 || offset < 1)
      {
        yash_logMessage("Error: Command arguments cannot be greater then 5 or less than 1");
        return -1;
      }
      startPointer = counter + 1;
    }
    // for the >, >> and < also I am storing the count of operator and checking the
    // number of arguments in the command if they are valid or not.
    else if (strcmp((*prompList)[counter], ">") == 0 ||
             strcmp((*prompList)[counter], ">>") == 0 ||
             strcmp((*prompList)[counter], "<") == 0)
    {
      endPointer = counter;
      int offset = endPointer - startPointer;
      if (offset > 5 || offset < 1)
      {
        yash_logMessage("Error: Command arguments cannot be greater then 5 or less than 1");
        return -1;
      }
      startPointer = counter + 1;
    }
    // for the ; also I am storing the count of operator and checking the
    // number of arguments in the command if they are valid or not.
    else if (strcmp((*prompList)[counter], ";") == 0)
    {
      sequentialCount++;
      endPointer = counter;
      int offset = endPointer - startPointer;
      if (offset > 5 || offset < 1)
      {
        yash_logMessage("Error: Command arguments cannot be greater then 5 or less than 1");
        return -1;
      }
      startPointer = counter + 1;
    }
    counter++;
  }

  // now if the token is a null character
  // means its the last command or the only command in prompt
  // In this also i am checking the number of arguments if they are valid or not.
  if ((*prompList)[counter] == NULL)
  {
    endPointer = counter;
    int offset = endPointer - startPointer;
    if (offset > 5 || offset < 1)
    {
      yash_logMessage("Error: Command arguments cannot be greater then 5 or less than 1");
      return -1;
    }
  }

  // now in this I am checking if the number of special characters are
  // as per the requirements given in the assignment and printing error
  // message if they are not aligned.
  if (hashCount > 5)
  {
    yash_logMessage("Error: Max 5 contactenation operations are allowed");
    return -1;
  }

  if (pipeCount > 6)
  {
    yash_logMessage("Error: Max 6 piping operations are allowed");
    return -1;
  }

  if (conditionalCount > 5)
  {
    yash_logMessage("Error: Max 5 conditional operations are allowed");
    return -1;
  }

  if (sequentialCount > 4)
  {
    yash_logMessage("Error: Max 5 sequential operations are allowed");
    return -1;
  }

  return 0;
}

// this is the function used to do the cleanup task by freeing memory and closing fds
void yash_cleanUp()
{

  close(original_stdin);
  close(original_stdout);

  free(userPrompt);
  free(userPromptList);
}

// this is the function which execute the shell loop
// it stays in loop for the time the shell is active
// it prints the prompt and wait for the user input
// and then act accordingly based on the input.
void yash_loop()
{

  // here the shell loop starts
  do
  {

    // I am using fflush to fush all the streams before taking prompts.
    fflush(NULL);

    // init the variables to store input and tokens.
    userPrompt = malloc(sizeof(char) * INPUT_BUFFER_SIZE);
    userPromptList = malloc(sizeof(char) * PROMPT_BUFFER_SIZE);

    // this is the pipe variables used to redirect the
    // input and output streams based on special characters
    // further I am also storing the stdin and stdout stream so I can reset it back
    // to take input from or show output on terminal.
    int pipeFD[2], prev_pipeFD = STDIN_FILENO;
    original_stdin = dup(STDIN_FILENO);
    original_stdout = dup(STDOUT_FILENO);

    // this is the function used to write a shell specific prompt
    // which tells the user that shell is asking for the prompt.
    yash_prompt();

    // this function is used to get the prompt from the using from at the stdin stream.
    yash_readPrompt(&userPrompt);

    // if the length is equal to zero just continue the loop skipping all the processing
    // again asking for a valid input
    if (strlen(userPrompt) == 0)
    {
      yash_cleanUp();
      continue;
    }

    // this funtion converts the user prompt into set of token splitted by space
    // further these tokens are stored in the userPromptList passed.
    yash_processUserPrompt(&userPrompt, &userPromptList);

    // now after getting the input and converting then to tokens
    // these tokens are passed to a validation function which checks if the
    // commands and special character are as per the requirements given in the assignment
    // regarding length of arguments and the count of special characters.
    int isValid = yash_validate_prompt(&userPromptList);

    // if there is some issue with the prompt -1 will be returned from above function
    // an the further execution is terminated and the loop is continue to get correct input
    // from user
    if (isValid == -1)
    {
      yash_cleanUp();
      continue;
    }

    // now if the prompt is newt which is for opening new terminal session
    // it execute the specific function to open new terminal and continue the
    // loop to get next prompt.
    if (strcmp(userPromptList[0], "newt") == 0)
    {
      yash_openNewSession();
      yash_cleanUp();
      continue;
    }

    // if the prompt is fg then the shell will start waiting
    // for the latest bg process.
    // if there is no bg process it will print error message.
    if (strcmp(userPromptList[0], "fg") == 0)
    {
      if (bgProcessListPointer == -1)
      {
        yash_logMessage("No Background Process Exist.");
      }
      else
      {
        isFgProcess = 1;
        int status;
        yash_logMessage("Foreground Process: ");
        yash_logMessage(bgProcessNames[bgProcessListPointer]);

        // wait for lastest process executing in background.
        waitpid(bgProcessIds[bgProcessListPointer], &status, 0);

        bgProcessIds[bgProcessListPointer] = -1;
        bgProcessListPointer--;
        isFgProcess = 0;
      }

      yash_cleanUp();
      continue;
    }

    // these are the variables used to keep track of the command's
    // start, end and count while iterating over each token of input
    long commandStartPointer = 0;
    long commandEndPointer = 0;
    long commandsCounter = 0;

    long iterator = 0;
    int EOA = -1;

    // this is used to track which was the lastSpecialCharacter which was executed.
    char *lastSpecialCharacter = "";

    // this is used to track the count of hash for processing the logic
    int lastHashReplacement = -1;

    // these are the variables used by the conditional operators.
    int isLastEndValid = 0;
    int isLastOrValid = -1;

    // now here I am iterating in while for each token got from user prompt
    while (EOA == -1)
    {

      // as the token list is NULL terminated if NULL is encountered while execution
      // then I am just updating the end pointer and count and exiting from the loop.
      // last command is executed outside the loop.
      if (userPromptList[iterator] == NULL)
      {
        commandEndPointer = iterator - 1;
        commandsCounter++;
        EOA = 1;
        continue;
      }
      // if a special character # is encountered then I am using the cat to output
      // content of each file seperated by #. As cat works the same as per the requirement
      // in the assignment.
      else if (strcmp(userPromptList[iterator], "#") == 0)
      {
        // here i am storing the last executed character.
        lastSpecialCharacter = "#";

        // if its the first command I am adding cat command in the start followed by file names.
        if (commandsCounter == 0)
        {
          userPromptList[iterator] = userPromptList[iterator - 1];
          userPromptList[iterator - 1] = "cat";
        }
        else
        {
          // here i am appending other file names to create the cat command structure
          if (lastHashReplacement == -1)
          {
            userPromptList[iterator] = userPromptList[iterator + 1];
            lastHashReplacement = iterator;
          }
          else
          {
            userPromptList[lastHashReplacement + 1] = userPromptList[iterator + 1];
            lastHashReplacement++;
          }
        }
        commandsCounter++;
      }
      // if pipe is encountered then I am using pipe() system call to connect the
      // the input and output of different commands.
      else if (strcmp(userPromptList[iterator], "|") == 0)
      {
        // saving the character.
        lastSpecialCharacter = "|";

        // getting the current command's tokens before the pipe in command vector variable.
        commandEndPointer = iterator;
        int argsCount = commandEndPointer - commandStartPointer;
        char *commandVector[argsCount];

        for (int iterator1 = 0; iterator1 < argsCount; iterator1++)
        {
          commandVector[iterator1] = userPromptList[iterator1 + commandStartPointer];
        }

        // calling the pipe to get read and write file descriptor.
        pipe(pipeFD);

        // if there are more than 1 commands before this pipe
        // then I am redirecting the stdin of current command to previous command's
        // write end of the pipe.
        if (commandsCounter > 0)
        {
          dup2(prev_pipeFD, STDIN_FILENO);
          close(prev_pipeFD);
        }

        // stdout to the write end of current command's pipe
        dup2(pipeFD[1], STDOUT_FILENO);
        close(pipeFD[1]);

        // then executing the command
        yash_executeCommand(commandVector[0], commandVector, argsCount);

        // and saving the read end as prev pipe for further use be other commands.
        prev_pipeFD = pipeFD[0];

        // at last I am upating the pointers which keeps track of the commands
        // in token list.
        commandStartPointer = iterator + 1;
        commandsCounter++;
      }
      // if the operator is > then it is redirecting the output of left hand side
      // of operator to file mentioned in right hand side. In this if the file does not exist
      // it will be created and the content will be replaced by the output of command.
      else if (strcmp(userPromptList[iterator], ">") == 0)
      {

        lastSpecialCharacter = ">";
        // here I am opening/creating the file
        int fileDescriptor = open(userPromptList[iterator + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        // redirecing the standard ouput to the file so what
        // ever will be output of command it will be stored in file
        dup2(fileDescriptor, STDOUT_FILENO);
        close(fileDescriptor);

        // again getting the current command's tokens before the operator in command vector variable.
        commandEndPointer = iterator;
        int argsCount = commandEndPointer - commandStartPointer;
        char *commandVector[argsCount];

        for (int iterator1 = 0; iterator1 < argsCount; iterator1++)
        {
          commandVector[iterator1] = userPromptList[iterator1 + commandStartPointer];
        }
        // at last executing the command
        yash_executeCommand(commandVector[0], commandVector, argsCount);
      }
      // if the operator is >> then the output of the command is redirect to a file
      // but instead of overwriting, data is appended in the file for each execution of command.
      else if (strcmp(userPromptList[iterator], ">>") == 0)
      {
        lastSpecialCharacter = ">>";
        // here I am opening/creating the file and
        // redirecing the standard ouput to the file so what
        // ever will be output of command it will be stored in file
        int fileDescriptor = open(userPromptList[iterator + 1], O_WRONLY | O_CREAT | O_APPEND, 0666);
        dup2(fileDescriptor, STDOUT_FILENO);
        close(fileDescriptor);

        // again getting the current command's tokens before the operator in command vector variable.
        commandEndPointer = iterator;
        int argsCount = commandEndPointer - commandStartPointer;
        char *commandVector[argsCount];

        for (int iterator1 = 0; iterator1 < argsCount; iterator1++)
        {
          commandVector[iterator1] = userPromptList[iterator1 + commandStartPointer];
        }

        // executing the command.
        yash_executeCommand(commandVector[0], commandVector, argsCount);
      }
      // if the operator is < then the input of the command is redirected from a file.
      else if (strcmp(userPromptList[iterator], "<") == 0)
      {
        // again I am storing the operator in last execution.
        // opening the file which should already existed.
        lastSpecialCharacter = "<";
        int fileDescriptor = open(userPromptList[iterator + 1], O_RDONLY, 0666);

        // if the file does not exist I am printing the error message.
        if (fileDescriptor < 0)
        {
          yash_logMessage("Error: There was some error opening the file. Check if the file exist.");
        }
        // otherwise I am redirecting the standard input to point to the file.
        // to get content of the file
        else
        {
          dup2(fileDescriptor, STDIN_FILENO);
          close(fileDescriptor);

          // further I am getting all the tokens related to command and executing it
          commandEndPointer = iterator;
          int argsCount = commandEndPointer - commandStartPointer;
          char *commandVector[argsCount];

          for (int iterator1 = 0; iterator1 < argsCount; iterator1++)
          {
            commandVector[iterator1] = userPromptList[iterator1 + commandStartPointer];
          }

          yash_executeCommand(commandVector[0], commandVector, argsCount);
        }
      }
      // if the operator is & this is to run a command in background.
      else if (strcmp(userPromptList[iterator], "&") == 0)
      {
        // first I am storing the operator and then getting the command from list
        lastSpecialCharacter = "&";
        commandEndPointer = iterator;
        int argsCount = commandEndPointer - commandStartPointer;
        char *commandVector[argsCount];

        for (int iterator1 = 0; iterator1 < argsCount; iterator1++)
        {
          commandVector[iterator1] = userPromptList[iterator1 + commandStartPointer];
        }

        // then i am calling execute bg function which is used to send process to background
        // in this shell will have control over the terminal and can execute commands while other
        // process is running in the background.
        yash_execute_in_bg(commandVector[0], commandVector, argsCount);
      }
      // if the operation is ; then all the commands are executed sequencially.
      else if (strcmp(userPromptList[iterator], ";") == 0)
      {
        // first I am storing the operator and then getting the command from list
        lastSpecialCharacter = ";";
        commandEndPointer = iterator;
        int argsCount = commandEndPointer - commandStartPointer;
        char *commandVector[argsCount];

        for (int iterator1 = 0; iterator1 < argsCount; iterator1++)
        {
          commandVector[iterator1] = userPromptList[iterator1 + commandStartPointer];
        }

        // then executing the command before the special charcter.
        yash_executeCommand(commandVector[0], commandVector, argsCount);

        // then updating the pointers to point to next command in the prompt
        commandStartPointer = iterator + 1;
        commandsCounter++;
      }
      // this is the conditional operator && which works like if any of the command fails it will stop
      // execution at that point. But it will keep on executing commands from left to right
      // if there is no issue.
      else if (strcmp(userPromptList[iterator], "&&") == 0)
      {
        // first I am storing the operator and then getting the command from list
        lastSpecialCharacter = "&&";
        commandEndPointer = iterator;
        int argsCount = commandEndPointer - commandStartPointer;
        char *commandVector[argsCount];

        for (int iterator1 = 0; iterator1 < argsCount; iterator1++)
        {
          commandVector[iterator1] = userPromptList[iterator1 + commandStartPointer];
        }

        // now I am executing the command getting its status.
        int status = yash_executeCommand(commandVector[0], commandVector, argsCount);
        commandStartPointer = iterator + 1;
        commandsCounter++;

        // if there is some issue in execution then I am setting the validStatus and then breaking from
        // loop, which is processing the tokens.
        if (status == -1)
        {
          isLastEndValid = -1;
          break;
        }
      }
      // this is the conditional operator || it will stop if at the point
      // where the command executed successfully but keep on executing commands from
      // left to right if commands are failing.
      else if (strcmp(userPromptList[iterator], "||") == 0)
      {
        // first I am storing the operator and then getting the command from list
        lastSpecialCharacter = "||";
        commandEndPointer = iterator;
        int argsCount = commandEndPointer - commandStartPointer;
        char *commandVector[argsCount];

        for (int iterator1 = 0; iterator1 < argsCount; iterator1++)
        {
          commandVector[iterator1] = userPromptList[iterator1 + commandStartPointer];
        }

        // then i executed the command if the commad was successful i
        // am setting the valid status and breaking from loop but if it failed
        // I am moving to next command
        int status = yash_executeCommand(commandVector[0], commandVector, argsCount);
        commandStartPointer = iterator + 1;
        commandsCounter++;

        if (status == 0)
        {
          isLastOrValid = 0;
          break;
        }
      }

      iterator++;
    }

    // this is a cleanup code if the operators were conditional operator.
    // In this I am resting the stdin and stdout and freeing space used for  user prompts.
    if ((strcmp(lastSpecialCharacter, "&&") == 0 && isLastEndValid == -1) ||
        (strcmp(lastSpecialCharacter, "||") == 0 && isLastOrValid == 0))
    {
      yash_cleanUp();
      continue;
    }

    // this is the conditions for the special characters redirection and background process
    // where I am reseting the stdin and stdout and freeing the memory
    if (strcmp(lastSpecialCharacter, ">") == 0 ||
        strcmp(lastSpecialCharacter, ">>") == 0 ||
        strcmp(lastSpecialCharacter, "<") == 0 ||
        strcmp(lastSpecialCharacter, "&") == 0)
    {
      if (dup2(original_stdout, STDOUT_FILENO) == -1)
      {
        yash_logMessage("Error while reseting stdout");
        exit(EXIT_FAILURE);
      }

      if (dup2(original_stdin, STDIN_FILENO) == -1)
      {
        yash_logMessage("Error while reseting stdin");
        exit(EXIT_FAILURE);
      }

      yash_cleanUp();
      continue;
    }

    // this is the code to execute the last command given in prompt it could be last
    // if there are multiple commands seperated by special characters or it could be the only commnd
    // given in prompt.
    commandEndPointer = iterator;
    int argsCount = commandEndPointer - commandStartPointer;
    char *commandVector[argsCount];

    for (int iterator1 = 0; iterator1 < argsCount; iterator1++)
    {
      commandVector[iterator1] = userPromptList[iterator1 + commandStartPointer];
    }

    // for the pipe I have to take the input from the last command
    // so I am redirecting the stdin to previous pipe.
    if (strcmp(lastSpecialCharacter, "|") == 0)
    {
      dup2(prev_pipeFD, STDIN_FILENO);
      close(prev_pipeFD);
    }

    // for hash special character I am setting the position to null where the file list ends
    if (strcmp(lastSpecialCharacter, "#") == 0 && lastHashReplacement != -1)
    {
      commandVector[lastHashReplacement + 1] = NULL;
    }

    // at last we want to see the output on terminal so
    // I am redirecting the stdout to origin stdout which is the terminal.
    if (dup2(original_stdout, STDOUT_FILENO) == -1)
    {
      yash_logMessage("Error while reseting stdout");
      exit(EXIT_FAILURE);
    }

    // here i am executing the command.
    yash_executeCommand(commandVector[0], commandVector, argsCount);

    // further I am updating the pointers.
    commandStartPointer = iterator + 1;
    commandsCounter++;

    // at last I am reseting the stdin and stdout clearing the memory
    // and closing any unused file descriptors.
    if (dup2(original_stdin, STDIN_FILENO) == -1)
    {
      yash_logMessage("Error while reseting stdin");
      exit(EXIT_FAILURE);
    }

    yash_cleanUp();

  } while (1);
}

// this is the handler for sigint signal it is used to prevent terminal from exiting
// further it is also being used by the kill the latest bg process bring to foreground using fg
void handleCtrlC()
{
  if (isFgProcess == 1)
  {
    kill(bgProcessIds[bgProcessListPointer], SIGINT);
    yash_logMessage("");

    return;
  }
  yash_logMessage("");
  yash_prompt();
  fflush(stdout);
}

// this is the main driver function of the shell
// it starts the shell loop
// and set signals
int main(int argc, char const *argv[])
{
  // setting the signal handler
  signal(SIGINT, handleCtrlC);

  // starting the shell loop.
  yash_loop();
  return 0;
}
