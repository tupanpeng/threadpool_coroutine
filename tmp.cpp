#include<iostream>
#include<pthread.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<cstring>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/wait.h>

#define ERR_EXIT(m) do{perror(m);exit(EXIT_FAILURE);}while(0)
using namespace std;

typedef struct{
	int sock;
	struct sockaddr_in* servaddr;
}request_args;

void* request(void * args)
{
	request_args ra = *((request_args*)args);
	struct sockaddr_in servaddr = *ra.servaddr;
	int sock = ra.sock;
		
	if(connect(sock,(sockaddr*)&servaddr,sizeof(servaddr))<0)
                ERR_EXIT("connect");

        cout<<"connected!"<<endl;
        char buf[1000]="asdcefg";
        int len = write(sock,buf,1000);
        cout<<"writelen:"<<len<<endl;
        len = read(sock,buf,1000);
	sleep(10);
        cout<<"readlen:"<<len<<" return:"<<buf<<endl;
	//pthread_exit(NULL);
	return NULL;
}

using namespace std;
int main()
{
	std::cout<<"pthread_t:"<<sizeof(pthread_t)<<endl
		<<"pid_t:"<<sizeof(pid_t)<<std::endl
		<<"void*:"<<sizeof(void*)<<endl
		<<"int:"<<sizeof(int)<<endl
		<<"long int:"<<sizeof(long int)<<endl
		<<"long long int:"<<sizeof(long long int)<<endl
		<<"float:"<<sizeof(float)<<endl
		<<"double:"<<sizeof(double)<<endl
		;

	struct sockaddr_in servaddr;
	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(9999);
	servaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
	
	int cid,process_num=100;
	while((cid=fork())>0)
	{
		process_num--;
		//parent
		if(process_num==0)
		{
			waitpid(-1,NULL,0);
			return 0;
		}
	}
	//child
	int sock;
        if((sock=socket(PF_INET,SOCK_STREAM,0))<0)
                ERR_EXIT("socket");

	struct sockaddr_in cliaddr;
        memset(&cliaddr,0,sizeof(cliaddr));
        cliaddr.sin_family=AF_INET;
        cliaddr.sin_port=htons(11111+process_num);
        cliaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
        int on =1;
        if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on))<0)
                ERR_EXIT("setsocket");
	
	if(0>bind(sock,(sockaddr*)&cliaddr,sizeof(cliaddr)))
		ERR_EXIT("bind");

	request_args ra = {sock,&servaddr}; 
	request((void*)&ra);
	/*pthread_t tid=-1;
	request_args ra = {sock,&servaddr};
	pthread_create(&tid,NULL,request,(void *)&ra);
	pthread_join(tid,NULL);*/
	return 0;
}
