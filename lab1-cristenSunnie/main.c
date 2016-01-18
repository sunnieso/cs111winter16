/* CS111 Winter 2016 Lab1a
See README for further information
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
// [command] Check if a file descriptor is valid
int validFd(int fd, int fd_array_cur){
	if( fd >= fd_array_cur){	
  		fprintf(stderr, "Error: Invalid use of file descriptor %d before initiation.\n", fd);
  		return 0;
  	}
  	return 1;
}

// [read write] Check if the open system call had an error
int checkOpenError(int fd) {
	if (fd == -1) {
		fprintf(stderr, "Error: open returned unsuccessfully\n");
    return -1;
  }
  return 0;
}

// [command] Checks for --command arguments
int passChecks(char* str, int index, int num_args) {
  int i = 0;
  // checks if is a digit
  while (str != NULL && *(str+i) != '\0') {
    if (!isdigit(*(str+i))) {
      fprintf(stderr, "Error: Incorrect usage of --command. Requires integer argument.\n");
      return 0;
    }
    i++;
  }
  // checks if is within number of arguments
  if (index >= num_args) {
    fprintf(stderr, "Error: Invalid number of arguments for --command\n");
    return 0;
  }
  return 1;
}


//[command]
// Stores all arguments for a command in args_array, until the next "--" flag
// Doesn't include optarg which is accepted automatically.
// Keeps the current index updated and returns the updated argv index (optind)
int findArgs(char*** args_array, size_t args_array_size, 
              int index, int* args_current,
              int argc, char** argv) {
  
  int args_array_cur = *args_current;
  //store arguments of the command into an array of char**
  while(index < argc){
    //break the loop if the index reaches the next "--"option
    if(argv[index][0] == '-' && argv[index][1] == '-') {
      break;
    }
      
    //now this must be an argument for the command. Store it into args array
    //realloc: same mechanics as fd_array
    if(args_array_cur == args_array_size){
      args_array_size *= 2;
      *args_array = (char**)realloc(*args_array, args_array_size*sizeof(char*)); 
    }
    (*args_array)[args_array_cur] = argv[index];
    args_array_cur++;
    index++;
  }
  *args_current = args_array_cur;
  return index;
}


struct wait_output_chain{
  pid_t childPid;
  int begin;
  int end;
};

int main(int argc, char **argv) {
  // c holds return value of getopt_long
  int c;

  // j is an iterator for for loops
  int j;

  // status for child process
  int status;
  // will be updated to 1 if any calls to open fail
  int exit_status = 0;

  // Declare array to hold file descriptors
  size_t fd_array_size = 2;
  int fd_array_cur = 0;
  int * fd_array = malloc(fd_array_size*sizeof(int));
  // Declare a fd_isPipe.   
  int * fd_isPipe = malloc(fd_array_size*sizeof(int));/*fd_isPipe[fd] is 1 if fd is read end pipe descripter. 
                                                        if fd is the write end of the pipe and has not been used, 
                                                        fd_isPipe[fd] stores the pid of the process that uses it.
                                                        if the process that uses fd is terminated, fd_isPipe[fd] 
                                                        will be set to 0.  
                                                        fd_isPipe[fd] is also 0 for regular file descripters.*/

  // open flag
  int oflag = 0;
  char oflag_str[1000];
  strcpy(oflag_str, "");

  // Verbose can be on or off, automatically set to off
  int verbose = 0;

  //wait 
  size_t wait_info_size = 2;
  int wait_info_cur = 0;
  struct wait_output_chain* wait_info = malloc(wait_info_size * sizeof *wait_info);

  // storing pid.
  size_t pid_array_size = 2;
  int pid_array_cur = 0;
  pid_t* pid_array = malloc(pid_array_size*sizeof(pid_t));
  // Parse options
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
// SUBCOMMAND
    	  {"command",     required_argument,  0,  'c' },
        {"wait",        no_argument,        0,  'z' },
// FILE FLAGS
        {"append",      no_argument,        0,  6 },
        {"cloexec",     no_argument,        0,  7 },
        {"creat",       no_argument,        0,  8 },
        {"directory",   no_argument,        0,  9 },
        {"dsync",       no_argument,        0,  10 },
        {"excl",        no_argument,        0,  11 },
        {"nofollow",    no_argument,        0,  12 },
        {"nonblock",    no_argument,        0,  13 },
        {"rsync",       no_argument,        0,  14 },
        {"sync",        no_argument,        0,  15 },
        {"trunc",       no_argument,        0,  16 },
// FILE OPENING
        {"rdonly",      required_argument,  0,  17 },
        {"wronly",      required_argument,  0,  18 },
        {"rdwr",        required_argument,  0,  19 },
        {"pipe",        no_argument,        0,  20 },
 
// MISCELLANEOUS  
        {"close",      required_argument,  0,  28 },
        {"verbose",     no_argument,        0,  21 },
        {"profile",     no_argument,        0,  22 },
        {"abort",       no_argument,        0,  23 },
        {"catch",       required_argument,  0,  24 },
        {"ignore",      required_argument,  0,  25 },
        {"default",     required_argument,  0,  26 },
        {"pause",       no_argument,        0,  27 }
        
    };

    // get the next option
    c = getopt_long(argc, argv, "", long_options, &option_index);
   
    // break when there are no further options to parse
    if (c == -1)
      break;

    // args_array will store flag and its arguments
    size_t args_array_size = 2; 
    char** args_array = malloc(args_array_size*sizeof(char*)); //command argument(s)
    int args_array_cur = 0;    //current index for the above array  

    switch (c) {
    
//command
    case 'c': { // command (format: --command i o e cmd args_array)
      int i, o, e; // stdin, stdout, stderr

      //store the file descripter numbers and check for errors
      if (!passChecks(optarg, optind, argc)) { break; }
      i = atoi(optarg);
      
      if (!passChecks(argv[optind], optind, argc)) { break; }
      o = atoi(argv[optind]); optind++;
      
      if (!passChecks(argv[optind], optind, argc)) { break; }
      e = atoi(argv[optind]); optind++;

      // check if there is the proper number of arguments
      if (optind >= argc) {
        fprintf(stderr, "Error: Invalid number of arguments for --command\n");
        break;
      }
      //for --wait. Record the location of the command substring in argv for --wait to output later
      int cmd_begin = optind;
      int cmd_end;

      // save command into args array
      args_array[0] = argv[optind]; optind++;
      args_array_cur++;

      // find arguments for command
      optind = findArgs(&args_array, args_array_size, optind, &args_array_cur,
                        argc, argv);
      cmd_end = optind;

      if(args_array_cur == args_array_size){
          args_array_size*=2;
          args_array = (char**)realloc((void*)args_array, args_array_size*sizeof(char*)); 
      }
      //append NULL to args_array (necessary for execvp())
      args_array[args_array_cur] = NULL;
      args_array_cur++;

      // print if verbose
      if (verbose == 1) {
        printf("--command %d %d %d ", i,o,e);
        for (j = 0; j < args_array_cur-1; j++) {
          printf("%s ", args_array[j]);
        }
        printf("\n");
      }

      //check if i,o,e fd are valid 
      if(!(validFd(i,fd_array_cur) && validFd(o,fd_array_cur) && validFd(e,fd_array_cur)))  
        continue;

      args_array[args_array_cur] = NULL;
      args_array_cur++;

      
      // execute command      
      pid_t pid = fork();
      if(pid == 0){   //child process
      
        // Before executing the command, check that if need to wait for other process to write 
        // into this input file descripter.

        // if 'i' is a read end pipe fd, and write end of the pipe has not been used, then wait
        // on the command that uses the write end of the pipe.

       
        /* StackOverflow:If you use dup() or dup2() to duplicate one end of a pipe 
           to standard input or standard output, you need to close() both ends of 
           the original pipe.
         */
        


        //redirect stdin to i, stdout to o, stderr to e
        dup2(fd_array[i], 0);
        dup2(fd_array[o], 1);
        dup2(fd_array[e], 2);
        if(fd_isPipe[i] != 0)
        //   // printf("%s.\tClose read  i,\t closing fd = %d: %d\n", args_array[0], i, close(fd_array[i]));
          // printf("%s.\tClose write i,\t closing fd = %d: %d\n", args_array[0], i+1,close(fd_array[i+1]));
          close(fd_array[i+1]);
        // }
        if(fd_isPipe[o] != 0)
        //   printf("%s.\tClose read  o,\t closing fd = %d: %d\n", args_array[0], o-1, close(fd_array[o-1]));
        // printf("%s.\tClose write o,\t closing fd = %d: %d\n", args_array[0], o, close(fd_array[o]));
          close(fd_array[o-1]);
        // }
        // printf("%s,\tclose %d: %d\n", args_array[0], i, close(fd_array[i]));
        // printf("%s,\tclose %d: %d\n", args_array[0], o, close(fd_array[o]));
        // printf("%s,\tclose %d: %d\n", args_array[0], e, close(fd_array[e]));
        close(fd_array[i]);
        close(fd_array[o]);
        close(fd_array[e]);

        execvp(args_array[0], args_array);
        //return to main program if execvp fails
        fprintf(stderr, "Error: Unknown command '%s'\n", args_array[0]);
        exit(255);  
      }
      printf("Parent process: child proces pid = %d, cmd = %s\n", pid, args_array[0]);
      // if 'o' is a write end pipe fd, then set fd_isPipe[o] to the pid
      if(fd_isPipe[o] != 0){
        fd_isPipe[o] = (int) pid;
        close(fd_array[o]);
      }
      if(fd_isPipe[i] != 0){
        fd_isPipe[i];
        close(fd_array[i]);
      }


      // prepare to store the child pid to pid_array
      if(pid_array_cur == pid_array_size){
          pid_array_size*=2;
          pid_array = realloc(pid_array, pid_array_size*sizeof(pid_t)); 
      }
      pid_array[pid_array_cur++] = pid;
      // printf("Waiting for process: %s\n", args_array[0]);
      // while(pid != waitpid(pid, &status, WNOHANG) && !WIFEXITED(status)){
      //   printf("Child process %d WIFEXITED=%d\n", pid, WIFEXITED(status));
      // }
      
      // printf("Resuming main()...\n");

      //record command in wait_output_chain for --wait
      if(wait_info_cur == wait_info_size){
          wait_info_size *=2;
          wait_info = realloc((void*)wait_info, wait_info_size*sizeof *wait_info); 
      }
      wait_info[wait_info_cur].childPid = pid;
      wait_info[wait_info_cur].begin = cmd_begin;
      wait_info[wait_info_cur++].end = cmd_end;
      
      break;
    }
//wait
    case 'z': {
      if (verbose){
        printf("--wait\n");
      }
      //wait any child process to finish. 0 is for blocking.
      pid_t returnedPid;
      int j1;
      for(j1 = 0; j1 < pid_array_cur; j1++){
        // printf("waitpid is waiting...\n");m              
        printf("pid_array[%d] = %d\n", j1, pid_array[j1]);
        returnedPid = waitpid(pid_array[j1], &status, WNOHANG);
        printf("pid_array_cur = %d, j = %d, WNOHANG: pid = %d\n", pid_array_cur, j1, returnedPid);
        if (!WIFEXITED(status)){
          returnedPid = waitpid(pid_array[j1], &status, 0);
        }

        // printf("waitpid returns %d\n", returnedPid);
        // returnedPid = waitpid(WAIT_ANY, &status, WNOHANG);// > 0 || WIFEXITED(status)){
        // printf("returnPid = %d\n", returnedPid);
        
        if(returnedPid == 0){ printf("returnedPid == 0\n"); continue;}
        if(returnedPid == -1){ continue;}

       // //WEXITSTATUS returns the exit status of the child.
        int waitStatus = WEXITSTATUS(status);
        printf("%d ", waitStatus);

        //the main program needs to return the with biggest value of exit status.
        if (waitStatus > exit_status) {
          exit_status = waitStatus;
        }
        for(j = 0 ; j != wait_info_cur; j++){
          if(returnedPid == (wait_info[j]).childPid)
            break;
        }
        int j1= j;
        for (j = wait_info[j1].begin; j != wait_info[j1].end; j++) {
          printf("%s ", argv[j]);
        }
        printf("\n");
      }
      break;
    }
     
//append
    case 6:
      oflag |= O_APPEND;  strcat(oflag_str,"--append ");
    break;
//cloexev
    case 7:
      oflag |= O_CLOEXEC;  strcat(oflag_str,"--cloexev ");
    break;
//creat
    case 8:
      oflag |= O_CREAT;  strcat(oflag_str,"--creat ");
    break;
//directory
    case 9:
      oflag |= O_DIRECTORY;  strcat(oflag_str,"--directory ");
    break;
//dsync
    case 10:
      oflag |= O_DSYNC;  strcat(oflag_str,"--dsync ");
    break;
//excl
    case 11:
      oflag |= O_EXCL;  strcat(oflag_str,"--excl ");
    break;
//nofollow
    case 12:
      oflag |= O_NOFOLLOW;  strcat(oflag_str,"--nofollow ");
    break;
//nonblock
    case 13:
      oflag |= O_NONBLOCK;  strcat(oflag_str,"--nonblock ");
    break;
//rsync
    case 14:
      oflag |= O_RSYNC;  strcat(oflag_str,"--rsync ");
    break;
//sync
    case 15:
      oflag |= O_SYNC;  strcat(oflag_str,"--sync ");
    break;
//trunc
    case 16:
      oflag |= O_TRUNC;  strcat(oflag_str,"--trunc ");
    break;
//read only
    case 17: 
//write only
    case 18:
//read and write 
    case 19: 
      // assign oflag
      if (c == 17)      oflag = O_RDONLY | oflag;
      else if( c== 18)  oflag = O_WRONLY | oflag;
      else              oflag = O_RDWR | oflag;
      // find all arguments for the current flag
      optind = findArgs(&args_array, args_array_size, optind, &args_array_cur,
                        argc, argv);

      // print command if verbose is enabled
      if (verbose) {
        printf("%s", oflag_str);
        oflag_str[0] = '\0';
        char * flags;
        if (c == 17)    flags = "--rdonly";
        else if(c == 18)flags = "--wronly";
        else            flags = "--rdwr";

        printf("%s ", flags);
        printf("%s ", optarg);
        for (j = 0; j < args_array_cur; j++) {
          printf("%s ", args_array[j]);
        }
        printf("\n");
      }
      
      // open file into read write file descriptor
      int rw_fd = open(optarg, oflag, 777);
      if(checkOpenError(rw_fd) == -1) {
        exit_status = 1;
        continue;
      }
      
      // save file descriptor to array
      if (fd_array_cur >= fd_array_size) {
        fd_array_size *= 2;
        //printf("fd_array reallocs %zu\n", fd_array_size);
        fd_array = (int*)realloc(fd_array, fd_array_size*sizeof(int)); 
        fd_isPipe = (int*)realloc(fd_isPipe, fd_array_size*sizeof(int));
      }
      fd_array[fd_array_cur] = rw_fd;
      fd_isPipe[fd_array_cur] = 0;
      fd_array_cur++;

      //clean oflag content.
      oflag = 0;
      break;   
//pipe
    case 20:
      optind = findArgs(&args_array, args_array_size, optind, &args_array_cur,
                        argc, argv);

      if(verbose){
        printf("--pipe ");
        for(j = 0 ; j != args_array_cur; j++){
          printf("%s ", args_array[j]);
        }      
        printf("\n");
      }
      if(args_array_cur > 0 ){
        fprintf(stderr, "Error: --pipe does not take arguments.");
      }
      int pipefd[2]; //store indices of fd_array
      if(pipe(pipefd)== -1){
          perror("pipe()");
//        fprintf(stderr, "Error: fail to create pipe. pipe() returns -1.\n");
      }

      // save file descriptor to array
      if (fd_array_cur  + 1 >= fd_array_size) {
        fd_array_size *= 2;
      // printf("fd_array reallocs %zu\n", fd_array_size);
       // printf("fd_cur = %d\n", fd_array_cur);
        fd_array = (int*)realloc(fd_array, fd_array_size*sizeof(int)); 
        fd_isPipe = (int*)realloc(fd_isPipe, fd_array_size*sizeof(int));
      }
      fd_isPipe[fd_array_cur] = 1;
      fd_array[fd_array_cur++] = pipefd[0];
      fd_isPipe[fd_array_cur] = 1;
      fd_array[fd_array_cur++] = pipefd[1];

      break;

//close
    case 28:
      break;
//verbose
    case 21: 
      verbose = 1;
      break;
//profile
    case 22:

      break;
//abort
    case 23:
      if(verbose) printf("--abort\n");
      abort();
      break;
//catch
    case 24:
      break;
//ignore
    case 25:
      break;
//default
    case 26:
      break;
//pause    
    case 27:
      break;
    // case '?': // ? returns when doesn't recognize option character
      // break;

    default:
        fprintf(stderr, "Error: ?? getopt returned character code 0%o ??\n", c);
    }
    // Free arguments array for next command
   // if(args_array_cur != 0){
     // printf("freeing args_array at c = %d\n", c);
      // printf("freeing args %s\n", args_array[0]);
      free(args_array);
    // if(c != 'c'){
    // }
    // else{
    //   printf("NOT freeing args %s\n", args_array[0]);
    //   printf("keep wait_info[%d].cmd = %s\n", wait_info_cur - 1, (*(wait_info[wait_info_cur-1].cmd))[0]);
    // }

   // }
  }

  // Prints out extra options that weren't parsed
 // if (optind < argc) {
 //      printf("non-option ARGV-elements: ")
 //      while (optind < argc)
 //          printf("%s ", argv[optind++]);
 //      printf("\n");
 //  }

/*
  for(j = 0; j != fd_array_cur; j++)
    printf("fd_array[%d] = %d, addr = %p\n", j, fd_array[j],(void*)&(fd_array[j]));
  for(;j < fd_array_size; j++)
    printf("fd_array[%d] = NULL, addr = %p\n",j, (void*)&(fd_array[j]));
  printf("fd_size = %zu\n", fd_array_size);
*/
  // Close all used file descriptors
  fd_array_cur--;
  while (fd_array_cur >= 0) {
    if(fd_array[fd_array_cur] != -1)
    	close(fd_array[fd_array_cur]);
  	fd_array_cur--;
  }
  // Free file descriptor array
  // printf("freeing fd_array\n");
  free(fd_array);
  free(fd_isPipe);
  // for(j = 0 ; j < wait_info_cur; j++){
  //   printf("freeing %s\n",(*(wait_info[j].cmd))[0]);
  //   free(*(wait_info[j].cmd));
  // }
  // printf("freeing wait_info\n");
  free(wait_info);
  free(pid_array);
  // Exit with previously set status
  exit(exit_status);
}
