/**
 * Linux Job Control Shell Project
 *
 * Operating Systems
 * Grados Ing. Informatica & Software
 * Dept. de Arquitectura de Computadores - UMA
 *
 * Some code adapted from "OS Concepts Essentials", Silberschatz et al.
 *
 * To compile and run the program:
 *   $ gcc shell.c job_control.c -o shell
 *   $ ./shell
 *	(then type ^D to exit program)
 **/

#include "job_control.h"   /* Remember to compile with module job_control.c */

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough */

job* job_list;

/**********************AMPLIACION*************************/
void sighup_handler(){
	FILE *fp;
 	fp=fopen("hup.txt","a"); // abre un fichero en modo 'append'
 	if (fp) { 
    	fprintf(fp, "SIGHUP recibido.\n"); //escribe en el fichero
    	fclose(fp);
 	}
}

#include <dirent.h>
#include <stdio.h>

void traverse_proc(void) {
    DIR *d; 
    struct dirent *dir;
    char buff[2048];
    d = opendir("/proc");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            sprintf(buff, "/proc/%s/stat", dir->d_name); 
            FILE *fd = fopen(buff, "r");
            if (fd){
                long pid;     // pid
                long ppid;    // ppid
                char state;   // estado: R (runnable), S (sleeping), T(stopped), Z (zombie)

                // La siguiente linea lee pid, state y ppid de /proc/<pid>/stat
                fscanf(fd, "%ld %s %c %ld", &pid, buff, &state, &ppid);
								if (ppid == getpid() && state == 'Z') {
									printf("%ld\n", pid);
								}
                fclose(fd);
            }
        }
        closedir(d);
    }
}

#include <string.h>

// Convierte una cadena de comando como "xclock -update 1" a un array argv[]
void parse_command_args(const char* command_str, char* argv[]) {
    char* token;
    static char buffer[MAX_LINE]; // Usa un buffer local persistente
    strncpy(buffer, command_str, MAX_LINE);
    buffer[MAX_LINE - 1] = '\0';  // Asegura terminacion

    int i = 0;
    token = strtok(buffer, " ");
    while (token != NULL && i < MAX_LINE / 2 - 1) {
        argv[i++] = token;
        token = strtok(NULL, " ");
    }
    argv[i] = NULL;
}

/**********************FINAMPLIACION*************************/

void sigchld_handler() {
	block_SIGCHLD();

	job* iter = get_iterator(job_list);
    pid_t pid_wait;
    enum status status_res;
    int status, info;

	while(has_next(iter)){
		job* the_job = next(iter);
		pid_wait = waitpid(-the_job->pgid, &status, WCONTINUED | WNOHANG | WUNTRACED);  // ¿Signo - para the_job->pgid?

		if (pid_wait == the_job->pgid){
			status_res = analyze_status(status, &info);

			// Informamos del cambio de estado de la tarea
            printf("\nBackground pid: %d, command: %s, %s, info: %d\n", the_job->pgid, the_job->command, status_strings[status_res], info);

			if (status_res == SUSPENDED){ // Si la tarea ha sido suspendida
				the_job->state = STOPPED;
			}
			else if (status_res == CONTINUED){ // Si la tarea estaba suspendida y se ha reanudado
				the_job->state = BACKGROUND;
			}
			else { // Si no, la tarea ha terminado, luego la borramos
				/*Ampliacion*/
				if(the_job->state == RESPAWNABLE){
					pid_t new_pid = fork();
        if (new_pid == 0) { // hijo
            new_process_group(getpid());
            restore_terminal_signals();
            char *argv[MAX_LINE / 2];
            parse_command_args(the_job->command, argv);  // Necesitaras implementar esto si no existe
            execvp(argv[0], argv);
            perror("Error relanzando proceso respawnable");
            exit(EXIT_FAILURE);
        } else if (new_pid > 0) {
            printf("Respawned job... pid: %d, command: %s\n", new_pid, the_job->command);
            the_job->pgid = new_pid;
				}else {
            perror("Fork error al respawnear");
        }
				/*FinAmpliacion*/
				}else{ 
					delete_job(job_list, the_job);
				}	
			}
		}
		else if (pid_wait == -1){
			perror("Wait error");
		}
	}

	unblock_SIGCHLD();

}

/**
 * MAIN
 **/
int main(void)
{
	char inputBuffer[MAX_LINE]; /* Buffer to hold the command entered */
	int background;             /* Equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* Command line (of 256) has max of 128 arguments */
	/* Probably useful variables: */
	int pid_fork, pid_wait; /* PIDs for created and waited processes */
	int status;             /* Status returned by wait */
	enum status status_res; /* Status processed by analyze_status() */
	int info;				/* Info processed by analyze_status() */

	FILE *in_file, *out_file, *ap_file;

	ignore_terminal_signals();
	job_list = new_list("Job list");
	signal(SIGCHLD, sigchld_handler);
	signal(SIGHUP, sighup_handler); //Ampliacion

	while (1){   /* Program terminates normally inside get_command() after ^D is typed*/
		
		printf("COMMAND->");
		fflush(stdout);
		
		int respawnable = 0; //Ampliacion (detectar + en entrada)

		get_command(inputBuffer, MAX_LINE, args, &background);

		/*Ampliacion*/
		int i = 0; 
		while (args[i] != NULL) i++;
		if (i > 0 && strcmp(args[i - 1], "+") == 0) {
    	args[i - 1] = NULL;
    	respawnable = 1;
    	background = 1;
		}
		/*FinAmpliacion*/

		char *file_in, *file_out, *file_ap;
        parse_redirections(args, &file_in, &file_out, &file_ap);
		
		if(args[0]==NULL) continue;   /* Do nothing if empty command */

		/**********************AMPLIACION*************************/

		if(!strcmp(args[0], "currjob")){
			if(empty_list(job_list)){
				printf("No hay trabajo actual");
			}else{
				block_SIGCHLD();
				job* the_job = get_item_bypos(job_list, 1);
				printf("Trabajo actual: PID=%d command=%s", the_job->pgid, the_job->command);
				unblock_SIGCHLD();
			}
			continue;
		}

		if(!strcmp(args[0], "deljob")){
			block_SIGCHLD();
			if(empty_list(job_list)){
				printf("No hay trabajo actual");
			}else{
			 	int cont = 1;
				if(args[1]!=NULL){
					cont = atoi(args[1]);
				}
				job* the_job = get_item_bypos(job_list, cont);
				if(the_job->state == STOPPED){
					printf("No se permiten borrar trabajos en segundo plano suspendidos");
				}else{
					printf("Borrando trabajo actual de la lista de jobs: PID=%d command=%s", the_job->pgid, the_job->command);
					delete_job(job_list, the_job);
				}
			}
			unblock_SIGCHLD();
			continue;
		}

		if(!strcmp(args[0], "zjobs")){
			block_SIGCHLD();
			traverse_proc();
			unblock_SIGCHLD();
			continue;
		}

		if(!strcmp(args[0], "bgteam")){

			if(args[1] == NULL || args[2] == NULL){
				printf("El comando bgteam requiere dos argumentos");
				continue;
			}

			int cont = atoi(args[1]);
			if(cont>0){
				block_SIGCHLD();
				for(int i = 0; i<cont; i++){
					pid_fork = fork();
					if (pid_fork == 0){ // Hijo
						new_process_group(getpid());
						restore_terminal_signals();
						execvp(args[2], &args[2]);
						printf("\nError, command not found: %s\n", args[0]);
						exit(EXIT_FAILURE);
					}
					else {
						add_job(job_list, new_job(pid_fork, args[2], BACKGROUND));
					}
				}
				unblock_SIGCHLD();
			}
			continue;
		}

		/**********************FINAMPLIACION*************************/

		if (!strcmp(args[0], "cd")){
			if (args[1] != NULL) {
				chdir(args[1]);
			}
			else {
				if (chdir(getenv("HOME")) != 0) {
					printf("Error: HOME not set\n");
				}
			}
			continue;
		}

		if (!strcmp(args[0], "exit")){
			printf("Bye");
			exit (EXIT_SUCCESS);
			continue;
		}

		if (!strcmp(args[0], "jobs")){
			if (empty_list(job_list)){
				printf("No Background or Suspended jobs");
			}
			else {
				block_SIGCHLD();
				print_job_list(job_list);
				unblock_SIGCHLD();
			}
			continue;
		}

		if (!strcmp(args[0], "fg")){
			int pos = 1;
			if (args[1] != NULL){
				pos = atoi(args[1]);
			}
			block_SIGCHLD();
			job* the_job = get_item_bypos(job_list, pos);
			unblock_SIGCHLD();

			// Si el trabajo era respawnable, pierde su inmortalidad al ir al foreground
		if (the_job->state == RESPAWNABLE) {
    	the_job->state = FOREGROUND;
		}

			if (the_job != NULL) {
				pid_t the_job_pgid = the_job->pgid;
   				char* the_job_name = strdup(the_job->command);
				set_terminal(the_job->pgid);

				if (the_job->state == STOPPED) {
					killpg(the_job_pgid, SIGCONT);
				}

				block_SIGCHLD();
				delete_job(job_list, the_job);
				unblock_SIGCHLD();

				pid_wait = waitpid(the_job_pgid, &status, WUNTRACED);
				set_terminal(getpid());

				if (pid_wait == the_job_pgid) {
                    status_res = analyze_status(status, &info);
                    printf("\nForeground pid: %d, command: %s, %s, info: %d\n", the_job_pgid, the_job_name, status_strings[status_res], info);

                    if (status_res == SUSPENDED) {
                        block_SIGCHLD();
                        add_job(job_list, new_job(pid_wait, the_job_name, STOPPED));
                        unblock_SIGCHLD();
                    }

                } else if (pid_wait == -1) {
                    printf("Wait error");
                }
			}
			continue;
		}

		if (!strcmp(args[0], "bg")){
			int pos = 1;
			if (args[1] != NULL){
				pos = atoi(args[1]);
			}
			
			block_SIGCHLD();
			job* the_job = get_item_bypos(job_list, pos);
			unblock_SIGCHLD();

			/*Ampliacion*/
			if (the_job->state == RESPAWNABLE) {
    		the_job->state = BACKGROUND;
			}
			/*FinAmpliacion*/
			if (the_job != NULL && the_job->state == STOPPED) {
				the_job->state = BACKGROUND;
				killpg(the_job->pgid, SIGCONT);
				printf("\nBackground job running... pid: %d, command: %s\n", the_job->pgid, the_job->command);
			}
			continue;
		}

		pid_fork = fork();

		if (pid_fork > 0){ // Padre
			new_process_group(pid_fork);

			if (!background){ // Foreground
				set_terminal(pid_fork);
				pid_wait = waitpid(pid_fork, &status, WUNTRACED);
				set_terminal(getpid());

				if (pid_wait == pid_fork){
					status_res = analyze_status(status, &info);
					printf("\nForeground pid: %d, command: %s, %s, info: %d\n", pid_fork, args[0], status_strings[status_res], info);

					if (status_res == SUSPENDED){
						block_SIGCHLD();
						add_job(job_list, new_job(pid_fork, args[0], STOPPED));
						unblock_SIGCHLD();
					}
				}
				else if (pid_wait == -1){
					perror("Wait error");
				}
			}
			else { // Background
				printf("\nBackground job running... pid: %d, command: %s\n", pid_fork, args[0]);
				/*Ampliacion*/
				enum job_state job_mode;
				if(respawnable){
					job_mode = RESPAWNABLE;
				}else{
					job_mode = BACKGROUND;
				}
				/*FinAmpliacion*/
				block_SIGCHLD();
				add_job(job_list, new_job(pid_fork, args[0], job_mode));
				unblock_SIGCHLD();
			}
		}
		else if (pid_fork == 0){ // Hijo
			new_process_group(getpid());
			if (!background) {
				set_terminal(getpid());
			}
			restore_terminal_signals();	

			if (file_in) {
				in_file = fopen(file_in, "r");

				if (in_file == NULL) {
					perror("Redirecition error.");
					exit(EXIT_FAILURE);
				}
				
				if (dup2(fileno(in_file), STDIN_FILENO) == -1) {
					perror("Redirection error");
					exit(EXIT_FAILURE);
				}

				fclose(in_file);
			}

			if (file_out) {
				out_file = fopen(file_out, "w");

				if (out_file == NULL) {
					perror("Redirecition error.");
					exit(EXIT_FAILURE);
				}
				
				if (dup2(fileno(out_file), STDOUT_FILENO) == -1) {
					perror("Redirection error");
					exit(EXIT_FAILURE);
				}

				fclose(out_file);
			}

			/**********************AMPLIACION*************************/

			if(file_ap){
				ap_file = fopen(file_ap, "a");

				if (ap_file == NULL) {
					perror("Redirecition error.");
					exit(EXIT_FAILURE);
				}
				
				if (dup2(fileno(ap_file), STDOUT_FILENO) == -1) {
					perror("Redirection error");
					exit(EXIT_FAILURE);
				}

				fclose(ap_file);
			}

			/**********************FINAMPLIACION*************************/

			if(!strcmp(args[0], "fico")){ //Ampliacion
				execvp("./cuentafich.sh", args);
			}else{                        //FinAmpliacion
				execvp(args[0], args);
			}
			
			// Si falla, continua por aqui
			printf("\nError, command not found: %s\n", args[0]);
			exit(EXIT_FAILURE);
		}

	} /* End while */
}
