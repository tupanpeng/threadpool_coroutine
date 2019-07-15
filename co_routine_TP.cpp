//co_routine_TP.cpp
#include<iostream>
#include<string>
#include<cstring>
#include "co_routine_inner_TP.h"
#include "co_routine_TP.h"

extern "C"
{
	extern void coctx_swap(coctx_t *, coctx_t*) asm ("coctx_swap");
}

using namespace std;

void co_swap(stCoRoutine_t* cur, stCoRoutine_t* co)
{
	coctx_swap(&(cur->ctx),&(co->ctx));
}

/*-------------------------------------------co_create()---------------------------------------------*/
static __thread stCoRoutineEnv_t* gCoEnvPerThread = NULL;//independent for each thread

stCoRoutineEnv_t* co_get_curr_thread_env()
{
	return gCoEnvPerThread;
}

int co_create(stCoRoutine_t** ppco, const stCoRoutineAttr_t * attr, pfn_co_routine_t pfn, void *arg)
{
	stCoRoutineEnv_t* pEnv = NULL;

	stCoRoutine_t* pCo = (stCoRoutine_t*)malloc(sizeof(stCoRoutine_t));
	memset(pCo,0,(long)(sizeof(stCoRoutine_t)));
	pCo->pfn = pfn;
	pCo->arg = arg;

	stStackMem_t* stack_mem = (stStackMem_t*)malloc(sizeof(stStackMem_t));
	stack_mem->occupy_co=pCo;
	stack_mem->stack_size=128*1024;
	stack_mem->stack_buffer=(char*)malloc(128*1024);
	stack_mem->stack_bp=stack_mem->stack_buffer+128*1024;
	pCo->stack_mem = stack_mem;
	
	memset(&(pCo->ctx),0,(long)(sizeof(coctx_t)));
	pCo->ctx.ss_sp = stack_mem->stack_buffer;
	pCo->ctx.ss_size = 128*1024;
	pCo->cStart = 0;
	pCo->cEnd = 0;
	pCo->cIsMain = 0;
	pCo->cEnableSysHook = 0;
	pCo->cIsShareStack = 0;

	pCo->save_size = 0;
	pCo->save_buffer = NULL;
	
	cout<<"co_create"<<endl;

	if(!co_get_curr_thread_env())
	{
		pEnv = (stCoRoutineEnv_t*)malloc(sizeof(stCoRoutineEnv_t));
		memset(pEnv,0,(long)(sizeof(stCoRoutineEnv_t)));
		gCoEnvPerThread = pEnv;
	        pEnv->iCallStackSize=0;
		stCoRoutine_t* self = (stCoRoutine_t*)malloc(sizeof(stCoRoutine_t));
		memset(self,0,(long)(sizeof(stCoRoutine_t)));
		
		stStackMem_t* stack_mem1 = (stStackMem_t*)malloc(sizeof(stStackMem_t));
		stack_mem1->occupy_co=pCo;
		stack_mem1->stack_size=128*1024;
		stack_mem1->stack_buffer=(char*)malloc(128*1024);
		stack_mem1->stack_bp=stack_mem1->stack_buffer+128*1024;
		self->stack_mem = stack_mem1;
	
		memset(&(self->ctx),0,(long)(sizeof(coctx_t)));
		self->ctx.ss_sp = stack_mem1->stack_buffer;
		self->ctx.ss_size = 128*1024;
		self->cStart = 0;
		self->cEnd = 0;
		self->cIsMain = 0;
		self->cEnableSysHook = 0;
		self->cIsShareStack = 0;

		self->save_size = 0;
		self->save_buffer = NULL;
		self->cIsMain=1;
		pEnv->pCallStack[pEnv->iCallStackSize++]=self;
		//setEpoll
		self->env=pEnv;	
	}
	pCo->env=gCoEnvPerThread;
	*ppco = pCo;
	return 0;
}
/*********************************************co_create()********************************************/


/*-------------------------------------------co_resume()---------------------------------------------*/
static int CoRoutineFunc(stCoRoutine_t* co, void *)
{
	if(co->pfn)
	{
		co->pfn(co->arg);
	}
	co->cEnd=1;
	stCoRoutineEnv_t *env = co->env;
	co_yield_env(env);
	return 0;

}

void co_resume(stCoRoutine_t* co)
{
	cout<<"in resume"<<endl;
	stCoRoutineEnv_t * pEnv = co->env;
	//cout<<"co_resume"<<pEnv->iCallStackSize<<endl;
	stCoRoutine_t *curco = pEnv->pCallStack[pEnv->iCallStackSize-1];
	//cout<<"co_resume"<<pEnv->iCallStackSize<<endl;
	if(!co->cStart)
	{
		
		coctx_make(&co->ctx,(coctx_pfn_t)CoRoutineFunc,co,0);
		//cout<<"co_resume"<<pEnv->iCallStackSize<<endl;
		co->cStart =1;
	}
	pEnv->pCallStack[pEnv->iCallStackSize++]=co;
	cout<<"co_resume"<<endl;
	co_swap(curco,co);
	cout<<pEnv->iCallStackSize<<" resumed!"<<endl;
}
/*********************************************co_resume()********************************************/


/*-------------------------------------------co_yield_ct()---------------------------------------------*/
void co_yield_env(stCoRoutineEnv_t *env)
{
	stCoRoutine_t *last = env->pCallStack[env->iCallStackSize-2];
	stCoRoutine_t *cur = env->pCallStack[env->iCallStackSize-1];
	env->iCallStackSize--;
	co_swap(cur,last);
}

void co_yield_ct()
{
	co_yield_env(co_get_curr_thread_env());
}
/*********************************************co_yield_ct()********************************************/

