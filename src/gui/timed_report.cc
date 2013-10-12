#include "timed_report.hh"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

void* TimedReport::TimedThread(void* arg){
	TimedReport *This = static_cast<TimedReport*>(arg);
	while(1){
		This->SendReport();
		This->Followup();
		usleep(This->_Period*1000);
		if(This->terminated_)
			break;
	}
	return NULL;
}

void TimedReport::start(){
	//pthread_create(&_ThreadID,NULL,TimedThread,(void*)this);
}

TimedReport::TimedReport(unsigned int period){
	terminated_ = false;
	_Period = period;
}

TimedReport::~TimedReport(){
	terminated_ = true;
	pthread_join(_ThreadID,NULL);
}
