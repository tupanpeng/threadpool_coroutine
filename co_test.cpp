#include "co_routine_TP.h"
#include "co_routine_inner_TP.h"
#include<iostream>
using namespace std;

void* func1(void * arg)
{
	int a = *(int*)arg;
	for(int i=0;i<3;i++)
	{
		cout<<a++<<endl;
		co_yield_ct();
	}
}


int main()
{
	//stCoRoutineEnv_t* env = new stCoRoutineEnv_t;
	stCoRoutine_t * cr1,*cr2;
	int a1=5,a2=50;
	co_create(&cr1,NULL,func1,(void*)(&a1));
	co_create(&cr2,NULL,func1,(void*)(&a2));
	co_resume(cr1);
	co_resume(cr2);
	co_resume(cr1);
	co_resume(cr2);
	co_resume(cr1);
	co_resume(cr2);
	return 0;

}
