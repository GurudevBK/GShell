/* Gurudev Ball-Khalsa
   CS360 Lab 8 - Jsh
   
   GuruShell (or jsh, whatever makes you happy)

   This shell is currently a rudimentary shell, capable of 
   file execution, directory traversal (cd), file redirection, 
   piping, and waiting using the '&' command after all commands.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#include <string.h>
#include "fields.h"
#include "jrb.h"

/* replaces /Users/$USER (mac)
   /home/$USER  (linux)
   with     ~

	BUGS:
	-> does not resolve homedir on some linux systems
 */
void get_resolved_homedir(char *wd) {
	int i, homelen, wdlen;
	char *home, *tmp;
	tmp  = (char*) calloc(strlen(wd), sizeof(char));

	/* get homedir pathname */
	if ((home = getenv("HOME")) == NULL)
		home = getpwuid(getuid())->pw_dir;
	
	/* we're gonna use these a lot */
	homelen = strlen(home);
	wdlen = strlen(wd);

	/* grab home directory string from working directory */
	if (wdlen >= homelen) {
		for (i = 0; i < homelen; i++) {
			tmp[i] = wd[i];
		}
		/* if we are in home or in one of home's child directories
		   update wd so that homedir is replaced by '~' */
		if (!strcmp(tmp, home)) {
			for(i = 1; i < wdlen-homelen + 1; i++) {
				tmp[i] = wd[homelen - 1 + i];
			}

			tmp[0] = '~';
	
			for (i = 0; i < wdlen-homelen+1; i++) {
				wd[i] = tmp[i];
			}
	
			wd[wdlen-homelen+1] = '\0';
		}
	}
	free(tmp);
}

int main(int argc, char **argv) {
	/* lets get some vars up in this b****/
	int i, j, k, throwout_line, showprompt, run_in_bkg, nscanned, tpid;  /* flags and iterators ****/
	int fv, cpid, status, fd_in, fd_out, pfd, pipefd[2], pipein, nargs; /* important containers ***/
	IS input;										 	               /* line parsing struct ****/
	char usr[32], *homedir, **arglist, buf[100], wd[1024];	          /* more useful containers */
	JRB inthepipe, treepid;										     /* processes in the pipe **/

	/* initialize variables */
	homedir = (char*) malloc(sizeof(char)*32);
	input = new_inputstruct(NULL); /* open input struct from stdin */
	inthepipe = make_jrb();
	i = 0; j = 0; k = 0;		
	fd_in = -1; fd_out = -1;
	nargs = 0;
	nscanned = 0;
	showprompt = 1;
	run_in_bkg = 0;
	throwout_line = 0;
	pipefd[0] = -1; /* if this is no longer -1, we know that we have opened a pipe */
	pipefd[1] = -1; /* if this is no longer -1, redirect output of previous fork() */
	pipein = -1;    /* variable for holding previous pipe's read file descriptor */

	/* if '-' is specified as the first commandline argument,
	   run the shell without showing prompt */
	if (argc == 2) {
		if (!strcmp(argv[1], "-")) {
			showprompt = 0;
		} else {
			fprintf(stderr, "usage1: ./gurush\nusage2: ./gurush - [don't show prompt]\n");
			exit(0);
		}
	} else if (argc > 2) {
		fprintf(stderr, "usage1: ./gurush\nusage2: ./gurush - [don't show prompt]\n");
		exit(0);
	}

	while(1) {

		/* get working dir and username for prompt */
		getcwd(wd, sizeof(wd));
		getlogin_r(usr, sizeof(usr));
		get_resolved_homedir(wd);

		/* reset flag */
		run_in_bkg = 0;
		k = 0;

		if (showprompt)
			printf("[gsh1.1:%s] %s $ ", wd, usr);

		/* read line into fields struct */
		nscanned = get_line(input);
		if (nscanned == -1) exit(0); /* exit on CTRL+D */

		if(nscanned > 0) {

			/* if command is "exit" or "bye", then quit the shell */
			if ((!strcmp(input->fields[0], "exit") || !strcmp(input->fields[0], "bye"))) {
				if (nscanned == 1) {
					jettison_inputstruct(input);
					exit(0);
				} else {
					fprintf(stderr, "'%s' must be specified with no arguments.\n", input->fields[0]);
					continue;
				}
			}

			/* put shell arguments into an array */
			arglist = (char**) malloc(sizeof(char*)*(input->NF + 1));
			for (i = 0; i < input->NF; i++) {
				if (throwout_line) break;

				/* check for file i/o redirection or pipe token */
				if (!strcmp(input->fields[i], "|")) {
					/* do some pipe shtuff (set up pipe) */

					if (i+1 < input->NF) {
						
						if (pipe(pipefd) < 0) {
						 	perror("pipe");
							fflush(stderr);
							for (j = 0; j < nargs; j++) {
								free(arglist[j]);
							}
							free(arglist);
							nargs = 0;
							throwout_line = 1;
							continue;
						}

						/* 
						   the preceding process' stdout is redirected to pipefd[1]
						   the current process' stdin reads from pipein (retained from pipefd[0])
						 */

						if ((pfd = fork()) == 0) {
							/* redirect output to pipe */
							if (dup2(pipefd[1], 1) != 1) {
								perror("dup2(pipefd[1])");
								fflush(stderr);
								for (j = 0; j < nargs; j++) {
									free(arglist[j]);
								}
								free(arglist);
								jettison_inputstruct(input);
								throwout_line = 1;
								continue;
							}
							
							/* get input from file redirect (if existent) */
							if(fd_in != -1) {
								if (dup2(fd_in, 0) != 0) {
									perror("dup2 fdin 1st fork");
									fflush(stderr);
								}
								close(fd_in);
								fd_in = -1;
							}
							
							/* get input from previous pipe */
							if (pipein != -1) {
								if (dup2(pipein, 0) != 0) {
									perror("dup2(pipefd[0])");
									fflush(stderr);
									for (j = 0; j < nargs; j++) {
										free(arglist[j]);
									}
									free(arglist);
									jettison_inputstruct(input);
									throwout_line = 1;
									continue;
								}	
								close(pipein); 
							}
							close(pipefd[0]);
							close(pipefd[1]);
							
							execvp(arglist[0], arglist);
							perror(arglist[0]);
							exit(1);
						} else {
							jrb_insert_int(inthepipe, pfd, new_jval_i(42));
							for (j = 0; j < nargs; j++) {
								free(arglist[j]);
							}
							free(arglist); 
							arglist = (char**) malloc(sizeof(char*)*(input->NF + 1));
							k = 0;
							nargs = 0;

							/* reclose our file descriptors for the next run */
							close(pipefd[1]);
							if (pipein != -1) close(pipein);
							if (fd_in != -1)  {
								close(fd_in);
								fd_in = -1;
							}
							pipein = pipefd[0];
						}
					} else {
						fprintf(stderr, "You must specify a program to pipe to.\n");
						for (j = 0; j < nargs; j++) {
							free(arglist[j]);
						}
						free(arglist);
						nargs = 0;
						throwout_line = 1;
						continue;
					}
				} else if (!strcmp(input->fields[i], "<")) {	
					/* if it exists, open following file read-only */
					if (i+1 < input->NF) {	
						if ((fd_in = open(input->fields[++i], O_RDONLY, 0644)) < 0) {
							perror("open");
							fflush(stderr);
							for (j = 0; j < nargs; j++) {
								free(arglist[j]);
							}
							free(arglist);
							nargs = 0;
							throwout_line = 1;
							continue;
						}

					} else {
						fprintf(stderr, "You must specify a file to redirect to stdin.\n");
						for (j = 0; j < nargs; j++) {
							free(arglist[j]);
						}
						free(arglist);
						nargs = 0;
						throwout_line = 1;
						continue;
					}
				} else if (!strcmp(input->fields[i], ">>")) {
					/* open following file for writing and appending */
					if (i+1 < input->NF) {
						if ((fd_out = open(input->fields[++i], O_WRONLY|O_CREAT|O_APPEND, 0644)) < 0) {
							perror("open");
							fflush(stderr);
							for (j = 0; j < nargs; j++) {
								free(arglist[j]);
							}
							free(arglist);
							nargs = 0;
							throwout_line = 1;
							continue;
						}
					} else {
						fprintf(stderr, "You must specify a file to which output can be redirected.\n");
						for (j = 0; j < nargs; j++) {
							free(arglist[j]);
						}
						free(arglist);
						nargs = 0;
						throwout_line = 1;
						continue;
					}
				} else if (!strcmp(input->fields[i], ">")) {
					/* open input->fields[i+1] as a file to write */ 	
					if (i+1 < input->NF) {
						if ((fd_out = open(input->fields[++i], O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
							perror("open");
							fflush(stderr);
							for (j = 0; j < nargs; j++) {
								free(arglist[j]);
							}
							free(arglist);
							nargs = 0;
							throwout_line = 1;
							continue;
						}
					} else {
						fprintf(stderr, "You must specify a file to redirect output to.\n");
						for (j = 0; j < nargs; j++) {
							free(arglist[j]);
						}
						free(arglist);
						nargs = 0;
						throwout_line = 1;
						continue;
					}
				} else if (!strcmp(input->fields[i], "&")) {
					/* caught run-in-background token, prompt will be printed promptly */	
					if (i == input->NF-1) {
						run_in_bkg = 1;
					} else {
						fprintf(stderr, "gsh: please specify '&' after all arguments (and before pipe?)\n");
						for (j = 0; j < nargs; j++) {
							free(arglist[j]);
						}
						free(arglist);
						nargs = 0;
						throwout_line = 1;
						continue;
					} 
				} else {
					/* add command to arglist and add terminating argument for execvp() */
					arglist[k++] = strdup(input->fields[i]);
					arglist[k] = NULL; 
					nargs++;
				}
			}
			
			/* if an error occurred, throw out the line and return to prompt */
			if (throwout_line) continue;

			/* implementation of cd with ~ shortcut for home folder
			   --
			   see get_resolved_homedir() for bugs
			 */
			if (!strcmp(arglist[0], "cd")) {
				if (input->NF == 2) {
					/* ~ is equivalent to home folder dir */
					if (arglist[1][0] == '~') {
						homedir = getenv("HOME");
						if (chdir(homedir) == -1)
							perror("cd");
						if (strlen(arglist[1]) > 2) { 
							/* if ~ preceded a deeper path, continue to specified dir */
							if (chdir(((void*)arglist[1]+2)) == -1) {
								perror("cd");
							}
						}
					} else if (chdir(arglist[1]) == -1) 
						perror(arglist[1]);
				} else 
					fprintf(stderr, "cd: incorrect arguments\n");

				for (j = 0; j < nargs; j++) 
					free(arglist[j]);
				free(arglist);
				nargs = 0;
				continue;
			}

			/* fork and execute specified programs */
			fv = fork();
			if (fv == 0) {
				/* redirect output if necessary */
				if(fd_out != -1) {
					if (dup2(fd_out, 1) != 1) {
						perror("dup2 fdout 2nd fork");
						fflush(stderr);
					}
					close(fd_out);
				}

				/* update input source */
				if(fd_in != -1) {
					if (dup2(fd_in, 0) != 0) {
						perror("dup2 fdin 2nd fork");
						fflush(stderr);
					}
					close(fd_in);
				}

				/* if previous commands were piped together, this is the last command
				 * and should take input from the penultimate command's stdout 
				 * ...
				 * do you like that Dr. Plank? do you like when I use the p-word?
				 */
				if (pipein != -1) {
					if (dup2(pipein, 0) != 0) {
						perror("dup2 in second fork()");
						for (j = 0; j < nargs; j++) {
							free(arglist[j]);
						}
						free(arglist);
						jettison_inputstruct(input);
						exit(1);
					}
					close(pipein);
				}
				/* execute command */
				execvp(arglist[0], arglist);
				perror(arglist[0]);
				exit(1);
			} else {
				jrb_insert_int(inthepipe, fv, new_jval_i(42));
				if (!run_in_bkg) {
					/* wait on all processes */
					while(!jrb_empty(inthepipe)) {
						tpid = wait(&status);
						if ((treepid = jrb_find_int(inthepipe, tpid)) != NULL) {
							jrb_delete_node(treepid);
							//remove tpid from tree (remove treepid)
						}
					}
				} else {
					/* if we're not running in the background, reset tree */
					jrb_free_tree(inthepipe);
					inthepipe = make_jrb();
				}

				/* reset descriptor variables and reinstate flag vals */
				if (fd_in != -1)  { close(fd_in);  fd_in  = -1; }
				if (fd_out != -1) { close(fd_out); fd_out = -1; }
				
				if (pipein != -1) { 
					close(pipein); 
					pipefd[0] = -1; 
					pipefd[1] = -1; 
					pipein = -1; 
				}
			}
			for (j = 0; j < nargs; j++) {
				free(arglist[j]);
			}
			free(arglist);
			nargs = 0;
		}
	}
	jettison_inputstruct(input);
}
