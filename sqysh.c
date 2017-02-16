
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
/*
  Function Declarations for builtin shell commands:
 */
int sqysh_cd(char **args);
int sqysh_exit(char **args);
int b_count = 0;
char str[128];
/*
  Linklist to hold the backgroud process.
 */
typedef struct proc
{
  int pid;
  char cmd[1024];
  struct proc * next;  
}process;

void addToBegin(process ** header, int pid, char* cmd){
  process* newProcess;
  newProcess = malloc(sizeof(process));
  newProcess -> pid = pid;
  strcpy(newProcess->cmd, cmd);
  newProcess -> next = *header;
  *header = newProcess;
  b_count++;
}

void removeProcess(process** head, int pid, int status){
  process * current = *head;
  process* temp = NULL;
  if (current->pid == pid){
    fprintf(stderr, "[%s (%d) completed with status %d]\n", (*head)->cmd, pid,status);
    temp = *head;
    *head = temp->next;
    free(temp);
    return;
  }
  while(current && current->next){
    if(current->next->pid == pid){
      fprintf(stderr, "[%s (%d) completed with status %d]\n", current->next->cmd, pid,status);
      temp = current -> next;
      current -> next = temp ->next;
      free(temp);
      break;
    }else{
      current = current -> next;
    }
  }



}

char *builtin_str[] = {
  "cd",
  "exit"
};
/*
  List of builtin commands, followed by their corresponding functions.
 */
int (*builtin_func[]) (char **) = {
  &sqysh_cd,
  &sqysh_exit
};

int sqysh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

/*
  Builtin function implementations.
*/
/**
   @brief iterate the linklist to find the status of background process.
   @param header of backgroud process list.
 */
int checkBackground(process ** head){
  int status;
  int pid;
  if(b_count != 0){

      while(( pid = waitpid(-1,&status,WNOHANG))>0){
               /*traverse the link list to find the specific pid print and remove*/
             removeProcess(head,pid,status);
             b_count--;
           }
  }
  return 0;      
}
/**
   @brief Bultin command: change directory.
   @param args List of args.  args[0] is "cd".  args[1] is the directory.
   @return Always returns 1, to continue executing.
 */
int sqysh_cd(char **args)
{
  int size;
  for (size = 0; args[size] != NULL; size++);
  if (size>2){
  	fprintf(stderr, "cd: too many arguments\n");
  	return 1;
  }
  else if (args[1] == NULL) {
    args[1] = getenv("HOME");
  } 
    if (chdir(args[1]) != 0) {
      fprintf(stderr, "cd: %s: %s\n", args[1], strerror(errno));
    }
  return 1;
}


/**
   @brief Builtin command: exit.
   @param args List of args.  Not examined.
   @return Always returns 0, to terminate execution.
 */
int sqysh_exit(char **args)
{
  return 0;
}

/**
  @brief Launch a program and wait for it to terminate.
  @param args Null terminated list of arguments (including program).
  @return Always returns 1, to continue execution.
 */
int sqysh_launch(char **args,process ** background_header)
{
  pid_t pid;
  int status;
  int b_flag = 0;
  int j;
  for (j = 0 ;args[j] != NULL; j++){
    if (strcmp(args[j],"&") == 0){
      b_flag = 1;
      args[j] = NULL;
      break;
    }
  }
  pid = fork();
  if (pid == 0) {
    // Child process
    char * finalargs[1024];
    finalargs[0] = NULL;
    int i;
    int position = 0;
    for (i = 0 ; args[i]!= NULL ; ++i){
      if (strcmp(args[i],">") == 0){
        if (finalargs[0] == NULL) exit(EXIT_FAILURE);
        close(fileno(stdout));
        if ((open(args[++i], O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1)
          perror("unable to allocate an outputfile!\n");
      }
      else if (strcmp(args[i],"<") == 0){
        if (finalargs[0] == NULL) exit(EXIT_FAILURE);
        close(fileno(stdin));
        if ((open(args[++i], O_RDONLY, 0666)) == -1)perror("unable to read from an inputfile!\n");
      }
      else
        finalargs[position++] = args[i];
    }
      finalargs[position] = NULL;
    if (execvp(finalargs[0], finalargs) == -1) {
      fprintf(stderr, "%s: %s\n", finalargs[0], strerror(errno));
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    // Error forking
    perror("sqysh");
  } else {
    	if (b_flag == 0 )
    		waitpid(pid,&status,WUNTRACED);
	else
	 addToBegin(background_header, pid, args[0]);

  }
  return 1;
}

/**
   @brief Execute shell built-in or launch program.
   @param args Null terminated list of arguments.
   @return 1 if the shell should continue running, 0 if it should terminate
 */
int sqysh_exec(char **args, process ** background_header)
{
  int i;

  if (args[0] == NULL) {
    return 1;
  }

  for (i = 0; i < sqysh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }

  }

  return sqysh_launch(args,background_header);
}


#define TOK_DELIM " \t\r\n\a"
/**
   @brief Split a line into tokens .
   @param line The line.
   @return Null-terminated array of tokens.
 */
char **sqysh_split_line(char *line)
{
  int bufsize = 64, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;
  if (!tokens) {
    fprintf(stderr, "sqysh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, TOK_DELIM);
  while (token != NULL) { 	
    	tokens[position] = token;
    	position++;
    if (position >= bufsize) {
      bufsize += 64;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "sqysh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;

}

/**
   @brief Loop getting input and executing it.
 */
void sqysh_loop()
{
  char **args;
  int status;
  process * header;
  process ** head = &header;
  		do {
		        if (isatty(0)) printf("sqysh$ ");
		        checkBackground(head);
			      if (fgets(str,1024,stdin)== NULL) exit(EXIT_FAILURE);
			      args = sqysh_split_line(str);
   		 	    status = sqysh_exec(args,head);
  	 	 	    free(args);
  		} while (status);
	// }else
       
 //                do {
 //                    checkBackground(head);
 //                    if (fgets(str,1024,stdin)== NULL) exit(EXIT_FAILURE);
 //                    args = sqysh_split_line(str);
 //                    status = sqysh_exec(args,head);
 //                    free(args);
 //              } while (status);
      
  }
   



/**
   @brief Main entry point.
   @param argc Argument count.
   @param argv Argument vector.
   @return status code
 */
int main(int argc, char ** argv)
{   
    int exec = 0;
    FILE* fp = NULL;
   if (argc == 2){ 
      if ((fp = fopen(argv[1],"r")) != NULL)
          dup2(fileno(fp),fileno(stdin));
        else
          exec = 1;
      } 
      if ((exec == 1) || argc >2 ) sqysh_exec(++argv,NULL);       
    sqysh_loop();
  if (fp!=NULL) fclose(fp);
  return 0;
}
