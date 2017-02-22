#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <semaphore.h>

#define LOAD_SEM "load_sem"
#define SHARE_SEM "share_mem_sema"
#define READY_RUN_SEM "ready_for_run"
#define UNLOAD_SEM "unload_sem"
#define EMPTY_SEM "empty_sem"
#define READY_FOR_KILL "kill_me_please"
#define SHM_PASS_KEY 2345789
#define SHM_PASS_UNLOAD_KEY 1245789
#define SHM_PASS_LOAD_KEY 1248789
#define BASE_10 10
#define MILISECOND 1000

FILE *file;
int car_capacity,number_of_passagers;
int action_counter_SHM;


void help();
int create_semaphores();
void clean_all();
void car(int time_on_the_road);
void generate_pass(int new_passager_time);
void print_SHM(char *process, int number_index,char *action);
void test_args(int number_of_passagers, int car_capacity, int new_passager_time, int time_on_the_road);
void passager(int number_of_process_SHM, int unload_order_SHM, int load_order_SHM);



int main( int argc, char *argv[] ) {

	if( argc != 5)	// wrong number of parameters, raise help
		help();

	file = fopen("proj2.out","w"); // basic output file
	if ( file == NULL ) {				 // error
		fprintf(stderr,"File cannot be open\n");
		return -1;
	}

	// ################### Save, convert and test arguments ###################
	char *end_ptr = NULL;

	number_of_passagers = strtoimax(argv[1], &end_ptr, BASE_10);
	if ( *end_ptr != 0 || errno == ERANGE ) {
		help();
		return -1;
	}
    car_capacity = strtoimax(argv[2], &end_ptr, BASE_10);
    if ( *end_ptr != 0 || errno == ERANGE ) {
		help();
		return -1;
	}
    int new_passager_time = strtoimax(argv[3], &end_ptr, BASE_10);
    if ( *end_ptr != 0 || errno == ERANGE ) {
		help();
		return -1;
	}
    int time_on_the_road = strtoimax(argv[4], &end_ptr, BASE_10);
    if ( *end_ptr != 0 || errno == ERANGE ) {
		help();
		return -1;
	}

	test_args(number_of_passagers, car_capacity, new_passager_time, time_on_the_road);
   

    // ########################################################################
    // ########################### Initialization  ############################
    pid_t car_pid, gen_pid, tmp_pid;

    action_counter_SHM =shmget(IPC_PRIVATE,sizeof(int), IPC_CREAT | S_IRUSR | S_IWUSR );
    int *ac =(int *)shmat(action_counter_SHM,NULL,0);
    (*ac) = 1;

    if (action_counter_SHM == -1 ) {
    	fprintf(stderr,"Error with creating share memory\n");
    	return -2;
    }

    srand(time(0)); 	// for pseudo-random number

    int t=create_semaphores();
    if ( t == -1 ) {
    	fprintf(stderr,"Error with creating semaphore\n");
    	return -2;
    }
	
	setbuf(file,NULL);
	setbuf(stderr,NULL);
	// ##########################  Its fork time ###########################
	if ( (tmp_pid = fork() ) < 0 ) { 					// fail fork
		fprintf(stderr,"Problem with system call\n");
		exit(-2);
	}

	if ( tmp_pid ==  0 ) {		// child process - car
		car(time_on_the_road);
		exit(0);				// end of process
	}

	else {
		car_pid = tmp_pid;
		if ( ( tmp_pid = fork() ) < 0 ) {				// fail fork
			fprintf(stderr,"Problem with system call\n");
			exit(-2);
		}

		if ( tmp_pid == 0 ) { 	// child process - generete passagers
			generate_pass(new_passager_time);
			exit(0);			// end of new process
		}

		else {
			gen_pid = tmp_pid;
		}
	}

	waitpid(car_pid, NULL, 0);		// wait for end of process car
	waitpid(gen_pid, NULL, 0);		// wait for end of process generete

	clean_all();	// destroy all semaphore and SHM

	return 0;
}

void clean_all() {

	shmctl(action_counter_SHM, IPC_RMID, NULL);	 // destroy shared memory
	fclose(file);

	sem_unlink(SHARE_SEM);			// destroy all semafors
	sem_unlink(LOAD_SEM);
	sem_unlink(READY_RUN_SEM);
	sem_unlink(UNLOAD_SEM);
	sem_unlink(EMPTY_SEM);
	sem_unlink(READY_FOR_KILL);
}

int create_semaphores() {
	int r=0;
	sem_t *load_sem  = sem_open(LOAD_SEM,O_CREAT,0666,0);
	sem_t *unload_sem  = sem_open(UNLOAD_SEM,O_CREAT,0666,0);
	sem_t *ready_for_kill = sem_open(READY_FOR_KILL,O_CREAT,0666,0);
	sem_t *ready_for_run = sem_open(READY_RUN_SEM,O_CREAT,0666,0);
	sem_t *share_mem_sem = sem_open(SHARE_SEM,O_CREAT,0666,1);
	sem_t *empty_sem = sem_open(EMPTY_SEM,O_CREAT,0666,1);

	if  ( load_sem == SEM_FAILED || unload_sem == SEM_FAILED ||
		 ready_for_kill == SEM_FAILED || ready_for_run == SEM_FAILED ||
		 share_mem_sem == SEM_FAILED || empty_sem == SEM_FAILED
		) {
		r = -1;
	}

	sem_close(unload_sem);
	sem_close(ready_for_kill);
	sem_close(ready_for_run);
	sem_close(share_mem_sem);
	sem_close(load_sem);
	sem_close(empty_sem);

	return r;
}

void test_args(int number_of_passagers, int car_capacity, int new_passager_time, int time_on_the_road) {
	if ( number_of_passagers <= 0 || car_capacity <= 0 ||
    	 number_of_passagers <= car_capacity || number_of_passagers % car_capacity !=0) {
        help();
        exit(-1);
    }

    if ( time_on_the_road < 0 || time_on_the_road >= 5001 ) {
    	help();
    	exit(-1);
    }

    if ( new_passager_time < 0 || new_passager_time >= 5001) {
        help();
        exit(-1);
    }
}

void passager(int pass_index_SHM,int unload_order_SHM, int load_order_SHM ) {
	sem_t *load_sem = sem_open(LOAD_SEM, O_RDWR);
	sem_t *share_mem_sem= sem_open(SHARE_SEM, O_RDWR);
	sem_t *ready_for_run = sem_open(READY_RUN_SEM,O_RDWR);
	sem_t *empty_sem = sem_open(EMPTY_SEM,O_RDWR);
	sem_t *unload_sem = sem_open(UNLOAD_SEM,O_RDWR);
	sem_t *ready_for_kill = sem_open(READY_FOR_KILL,O_RDWR);

	int *ac =(int *)shmat(action_counter_SHM,NULL,0);
	int *index =(int *)shmat(pass_index_SHM,NULL,0);
	int *unload_order =(int *)shmat(unload_order_SHM,NULL,0);
	int *load_order =(int *)shmat(load_order_SHM,NULL,0);

	sem_wait(share_mem_sem);
	int position = (*index)+1 % car_capacity;
	fprintf(file,"%i:P %i : started\n",(*ac),position);
	fflush(file);
	(*ac)++;
	(*index)++;
	sem_post(share_mem_sem);

	sem_wait(load_sem);
	print_SHM("P", position, "board");
	sem_post(load_sem);

	sem_wait(load_sem);
	int load_position = ((*load_order)+1)%car_capacity;
	(*load_order)++;
	if ( load_position != 0 ) {
		char action[100];
		sprintf(action,"board order %i",load_position);
		print_SHM("P", position, action);
		sem_post(load_sem);
	}
	else {
		print_SHM("P", position, "board last");
		(*load_order) = 0;
		sem_post(ready_for_run);
	}

	sem_wait(unload_sem);
	print_SHM("P", position, "unboard");
	sem_post(unload_sem);

	sem_wait(unload_sem);
	int unload_position = ((*unload_order)+1)%car_capacity;
	(*unload_order)++;
	if ( unload_position != 0 ) {
		char action[100];
		sprintf(action,"unboard order %i",unload_position);
		print_SHM("P", position, action);
		sem_post(unload_sem);
	}
	else {
		print_SHM("P", position,"unboard last");
		(*unload_order) = 0;
		sem_post(empty_sem);
	}

	sem_wait(ready_for_kill);
	print_SHM("P",position,"finished");
	sem_post(ready_for_kill);
}

void print_SHM(char *process, int number_index,char *action) {
	sem_t *share_mem_sem= sem_open(SHARE_SEM, O_RDWR);
	int *ac =(int *)shmat(action_counter_SHM,NULL,0);

	sem_wait(share_mem_sem);
	fprintf(file,"%i:%s %i: %s\n",(*ac),process,number_index,action);
	fflush(file);
	(*ac)++;
	sem_post(share_mem_sem);

	shmdt(ac);
	sem_close(share_mem_sem);
}

void generate_pass(int new_passager_time) {
	// ##################################### Initialization ########################################
	int pass_index_SHM =shmget(SHM_PASS_KEY,sizeof(int), IPC_CREAT | S_IRUSR | S_IWUSR );
	int unload_order_SHM =shmget(SHM_PASS_UNLOAD_KEY,sizeof(int), IPC_CREAT | S_IRUSR | S_IWUSR );
	int load_order_SHM =shmget(SHM_PASS_LOAD_KEY,sizeof(int), IPC_CREAT | S_IRUSR | S_IWUSR );
	pid_t pass_id[number_of_passagers];

	if ( pass_index_SHM == -1 || unload_order_SHM == -1 || load_order_SHM == -1 ) {
		fprintf(stderr,"Error with creating share memory\n");
		shmctl(pass_index_SHM, IPC_RMID, NULL);
		shmctl(unload_order_SHM, IPC_RMID, NULL);
		shmctl(load_order_SHM, IPC_RMID, NULL);
		
		clean_all();
		exit(-2);
	}

	int *index =(int *)shmat(pass_index_SHM,NULL,0);
	int *unload_order =(int *)shmat(unload_order_SHM,NULL,0);
	int *load_order =(int *)shmat(load_order_SHM,NULL,0);

	(*index)=0;
	(*unload_order)=0;
	(*load_order)=0;

	// ##############################################################################################
	for(int i=0; i < number_of_passagers; i++) { // create passagers
		pid_t pid;
		if ( ( pid=fork() ) < 0 ) {	 		// bad fork
			perror("fork");
			exit(-2);						// end with exit code -2
		}

		if ( pid == 0 ) {
			passager(pass_index_SHM,unload_order_SHM,load_order_SHM);
			exit(0);
		}

		else {								// save id, and sleep
			pass_id[i]=pid;
			int sleep_time = rand()%(new_passager_time+1);
			usleep(sleep_time * MILISECOND);
		}
	}

	for( int i = 0; i < number_of_passagers; i++) {	// wait for all
		waitpid(pass_id[i],NULL,0);
	}

	shmctl(pass_index_SHM, IPC_RMID, NULL);			// clean SHM
	shmctl(unload_order_SHM, IPC_RMID, NULL);
	shmctl(load_order_SHM, IPC_RMID, NULL);
}

void car(int time_on_the_road) {
	sem_t *load_sem = sem_open(LOAD_SEM, O_RDWR);
	sem_t *ready_for_run = sem_open(READY_RUN_SEM,O_RDWR);
	sem_t *empty_sem = sem_open(EMPTY_SEM,O_RDWR);
	sem_t *unload_sem = sem_open(UNLOAD_SEM,O_RDWR);
	sem_t *ready_for_kill = sem_open(READY_FOR_KILL,O_RDWR);

	print_SHM("C",1,"started");	

	for(int i=0; i < number_of_passagers/car_capacity; i++) {
		sem_wait(empty_sem);
		print_SHM("C",1,"load");	
		sem_post(load_sem);
		
		sem_wait(ready_for_run);
		print_SHM("C",1,"run");

		int sleep_time = rand()%(time_on_the_road+1);
		usleep(sleep_time * MILISECOND);

		print_SHM("C",1,"unload");
		sem_post(unload_sem);

		if ( i == (number_of_passagers/car_capacity-1)) {
			sem_wait(empty_sem);
			sem_post(ready_for_kill);


			sem_wait(ready_for_kill);
			print_SHM("C",1,"finished");
			sem_post(ready_for_kill);
		}
	}
}

void help() {
    fprintf(stderr,"./proj2 P C PT RT\n");
    fprintf(stderr,"P is number of proces represent passager, P > 0\n");
    fprintf(stderr,"C is capacity of cart, C > 0, P > C, P %% C == 0 \n");
    fprintf(stderr,"PT is maximal value of time (in ms), after which is generate new process for passanger\n");
    fprintf(stderr,"RT is maximal value of time (in ms) for passage trough railway track\n");
    exit(1);
}