#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include<assert.h>
#include<pthread.h>
#include<time.h>
#include<termios.h>
#include <unistd.h>
#include<poll.h>


#define MAX_TRANSFER 500     //maximum transfer amount at an instant
#define CONSTANT 97        
#define SIZE 20

FILE *fptr;

int money=1000,id,p;
pthread_mutex_t count_mutex;

int set=0;
int state=-1;
int marker_sent=0;
int marker_receive=0;
int marker_sent_by=-1;
int Rarr[SIZE];
int mat[SIZE][SIZE];
int chDataTop[SIZE];

/*Random function for selecting node*/
void init()
{
	int i,j;
	for(i=0;i<p;i++)
	{	
		Rarr[i]=0;
		chDataTop[i]=-1;
	}
	
	for(i=0;i<p;i++)
	   for(j=0;j<SIZE;j++)
		mat[i][j]=-1;
	state=-1;
	marker_sent=0;
	marker_receive=0;
	marker_sent_by=-1;

}

void send_marker()
{
	int i=0;
	int d=0;

	for(i=0;i<p;i++)
	{
		if(i!=id)
		{
          MPI_Send(&d,1,MPI_INT,i,7,MPI_COMM_WORLD);
          //fprintf(fptr,"Sending marker from rank %d  to rank %d \n",id,i);
        }
	}
		
}

void start_recording()
{
	int i=0;
	for(i=0;i<p;i++)
	{
		if(i!=id && i!=marker_sent_by)
			Rarr[i]=1;
	}
}		

void print_mat(int row)
{	
	int i=0;
	//while(i<SIZE)
	while(mat[row][i]!=-1 && mat[row][i]!=0)
	{
		printf("%d,",mat[row][i]);
		i++;
	}
}

void stop_algo()
{    
	sleep(id*0.2);
	int i=0;
	printf("\np%d\t%d\t",id,state);
    	for(i=0;i<p;i++)
	{
		printf("{");
		print_mat(i);
		printf("}    ");
	}
    printf("\n");
    fflush(stdout);   
    init();		
}
int check(int d)
{
	int ok=1;
	pthread_mutex_lock(&count_mutex);
	if(d<=money)
	ok=0;
	pthread_mutex_unlock(&count_mutex);
	if(d==0)
	ok=1;
	return ok;
}	
void *Send_Func_For_Thread(void *arg)
{
	MPI_Request req;
	int x;
	while(1)                       //execute continuosly
	{
		if(set==2)
		break;
		if(set==1)                 //if polled then stop sending (polling will make set=1)
		{
			if(marker_sent==0)
			{
				pthread_mutex_lock(&count_mutex);      //deduct money
				state=money;
				pthread_mutex_unlock(&count_mutex);	
				marker_sent=1;
				send_marker();
				marker_receive++;
				start_recording();
			}
			else
			{
				Rarr[marker_sent_by]=0;
				marker_receive++;
			}
			
			if((id==0 && marker_receive==p)||(id!=0 && marker_receive==p-1))
			stop_algo();
			marker_sent_by=-1;
			set=0;
			
			continue;
		}
		x = id;
		do
		{
			//x=randsend(p);         //select reciever node             
			x=rand()%p;
            sleep(.5);
		}
		while(x==id);
		int d;
		do{
			//d = randmoney(MAX_TRANSFER > money ? money:MAX_TRANSFER);              //select random sending amount
			d=rand()%50;
			sleep(0.3);
		}
		while(check(d));
		//while(d>money && d<=0);

		pthread_mutex_lock(&count_mutex);      //deduct money
		money = money-d;
		pthread_mutex_unlock(&count_mutex);

		MPI_Send(&d,1,MPI_INT,x,0,MPI_COMM_WORLD);  //send money
        //int q=(int)d;
		//fprintf(fptr,"Sending money = %d   from rank %d  to rank %f \n",q,id,x);
	}

	pthread_exit((void *)NULL);

}

void *Recv_Func_For_Thread(void *arg)
{
	MPI_Status status;
	int src;
	MPI_Request req;
	int flag;           //will set to true on completion of request
	int d,j;
	while(1)           //exectue continuosly
	{
		//if(set==2)
		//break;
		MPI_Irecv(&d,1,MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&req);    //Recv from any source, any tag
		MPI_Test(&req,&flag,&status);               //Test for the completion of request
        
		time_t st = time(NULL);

		while(!flag && difftime(time(NULL),st) < 2)   //wait for 5 second to receive all messages
		{
			//MPI_Irecv(&d,1,MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&req);
			MPI_Test(&req,&flag,&status);
		}

		if(!flag)   //if request not completed cancel it
		{
			MPI_Cancel(&req);
			MPI_Request_free(&req);
			break;
		}
        //fprintf(fptr,"receive:%d from %d Tag:%d\n",id,status.MPI_SOURCE,status.MPI_TAG);
		if(status.MPI_TAG == 7)      //if Recieved message tag==7 then break sending Thread by making set=1
		{
            //fprintf(fptr,"receive:%d from %d Tag:%d\n",id,status.MPI_SOURCE,status.MPI_TAG);
			set = 1;
			marker_sent_by=status.MPI_SOURCE;
			while(set==1)
            sleep(.4);
			continue;
		}
        //for Recording Purpose:
		src=status.MPI_SOURCE;
		if(Rarr[src])
		{
			chDataTop[src]++;
			mat[status.MPI_SOURCE][chDataTop[src]]=d;
		}
		pthread_mutex_lock(&count_mutex);
		money = money + d;                   //update amount
		pthread_mutex_unlock(&count_mutex);

	//	printf("\nRecieved money from rank %d . \n",status.MPI_SOURCE);
	//	printf("My balance of rank %d is :    %d\n\n",id,money);
    }
	pthread_exit((void *)NULL);

}

int main ( int argc, char *argv[] )
{
    int provided,count=0;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    //
    fptr=fopen("res.txt","w");
	double i; 
	int j,d=0,flag;
	MPI_Request req;
	MPI_Status status;
	
	srand(time(0));

	pthread_t thread[2];     // array for Send and Recv thread

	//
	MPI_Comm_rank ( MPI_COMM_WORLD, &id );
	MPI_Comm_size ( MPI_COMM_WORLD, &p );
    init();
     /*Sending & Receiving Thread Starts */
	pthread_create(&thread[0], NULL, Send_Func_For_Thread, (void *)0);  //Send Thread
	pthread_create(&thread[1], NULL, Recv_Func_For_Thread, (void *)1);  //Recieve Thread

    /*Polling Mechanism */

	struct pollfd mypoll = {STDIN_FILENO,POLLIN|POLLPRI };
	int t;
	if(id==0){    // take polling input from node 0 only
		while(1){
                //fflush(stdin);
			if(poll(&mypoll,1,100)){   // poll with timeout of 100 ms
				scanf("%d",&t);        //accept input
				if(t==1){              //if input is 1
					          
					MPI_Send(&d,1,MPI_INT,0,7,MPI_COMM_WORLD);    //sending message with special tag (7) to initialize (set=1) for all other nodes
                    t=0;
				}
				if(t==2)
				set=2;
				sleep(1);
                //fflush(stdin);
			}
		}
	}
	pthread_join(thread[0],NULL);    //wait for Send Thread termination
	pthread_join(thread[1],NULL);    //wait for Recv Thread termination
	fclose(fptr);

	MPI_Finalize();                    //terminate MPI execution environment
	pthread_exit((void *)NULL) ;
}
