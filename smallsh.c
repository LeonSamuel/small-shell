/*
* Student: Leon Samuel
* Date: 1/27/2022
*
* Description:
*     Program functions as a typical sh command line handler.
*     Supports running programs in foreground and background.
*     User can enter foreground only mode by pressing ^C^Z.
*/

#include <stdio.h>			// printf and perror
#include <stdlib.h>			// exit
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>			// execv, getpid, fork
#include <fcntl.h>
#include <sys/wait.h>			// waitpid
#include <signal.h>

char userInput[2048];			// stdin input
int backgroundChildExitMethod;		// child process exit method, 0 success, 1 fail
int exitStatus = 0;			// most recent non-built in exit status, 0 success, 1 fail
const char* backgroundArray[25];	// tracks background processes to output their PID once completed after users next input but before returning cmd
int backgroundArrayIndex = 0;		// for looping through background arrays
int foregroundMode = 1;			// All cmds will be treated at foreground cmds is set to 0 
#define EXIT "exit"			// command to exit program due to ^Z signal handler being disabled
#define MAX 256				// max file size


// The head of the linked list
struct child* head = NULL;
// The tail of the linked list
struct child* tail = NULL;
char exitMessage[MAX] = "";
char backgroundExitMessage[MAX] = "";


//  Struct for user input

struct input
{
	char* command;
	char* arg[512];
	char* inputFile;
	char* outputFile;
	int* background;
	//struct input* next;
};



/* 
*  Parse the current line which is space delimited and create a
*  input struct with the data from the user input.
*  Could use array, but this makes things more readable in future
*/
struct input* createInput(char* userInput)
{
	struct input* currInput = malloc(sizeof(struct input));
	int argIndex = 0;

	// For use with strtok_r
	char* saveptr;

	// The first token is the command
	char* token = strtok_r(userInput, " ", &saveptr);
	//printf("token: %s", token);
	currInput->command = calloc(strlen(token) + 1, sizeof(char));
	strcpy(currInput->command, token);

	currInput->background = 1;

	//if next token is null, it's a single command
	token = strtok_r(NULL, " ", &saveptr);
	if (token == NULL) {
		currInput->arg[argIndex] = currInput->command;
		return currInput;
	}
	
	currInput->arg[argIndex] = currInput->command; //adds cmd to arg line for using exevp

	while ( (strcmp(token, ">") != 0) && (strcmp(token, "<") != 0) && (token != NULL)) {
		
		//checks for ending &s and correctly handles midline &s
		if (strcmp(token, "&") == 0) {
			token = strtok_r(NULL, " ", &saveptr);
			if (token == NULL) {
				currInput->background = 0;
				return currInput;
			}
			else {
				currInput->arg[argIndex + 1] = "&";
				argIndex++;
				currInput->arg[argIndex + 1] = token;
				continue;
			}
		}	
		currInput->arg[argIndex+1] = token;
		token = strtok_r(NULL, " ", &saveptr);
		argIndex++;
		if (token == NULL) {
		}
		if (token == NULL) {
			return currInput;
		}
	}


	//printf("Milestone: 2.0\n");
	//strcpy(arg[argIndex], NULL);
	//printf("Milestone: 2.5\n");
	//printf("Arg Array:%s\n", args);

	if (token == NULL) {
		return currInput;
	}

	// Stores optional input file to draw data from
	if (strcmp(token, "<") == 0) {
		token = strtok_r(NULL, " ", &saveptr);
		currInput->inputFile = calloc(strlen(token) + 1, sizeof(char));
		strcpy(currInput->inputFile, token);
		token = strtok_r(NULL, " ", &saveptr);
	}
	if (token == NULL) {
		return currInput;
	}
	// Stores optional input file to draw data from
	if (strcmp(token, ">") == 0) {
		token = strtok_r(NULL, " ", &saveptr);
		currInput->outputFile = calloc(strlen(token) + 1, sizeof(char));
		strcpy(currInput->outputFile, token);
		token = strtok_r(NULL, " ", &saveptr);
	} 
	if (token == NULL) {
		return currInput;
	}
	// Stores optional output file to draw data from
	if (strcmp(token, "<") == 0) { // Input redirection can appear before or after output redirection.
		token = strtok_r(NULL, " ", &saveptr);
		currInput->inputFile = calloc(strlen(token) + 1, sizeof(char));
		strcpy(currInput->inputFile, token);
		token = strtok_r(NULL, " ", &saveptr);
	}
	if (token == NULL) {
		return currInput;
	}
	// Confirms if program will be executed in background
	if (strcmp(token, "&") == 0) {
		token = strtok_r(NULL, " ", &saveptr);
		if (token == NULL) {
			currInput->background = 0;
			return currInput;
		}
	}
	//printf("Current token: %s\n", token);
	// Set the next node to NULL in the newly created input entry
	//currInput->next = NULL;
	//printf("Milestone: Returning to main\n");
	return currInput;
}



//  Runs check on backgroundarray PIDS before returning command
//  to user. This check will print out termination method or exit value
//  to terminal for any background children that have finished running

void* wnoCheck() {
	int i;
	for (i = 0; i < backgroundArrayIndex; i++) {
		if (backgroundArray[i] != NULL && backgroundArray[i] != 0) {
			//checks the background PID in array against waitpid() which will return the PID if the process has finished
			//printf("testing this PID with wnohang: %i\n", backgroundArray[i]);
			if (backgroundArray[i] == waitpid(backgroundArray[i], &backgroundChildExitMethod, WNOHANG)) { // != 0
				if (backgroundChildExitMethod != 0) {
					printf("background pid %i is done: terminated by signal %i\n", backgroundArray[i], backgroundChildExitMethod);
				}
				else {
					printf("background pid %i is done: exit value %i\n", backgroundArray[i], backgroundChildExitMethod);
				}
				backgroundArray[i] = '\0'; // set to null if PID is found so we don't test it during next iteration
			}
		}
	}
	return 0;
}


// Expand $$ to be replaced with current parent PID

char* expandInput(char* userInput) {

	int curIndex = 0;
	char pid[2048];
	char expandedInput[2048] = "";
		
	sprintf(pid, "%i", getpid());

	while (userInput[curIndex] != NULL) {
		if (userInput[curIndex] == '$' && userInput[curIndex + 1] == '$'){
			strcat(expandedInput, pid);
			curIndex = curIndex + 2;
		}
		else {
			char* temp = userInput[curIndex];
			sprintf(expandedInput, "%s%c", expandedInput, userInput[curIndex]);
			curIndex = curIndex + 1;
		}
	}


	return expandedInput;
}


// Handles receiving SIGTSTP. Program will write reentrant message to terminal.
// All cmds will be treated at foreground cmds.

void handle_SIGTSTP(int signo) {
	// Enter foreground mode 
	if (foregroundMode == 1) {
		char* message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(1, message, 51); // write(STDOUT_FILENO, message, strlen(message)); 
		foregroundMode = 0;
	}


	// Exit foreground mode 
	else {
		char* message = "\nExiting foreground-only mode\n";
		write(1, message, 31);
		foregroundMode = 1;
	}
	fflush(stdout);
}



// Handles receiving input, looping, signal handlers,
//  most output, and swtiches.

int main()
{


	pid_t spawnpid = -5;		 
	int childStatus;			//return status number to main
	int childPID;				//returned pid from wait()
	//int* foregroundMode = 1;		//1 is background mode, 0 is foregroundMode

	//initiate signal catch for parent process
	struct sigaction SIGINT_action = {0};
	struct sigaction SIGTSTP_action = {0};


	// Redirect SIGTSTP (^Z) to handle_SIGTSTP()
	// Register handle_SIGINT as the signal handler
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	// Block all catchable signals while handle_SIGINT is running
	sigfillset(&SIGTSTP_action.sa_mask);
	// No flags set
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	
	// Redirect handling of SIG_IGN (^C) to ignore in parent
	// The ignore_action struct as SIG_IGN as its signal handler
	SIGINT_action.sa_handler = SIG_IGN;
	// Block all catchable signals while handle_SIGINT is running
	sigfillset(&SIGINT_action.sa_mask);
	// No flags set
	SIGINT_action.sa_flags = 0;
	// Register the ignore_action as the handler 
	sigaction(SIGINT, &SIGINT_action, NULL);
	


	// Continue to run program until user enters "Exit"
	while (strcmp(EXIT, userInput) != 0) {
		fflush(stdout);
		fflush(stdin);
		wnoCheck();	//returns to temrinal exit status of child proccesses that have completed
		printf(":");
		fflush(stdin);
		fgets(userInput, MAX, stdin);
		userInput[strcspn(userInput, "\n")] = 0; //removes /n at end of line 
		char* getcwd(char* buf, size_t size);
				
		//loop back to start if user enters comments or blank line
		if (userInput[0] == '#' || strcmp(userInput, " ") == 0 || strcmp(userInput, "") == 0) {
			memset(userInput, '\0', sizeof(userInput));
			continue;
		}

		// Handles exit command
		if (strcmp(EXIT, userInput) == 0) {
			break;
		}
		// Expand user input by replacing "$$" with current PID
		char* newTemp = expandInput(userInput);  ///might have to set as char or set with & sign
		strcpy(userInput, newTemp); //copying
		//free(newTemp);

		// Parse user input 
		struct input* parsedInput = createInput(userInput);
		//free(userInput);

		/*
		// Input verification 
		printf("User command entered: %s\n", parsedInput->command);
		printf("User first arg: %s\n", parsedInput->arg[0]);
		printf("User second arg: %s\n", parsedInput->arg[1]);
		printf("User third arg: %s\n", parsedInput->arg[2]);
		printf("User input file entered: %s\n", parsedInput->inputFile);
		printf("User output file entered: %s\n", parsedInput->outputFile);
		printf("User background file entered: %i\n", parsedInput->background);
		*/


		// Executes 3 commands exit, cd, and status via code built into this shell
		
		// Handles cd command 
		if (strcmp(parsedInput->command, "cd") == 0) {
			if (parsedInput->arg[1] == NULL) {
				chdir(getenv("HOME")); // The HOME variable contains the pathname of the user's login directory, we then chanage directory once it's found
				char cwd[MAX];
				getcwd(cwd, sizeof(cwd));
				continue;
			}
			else {
				char cwd[MAX];
				getcwd(cwd, sizeof(cwd));
				chdir(parsedInput->arg[1]); // should work for both relative and aboslute paths
				getcwd(cwd, sizeof(cwd));
				continue;
			}
		}
		// Handles status command 
		if (strcmp(parsedInput->command, "status") == 0) {
			if (strcmp(exitMessage, "") == 0) {
				printf("exit value %i\n", exitStatus);
			}
			else {
				printf("%s", exitMessage);
			}
			continue;
		}


		int childExitMethod;
		int fileDescriptor;
		int targetFD;
		pid_t childPID = fork();
		
	
		if (foregroundMode == 0 || parsedInput->background != 0) { 
			
			switch (childPID) {

			// Handle forked child creation error
			case -1: 
				perror("fork() failed\n");
				exit(1);
				break;

			// In child process
			case 0:
				
				// Reset handling of SIG_IGN (^C) to default registation handling in foreground child
				SIGINT_action.sa_handler = SIG_DFL;
				sigaction(SIGINT, &SIGINT_action, NULL);


				if (parsedInput->inputFile != NULL) {
					//printf("PARENT: Opening file %s.\n", parsedInput->inputFile);
					// Handle and directs input file
					int fileDescriptor = open(parsedInput->inputFile, O_RDONLY, 0444);
					if (fileDescriptor == -1) {
						printf("cannot open %s for input\n", parsedInput->inputFile);
						fflush(stdout);
						exit(1);
						break;
					}
					// input file redirection
					int result = dup2(fileDescriptor, 0);
					if (result == -1) {
						perror("source dup2()");
						exit(1);
						break;
					}
				}

				// Tests and opens output file descriptor
				if (parsedInput->outputFile != NULL) {
					int targetFD = open(parsedInput->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0222);
					if (targetFD == -1) {
						printf("cannot open %s for output\n", parsedInput->inputFile);
						fflush(stdout);
						break;
					}
					// Output file redirection
					int result = dup2(targetFD, 1);
					if (result == -1) {
						perror("output dup2()"); 
						fflush(stdout);
						exit(1);
						break;
					}
				}

				// Pass arguments to execvp
				execvp(parsedInput->command, parsedInput->arg);
				printf("%s: no such file or directory\n", parsedInput->command); //if execvp fails it will return to this line
				fflush(stdout);
				close(fileDescriptor);
				close(targetFD);
				exit(1); //exiting child program 
				break;

			default:
				// Wait for the child
				waitpid(childPID, &childExitMethod, 0); //wait for the indicated child PID in arg1 to finish, 
				if (WIFEXITED(childExitMethod)) { //Child exited normally with status of 0
					exitStatus = WEXITSTATUS(childExitMethod); //saves the exit code into the exitStatus variable 0:good  2:bad
					sprintf(exitMessage, "exit value %d\n", exitStatus); //, used EXIT to decipher output

					//printf(exitMessage);
					fflush(stdout);
					break;
				}
				else { // Child exited abnormally with status of not 0
					int exitStatus = WTERMSIG(childExitMethod); // store into exitStatus a int of likely 2 
					//sprintf(exitMessage, "termination of child %d with exit code of %d \n", childPID, exitStatus); // Child exited abnormally due to signal, used EXIT to decipher output
					sprintf(exitMessage, "terminated by signal %d\n", exitStatus); // Child exited abnormally due to signal, used EXIT to decipher output
					//printf(exitMessage);
					fflush(stdout);
					break;
				}
			}
		}
		else {
			fflush(stdout);
			switch (childPID) {

			// Handle forked child creation error
			case -1:
				perror("fork() failed\n");
				fflush(stdout);
				exit(1);
				break;

			// In child process
			case 0:

				// Handles obtaining input file descriptor
				if (parsedInput->inputFile != NULL) {
					int fileDescriptor = open(parsedInput->inputFile, O_RDONLY, 0444);
					if (fileDescriptor == -1) {
						printf("cannot open %s for input\n", parsedInput->inputFile);
						fflush(stdout);
						exit(1);
						break;
					}
					// Input file redirection handler
					int result = dup2(fileDescriptor, 0);
					if (result == -1) {
						perror("source dup2()"); 
						fflush(stdout);
						exit(1);
						break;
					}
				}
				// Handles when no input file is passed
				else {
					int fileDescriptor = open("/dev/null", O_RDONLY, 0444);
					if (fileDescriptor == -1) {
						perror("cannot open \"/dev/null\" for input\n");
						fflush(stdout);
						exit(1);
						break;
					}
					// Input file redirection handler - defualts to std
					int result = dup2(fileDescriptor, 0);
					if (result == -1) {
						perror("source dup2()");  
						fflush(stdout);
						exit(1);
						break;
					}
				}

				// Handles obtaining output file descriptor 
				if (parsedInput->outputFile != NULL) {
					int targetFD = open(parsedInput->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0222);
					if (targetFD == -1) {
						printf("cannot open %s for output\n", parsedInput->inputFile);
						fflush(stdout);
						break;
					}
					// Output file redirection handler - defualts to std
					int result = dup2(targetFD, 1);
					if (result == -1) {
						perror("output dup2()"); 
						fflush(stdout);
						exit(1);
						break;
					}
				}
				else {
					// Handles when no input file is passed
					int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0222);
					if (targetFD == -1) {
						perror("cannot open \"/dev/null\" for output\n");
						fflush(stdout);
						exit(1);
						break;
					}
					// Input file redirection handler - defualts to std
					int result = dup2(targetFD, 0);
					if (result == -1) {
						perror("output dup2()");  
						fflush(stdout);
						exit(1);
						break;
					}
				}
				//executes job in new process using exe
				execvp(parsedInput->command, parsedInput->arg);
				printf("%s: no such file or directory\n", parsedInput->command); //if execvp fails it will return to this line
				fflush(stdout);
				close(fileDescriptor);
				close(targetFD);
				exit(1); //exiting child program 

				// store childPID into background array which will be evaluated during each cmd input to see if process is finished
				default:
				printf("background pid is %i\n", childPID);
				backgroundArray[backgroundArrayIndex] = childPID;
				//printf("stored in background array: %i\n", backgroundArray[backgroundArrayIndex]);
				backgroundArrayIndex++; //move index for next background process to store
				childPID = waitpid(childPID, &backgroundChildExitMethod, WNOHANG);
				fflush(stdout);
				}
		}
	}

	//Terminate all lingering background children
	int i;
	for (i = 0; i < backgroundArrayIndex; i++) { 
		if (backgroundArray[i] != NULL && backgroundArray[i] != 0) {
			kill(backgroundArray[i], SIGKILL);
		}
	}
	return EXIT_SUCCESS;
}
