#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<signal.h>
#include<errno.h>
//#include"threadpool.h"
#include<assert.h>

#define DEFAULT_TIME 10
#define MIN_WAIT_TASK_NUM 10
#define DEFAULT_THREAD_VARY 10
#define ERR_EXIT(m) do{perror(m);exit(EXIT_FAILURE);}while(0)
typedef struct
{
	void *(*function)(void*);
	void *arg;
}threadpool_task_t;

typedef struct 
{
	pthread_mutex_t lock;
	pthread_mutex_t thread_counter;
	pthread_cond_t queue_not_full;
	pthread_cond_t queue_not_empty;
	pthread_t *threads;//thread id(TID)
	pthread_t adjust_tid;
	threadpool_task_t *task_queue;
	int min_thr_num;
	int max_thr_num;
	int live_thr_num;
	int busy_thr_num;
	int wait_exit_thr_num;
	int queue_front;
	int queue_rear;
	int queue_size;
	int queue_max_size;
	int shutdown;
}threadpool_t;

void* threadpool_thread(void *threadpool);

void* adjust_thread(void *threadpool);

int is_thread_alive(pthread_t tid);

int threadpool_free(threadpool_t *pool);

threadpool_t* cur_pool=NULL;

threadpool_t *threadpool_create(int min_thr_num,int max_thr_num,int queue_max_size)
{
	do
	{
	int i;
	if(cur_pool)
	{
		printf("delete current pool first!\n");
		break;
	}
	if(!(cur_pool=(threadpool_t*)malloc(sizeof(threadpool_t))))
	{
		printf("malloc threadpool fail!\n");
		break;
	}
	cur_pool->min_thr_num = min_thr_num;
	cur_pool->max_thr_num = max_thr_num;
	cur_pool->busy_thr_num = 0;
	cur_pool->live_thr_num = min_thr_num;
	cur_pool->queue_size = 0;
	cur_pool->queue_max_size = queue_max_size;
	cur_pool->queue_front =0;
	cur_pool->queue_rear =0;
	cur_pool->shutdown = false;

	if(!(cur_pool->threads=(pthread_t*)malloc(sizeof(pthread_t)*max_thr_num)))
	{
		printf("malloc threads fail!");
		break;
	}
	if(!(cur_pool->task_queue=(threadpool_task_t*)malloc(sizeof(threadpool_task_t)*queue_max_size)))
	{
		printf("malloc task queue fail!");
		break;
	}

	if(pthread_mutex_init(&(cur_pool->lock),NULL)||pthread_mutex_init(&(cur_pool->thread_counter),NULL)||(pthread_cond_init(&(cur_pool->queue_not_empty),NULL))||pthread_cond_init(&(cur_pool->queue_not_full),NULL))
	{
		printf("init mutex and cond fail!");
		break;
	}

	/*turn on min_thr_num threads*/
	for(int i = 0;i<min_thr_num;i++)
	{
		if(pthread_create(&(cur_pool->threads[i]),NULL,threadpool_thread,(void*)cur_pool))//need to complete
			printf("Thread[i]: pthread_create() fail!\n");
	}
	if(pthread_create(&(cur_pool->adjust_tid),NULL,adjust_thread,(void*)cur_pool))//need to complete
		printf("ajust_thread: pthread_create() fail!\n");
	return cur_pool;
	}while(0);
	threadpool_free(cur_pool);//need to complete
	return NULL;
}

void * threadpool_thread(void*threadpool)
{
	threadpool_t* pool = (threadpool_t*)threadpool;
	threadpool_task_t task;
	while(true)
	{
		pthread_mutex_lock(&(pool->lock));
		while((pool->queue_size==0)&&(!pool->shutdown))
		{
			printf("thread 0x%x is waiting...\n",(unsigned int)pthread_self());
			pthread_cond_wait(&(pool->queue_not_empty),&(pool->lock));
			if(pool->wait_exit_thr_num>0)//????
			{
				pool->wait_exit_thr_num--;
				if(pool->live_thr_num>pool->min_thr_num)
				{
					printf("thread 0x%x is exiting\n",(unsigned int)pthread_self());
					pool->live_thr_num--;
					pthread_mutex_unlock(&(pool->lock));
					pthread_exit(NULL);
				}
			}
		}
		if(pool->shutdown)
		{
			pthread_mutex_unlock(&(pool->lock));
			printf("thread 0x%x is exiting\n",(unsigned int)pthread_self());
			pthread_exit(NULL);
		}
		task.function=pool->task_queue[pool->queue_front].function;
		task.arg = pool->task_queue[pool->queue_front].arg;
		pool->queue_front=(pool->queue_front+1)%pool->queue_max_size;
		pool->queue_size--;
		pthread_cond_broadcast(&(pool->queue_not_full));//???why broadcast

		pthread_mutex_unlock(&(pool->lock));

		printf("thread 0x%x start working\n",(unsigned int)pthread_self());
		pthread_mutex_lock(&(pool->thread_counter));
		pool->busy_thr_num++;
		pthread_mutex_unlock(&(pool->thread_counter));
		(*(task.function))(task.arg);//execute task in cur thread;

		/*deading after compelete execution*/
		printf("thread 0x%x end working\n",(unsigned int)pthread_self());
		pthread_mutex_lock(&(pool->thread_counter));
		pool->busy_thr_num--;
		pthread_mutex_unlock(&(pool->thread_counter));
	}
	pthread_exit(NULL);
	return NULL;
}

void* adjust_thread(void *threadpool)
{
	int i;
	threadpool_t *pool = (threadpool_t*)threadpool;
	while(!pool->shutdown)
	{
		sleep(DEFAULT_TIME);//????
		pthread_mutex_lock(&(pool->lock));
		int queue_size=pool->queue_size;
		int live_thr_num = pool->live_thr_num;
		pthread_mutex_unlock(&(pool->lock));
		pthread_mutex_lock(&(pool->thread_counter));
                int busy_thr_num=pool->busy_thr_num;
                pthread_mutex_unlock(&(pool->thread_counter));
		
		if(queue_size>=MIN_WAIT_TASK_NUM&&live_thr_num<pool->max_thr_num)
		{
			pthread_mutex_lock(&(pool->lock));
			int add = 0;
			for(i=0;i<pool->max_thr_num&&add<DEFAULT_THREAD_VARY&&pool->live_thr_num<pool->max_thr_num;i++)
			{
				if(pool->threads[i]==0||!is_thread_alive(pool->threads[i]))
				{
					pthread_create(&(pool->threads[i]),NULL,threadpool_thread,(void*)pool);
					add++;
					pool->live_thr_num++;
				}
			}
			pthread_mutex_unlock(&(pool->lock));
		}

		if((busy_thr_num*2)<live_thr_num&&live_thr_num>pool->min_thr_num)/*destroy duo yu threads*/
		{
			pthread_mutex_lock(&(pool->lock));//????why not unlock the mutex?
			pool->wait_exit_thr_num=DEFAULT_THREAD_VARY;
			pthread_mutex_unlock(&(pool->lock));
			for(i=0;i<DEFAULT_THREAD_VARY;i++)
			{
				pthread_cond_signal(&(pool->queue_not_empty));
			}
		}
	}
	return NULL;
}

int threadpool_destroy(threadpool_t *pool)
{
	if(cur_pool==NULL)
		return -1;
	int i;
	cur_pool->shutdown=1;
	pthread_join(cur_pool->adjust_tid,NULL);
	pthread_cond_broadcast(&(cur_pool->queue_not_empty));//????
	for(i=0;i<cur_pool->min_thr_num;i++)
	{
		pthread_join(pool->threads[i],NULL);
	}
	threadpool_free(cur_pool);
	return 0;
}

int threadpool_free(threadpool_t*pool)
{
	if(pool==NULL)
	{
		return -1;
	}
	if(pool->task_queue)
		free(pool->task_queue);
	if(pool->threads)
	{
		free(pool->threads);
		pthread_mutex_lock(&(pool->lock));
		pthread_mutex_destroy(&(pool->lock));
		pthread_mutex_lock(&(pool->thread_counter));
		pthread_mutex_destroy(&(pool->thread_counter));
		pthread_cond_destroy(&(pool->queue_not_empty));
		pthread_cond_destroy(&(pool->queue_not_full));
	}
	free(pool);
	pool=NULL;
	cur_pool=NULL;
	return 0;
}

int threadpool_add_task(void*(*function)(void *arg),void* arg)//add task
{
	assert(cur_pool!=NULL);
	assert(function!=NULL);
	assert(arg!=NULL);

	pthread_mutex_lock(&(cur_pool->lock));
	while((cur_pool->queue_size==cur_pool->queue_max_size)&&(!cur_pool->shutdown))
	{
		pthread_cond_wait(&(cur_pool->queue_not_full),&(cur_pool->lock));
	}
	if(cur_pool->shutdown)
	{
		pthread_mutex_unlock(&(cur_pool->lock));
	}
	/*if(cur_pool->task_queue[cur_pool->queue_rear].arg!=NULL)
	{
		free(cur_pool->task_queue[cur_pool->queue_rear].arg);//????Only free arg?
		cur_pool->task_queue[cur_pool->queue_rear].arg=NULL;
	}**/
	cur_pool->task_queue[cur_pool->queue_rear].function = function;
	cur_pool->task_queue[cur_pool->queue_rear].arg = arg;
	cur_pool->queue_rear=(cur_pool->queue_rear+1)%cur_pool->queue_max_size;
	cur_pool->queue_size++;
	pthread_cond_signal(&(cur_pool->queue_not_empty));
	pthread_mutex_unlock(&(cur_pool->lock));
	return 0;	
}

int threadpool_all_threadnum()
{
	int all_threadnum = -1;
	pthread_mutex_lock(&(cur_pool->lock));
	all_threadnum = cur_pool->live_thr_num;
	pthread_mutex_unlock(&(cur_pool->lock));
	return all_threadnum;
}

int threadpool_busy_threadnum()
{
	int busy_threadnum = -1;
        pthread_mutex_lock(&(cur_pool->thread_counter));
        busy_threadnum = cur_pool->busy_thr_num;
        pthread_mutex_unlock(&(cur_pool->thread_counter));
        return busy_threadnum;
}

int is_thread_alive(pthread_t tid)
{
	int kill_rc = pthread_kill(tid,0);
	if(kill_rc==ESRCH)
	{
		return false;
	}
	return true;
}


//test
void *process(void*arg)
{
	printf("thread 0x%x working on task %d\n ",(unsigned int)pthread_self(),*(int*)arg);
	sleep(1);
	printf("task %d is end \n",*(int*)arg);
	return NULL;
}

int main(void)
{
	threadpool_t *thp = threadpool_create(3,100,12);
	printf("pool inited\n");
	int *num = (int*)malloc(sizeof(int)*20);
	int i;
	for(i=0;i<10;i++)
	{
		num[i]=i;
		printf("add task %d\n",i);
		threadpool_add_task(process,(void*)&num[i]);
	}
	sleep(10);
	threadpool_destroy(thp);
}
