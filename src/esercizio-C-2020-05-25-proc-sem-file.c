#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>

#define FILE_SIZE (1024*1024)

#define N 4

sem_t * proc_sem;
sem_t * mutex;

void soluzione_A();
void soluzione_B();
void child_A();

#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }

int main(){

	proc_sem = mmap(NULL, sizeof(sem_t)*2,
							PROT_READ | PROT_WRITE,
							MAP_SHARED | MAP_ANONYMOUS,
							-1,
							0);

	if (proc_sem == MAP_FAILED) {
			perror("mmap()");
			exit(EXIT_FAILURE);
		}

	mutex = &proc_sem[1];

	int res = sem_init(proc_sem,
				1, // 1 => il semaforo è condiviso tra processi,
				   // 0 => il semaforo è condiviso tra threads del processo
				0 // valore iniziale del semaforo (se mettiamo 0 che succede?)
			  );

	if(res==-1){
		perror("sem_init");
		exit(EXIT_FAILURE);
	}

	res = sem_init(mutex,
						1, // 1 => il semaforo è condiviso tra processi,
						   // 0 => il semaforo è condiviso tra threads del processo
						1 // valore iniziale del semaforo (se mettiamo 0 che succede?)
					  );

	if(res==-1){
		perror("sem_init");
		exit(EXIT_FAILURE);
	}

	printf("ora avvio la soluzione_A()...\n");
	soluzione_A();

	printf("ed ora avvio la soluzione_B()...\n");
	soluzione_B();

	printf("bye!\n");


	sem_destroy(proc_sem);
	sem_destroy(mutex);

	return 0;
}

void soluzione_A(){

	int file_fd;
	file_fd = open("outputA.txt",
					  O_CREAT | O_RDWR | O_TRUNC,
					  S_IRUSR | S_IWUSR
					  );

	int res = ftruncate(file_fd, FILE_SIZE);

	if (res == -1) {
		perror("ftruncate()");
		exit(EXIT_FAILURE);
	}
	close(file_fd);

	for(int i=0; i<N; i++){

		switch(fork()){
		case -1:
			perror("fork");
			exit(EXIT_FAILURE);
		case 0:
			child_A(i);
			break;

		default:
			;
		}
	}

	for(int i=0; i< FILE_SIZE+N; i++){

		if (sem_post(proc_sem) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}

	}

	while(wait(NULL)!= -1);

	printf("soluzione A completa.\n");


}

void soluzione_B(){

	int file_fd;
	int file_end=0;
	file_fd = open("outputB.txt",
					  O_CREAT | O_RDWR | O_TRUNC,
					  S_IRUSR | S_IWUSR
					  );

	int res = ftruncate(file_fd, FILE_SIZE);

	if (res == -1) {
		perror("ftruncate()");
		exit(EXIT_FAILURE);
	}

	char * addr;

	addr = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
				FILE_SIZE, // dimensione della memory map
				PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
				MAP_SHARED, // memory map condivisibile con altri processi
				file_fd,
				0); // offset nel file

	if (addr == MAP_FAILED) {
		perror("mmap()");
		exit(EXIT_FAILURE);
	}
	close(file_fd);
	int j;
	int write_count;

	for(int i =0; i<N; i++){

		switch(fork()){

		case -1:
			perror("fork");
			exit(EXIT_FAILURE);

		case 0:
			j=0;
			write_count=0;
			while(file_end==0){
				if (sem_wait(proc_sem) == -1) {
						perror("sem_wait");
						exit(EXIT_FAILURE);
					}

				if (sem_wait(mutex) == -1) {
						perror("sem_wait");
						exit(EXIT_FAILURE);
					}


					while(addr[j]!=0 && j<FILE_SIZE){
						j++;
					}

					if(j==FILE_SIZE){

						file_end++;

					}else{

						char ind = 'A'+ i;
						addr[j] = ind;
						write_count++;


						//printf("child B[%d] scrivo %c in posizione %d\n",i, ind, j);
						j++;
					}
					if (sem_post(mutex) == -1) {
								perror("sem_post");
								exit(EXIT_FAILURE);
							}
			}
			printf("child B [%d] : EOF, write count = %d\n", i, write_count);

			exit(EXIT_SUCCESS);
			break;

		default:
			;
		}
	}
	for(int i=0; i< FILE_SIZE+N; i++){

		if (sem_post(proc_sem) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}

	}

	while(wait(NULL)!= -1);

	printf("soluzione B completa.\n");

}

void child_A(int i){

	int file_fd;
	int res;
	file_fd = open("outputA.txt",O_RDWR) ;

	off_t posiz;
	char carattere;
	int file_end=0;

	int write_counter=0;

	while(file_end==0){
		if (sem_wait(proc_sem) == -1) {
				perror("sem_wait");
				exit(EXIT_FAILURE);
			}
		if (sem_wait(mutex) == -1) {
							perror("sem_wait");
							exit(EXIT_FAILURE);
						}


		while((res = read(file_fd, &carattere, sizeof(char)))>0 && carattere != 0);

		if(res == 1){
			char ind = 'A'+ i;

			posiz = lseek(file_fd, -1,SEEK_CUR);

			res = write(file_fd, &ind, sizeof(ind));
			CHECK_ERR(res, "write")

			write_counter++;
			//printf("child A[%d] scrivo %c in posizione %ld\n", i, ind, posiz);

		}else{
			printf("child A[%d] EOF, write count = %d.\n", i, write_counter);
			file_end++;
		}

		if (sem_post(mutex) == -1) {
				perror("sem_wait");
				exit(EXIT_FAILURE);
			}
	}

	close(file_fd);
	exit(EXIT_SUCCESS);
}






