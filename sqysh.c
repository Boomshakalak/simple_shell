
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
int end = 0 ; 
/*
  Linklist to hold the backgroud process.
 */
typedef struct proc
{
  int pid;
  char cmd[64];
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
  	fprintf(stderr, "sqysh: too many arguments to \"cd\"\n");
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
    char * finalargs[128];
    int i;
    int position = 0;
    for (i = 0 ; args[i]!= NULL ; ++i){
      if (strcmp(args[i],">") == 0){
        close(fileno(stdout));
        if ((open(args[++i], O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1)
          perror("unable to allocate an outputfile!\n");
      }
      else if (strcmp(args[i],"<") == 0){
        close(fileno(stdin));
        if ((open(args[++i], O_RDONLY, 0666)) == -1)perror("unable to read from an inputfile!\n");
      }
      else
        finalargs[position++] = args[i];
    }
      finalargs[position] = NULL;
    if (execvp(finalargs[0], finalargs) == -1) {
      perror("sqysh");
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

#define sqysh_RL_BUFSIZE 1024
/**
   @brief Read a line of input from stdin.
   @return The line from stdin.
 */
char *sqysh_read_line(void)
{
  int bufsize = sqysh_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "sqysh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    c = getchar();

    if (c == EOF || c == '\n') {
      buffer[position] = '\0';
      if (c == EOF) end = 1;
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    // If we have exceeded the buffer, reallocate.
    if (position >= bufsize) {
      bufsize += sqysh_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "sqysh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
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
void sqysh_loop(int argc, char **argv)
{
  char *line;
  char **args;
  int status;
  int fd;
  process * header;
  process ** head = &header;
	if(argc < 2 && isatty(fileno(stdin))){
  		do {
		        printf("sqysh$ ");
		        checkBackground(head);
			      line = sqysh_read_line();
			      args = sqysh_split_line(line);
   		 	    status = sqysh_exec(args,head);
   		 	    free(line);
  	 	 	    free(args);
  		} while (status);
	}else
  {
      if ((fd = open(*(++argv), O_RDONLY, 0666)) == -1){
        sqysh_exec(++argv,NULL);
      }
      else 
        {
                dup2(fd,fileno(stdin));
                do {
                    checkBackground(head);
                    line = sqysh_read_line();
                    args = sqysh_split_line(line);
                    status = sqysh_exec(args,head);
                    free(line);
                    free(args);
              } while (end!=1);}
  }

}

/**
   @brief Main entry point.
   @param argc Argument count.
   @param argv Argument vector.
   @return status code
 */
int main(int argc, char **argv)
{

  sqysh_loop(argc,argv);
  return EXIT_SUCCESS;
}
