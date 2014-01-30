#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

int main(void){
  int i, status;
  pid_t childID, endID;
  time_t when;

  if((childID = fork()) == -1){
    perror("fork error\n");
    exit(EXIT_FAILURE);
  }else if(childID == 0){
    time(&when);
    printf("Child process started at %s", ctime(&when));
    sleep(10);
    exit(EXIT_SUCCESS);
  }else{
    time(&when);
    printf("Parent process started at %s", ctime(&when));
    for(i = 0; i < 15; i++){
      endID = waitpid(childID, &status, WNOHANG|WUNTRACED);
      if(endID == -1){
        perror("waitpid error");
	exit(EXIT_FAILURE);
      }else if(endID == 0){
        time(&when);
	printf("Parent waiting for child at %s", ctime(&when));
	sleep(1);
      }else if(endID == childID){
        if(WIFEXITED(status))
          printf("Child ended normally\n");
	else if(WIFSIGNALED(status))
          printf("Child ended because of an uncaught signal\n");
	else if(WIFSTOPPED(status))
	  printf("Child process has stopped\n");
	exit(EXIT_SUCCESS);
      }
    }
  }
}
