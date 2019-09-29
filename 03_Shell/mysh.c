// Copyright [2019] <Yidong Fang>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_COMMAND_LEN 520
#define MAX_PARSE_NUM 260
#define MAX_SIM_JOBS 40
#define WAIT_CMD "wait"
#define JOBS_CMD "jobs"
#define EXIT_CMD "exit"
#define PROPMT "mysh> "

static const char INVALID_ARG[] = "Usage: mysh [batchFile]\n";

static int jobid_next = 0;

typedef struct job_info_s {
  int jobid;
  int running;
  pid_t jobpid;
  char** argv;
  int argc;
} job_info;

static job_info* job_arr[MAX_SIM_JOBS];

void func_wait(int pid) {
  printf("wait invoked.\n");
  return;
}

void func_jobs() {
  char job_cmd[MAX_COMMAND_LEN];
  char* p_job_cmd;
  // print all background jobs
  // check all the process status
  pid_t wait_return;
  for (size_t i = 0; i < MAX_SIM_JOBS; i++) {
    if (job_arr[i] != NULL) {
      wait_return = waitpid(job_arr[i]->jobpid, NULL, WNOHANG);
      // 0 - running, -1 - error, pid - stopped (appear only once)
      // printf("%d return %d\n", job_arr[i]->jobid, wait_return);
      if (wait_return == job_arr[i]->jobpid || wait_return == -1) {
        for (size_t k = 0; k < job_arr[i]->argc; k++) {
          free(job_arr[i]->argv[k]);
        }
        free(job_arr[i]->argv);
        free(job_arr[i]);
        job_arr[i] = NULL;
      } else {
        // the job is still running
        // reset pointer
        p_job_cmd = job_cmd;
        for (size_t j = 0; j < job_arr[i]->argc; j++) {
          int spf_result =
              snprintf(p_job_cmd, sizeof(job_cmd) - (p_job_cmd - job_cmd),
                       "%s ", job_arr[i]->argv[j]);
          if (spf_result >= 0) {
            p_job_cmd += spf_result;
          } else {
            fprintf(stderr, "error in jobs command.\n");
            fflush(stderr);
            exit(1);
          }
        }
        // remove the last space
        job_cmd[strlen(job_cmd) - 1] = '\0';
        printf("%d : %s\n", job_arr[i]->jobid, job_cmd);
      }
    }
  }
  fflush(stdout);
}

void func_exit() {
  // exit the shell
  exit(0);
}

void job_arr_add(char* const* argv, int argc, pid_t job_pid) {
  // melloc space for new argv
  char** argv_new = (char**)malloc(sizeof(char**) * argc);
  char* arg_new;
  for (size_t i = 0; i < argc; i++) {
    arg_new = (char*)malloc(strlen(argv[i]) + 1);
    argv_new[i] = strncpy(arg_new, argv[i], strlen(argv[i]) + 1);
  }

  for (size_t i = 0; i < MAX_SIM_JOBS; i++) {
    if (job_arr[i] == NULL) {
      job_arr[i] = malloc(sizeof(job_info));
      job_arr[i]->jobid = jobid_next;
      job_arr[i]->argv = argv_new;
      job_arr[i]->argc = argc;
      job_arr[i]->running = 1;
      job_arr[i]->jobpid = job_pid;
      return;
    }
  }
  return;
}

int func_general_cmd(char* const* argv, int argc, int background) {
  /*
  background 1: run int the backgrond
  background 0: wait the child
  */
  pid_t child_pid, tpid;
  int child_status;

  child_pid = fork();
  if (child_pid == 0) {
    // child process
    execvp(argv[0], argv);
    fprintf(stderr, "%s: Command not found\n", argv[0]);
    fflush(stderr);
  } else {
    // parent process
    if (!background) {
      // wait the child process to end
      do {
        tpid = wait(&child_status);
      } while (tpid != child_pid);
    }
    // add child process into the data structure
    job_arr_add(argv, argc, child_pid);
    jobid_next++;
    return 0;
  }
}

int check_built_in(char** arg_arr, int num_arg) {
  /*
  0 - no built-in keyword
  1 - built-in keywrod activated
  */
  if (num_arg == 1) {
    if (strcmp(EXIT_CMD, arg_arr[0]) == 0) {
      func_exit();
      return 1;
    } else if (strcmp(JOBS_CMD, arg_arr[0]) == 0) {
      func_jobs();
      return 1;
    }
  } else if (num_arg == 2 && strcmp(WAIT_CMD, arg_arr[0]) == 0) {
    // wait command
    // check whether the 2nd argument is a number first
    return 1;
  }
  return 0;
}

int parse_command(char* command) {
  /*
  built-in shell command: exit, jobs, wait
  return 0: normal
  return 1: exit
  */
  char* arg_arr[MAX_PARSE_NUM];
  char* token = strtok_r(command, " ", &command);
  int num_parse = 0, bg_flag = 0;
  // store all arguments into the arg_arr
  while (token) {
    // this will include a new line char
    arg_arr[num_parse++] = token;
    token = strtok_r(NULL, " ", &command);
  }

  // remove newline char from the last token
  int len = strlen(arg_arr[num_parse - 1]);
  if (len > 1 && arg_arr[num_parse - 1][len - 1] == '\n') {
    arg_arr[num_parse - 1][len - 1] = '\0';
  } else if (arg_arr[num_parse - 1][0] == '\n') {
    // the last token is a newline
    num_parse--;
  }
  if (num_parse == 0) {
    // do nothing
    return 1;
  }

  // check bg flag
  if (strcmp("&", arg_arr[num_parse - 1]) == 0) {
    bg_flag = 1;
  }
  // char* argv_new[num_parse - bg_flag];
  // for (size_t i = 0; i < num_parse - bg_flag; i++) {
  //   argv_new[i] = arg_arr[i];
  // }
  arg_arr[num_parse - bg_flag] = NULL;

  // check keyword function
  if (!check_built_in(arg_arr, num_parse - bg_flag)) {
    func_general_cmd(arg_arr, num_parse - bg_flag, bg_flag);
  }

  return 0;
}

int main(int argc, char** argv) {
  FILE* batch_fd = NULL;
  int eof_indicator = 0;
  if (argc > 2) {
    write(STDERR_FILENO, INVALID_ARG, sizeof(INVALID_ARG));
    return 1;
  } else if (argc == 1) {
    // interactive mode
    while (1) {
      char command[MAX_COMMAND_LEN];
      write(STDERR_FILENO, PROPMT, sizeof(PROPMT));
      fgets(command, MAX_COMMAND_LEN, stdin);
      eof_indicator = feof(stdin);
      if (eof_indicator) {
        printf("\n");
        fflush(stdout);
        break;
      }
      parse_command(command);
    }

  } else {
    // batch mode
    batch_fd = fopen(argv[1], "r");
    if (batch_fd == NULL) {
      // failed to open batch file
      // const int msg_size = 30 + strlen(argv[1]);
      // char* err_msg = (char*)malloc(msg_size);
      // snprintf(err_msg, msg_size, "Error: Cannot open file %s\n", argv[1]);
      // write(STDERR_FILENO, err_msg, strlen(err_msg));
      // free(err_msg);
      fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
      fflush(stderr);
      return 1;
    }
  }

  if (batch_fd) {
    fclose(batch_fd);
  }

  return 0;
}
