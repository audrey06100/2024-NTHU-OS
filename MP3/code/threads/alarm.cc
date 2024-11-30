// alarm.cc
//	Routines to use a hardware timer device to provide a
//	software alarm clock.  For now, we just provide time-slicing.
//
//	Not completely implemented.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "alarm.h"

#include "copyright.h"
#include "main.h"

//----------------------------------------------------------------------
// Alarm::Alarm
//      Initialize a software alarm clock.  Start up a timer device
//
//      "doRandom" -- if true, arrange for the hardware interrupts to
//		occur at random, instead of fixed, intervals.
//----------------------------------------------------------------------

Alarm::Alarm(bool doRandom) {
    timer = new Timer(doRandom, this);
}

//----------------------------------------------------------------------
// Alarm::CallBack
//	Software interrupt handler for the timer device. The timer device is
//	set up to interrupt the CPU periodically (once every TimerTicks).
//	This routine is called each time there is a timer interrupt,
//	with interrupts disabled.
//
//	Note that instead of calling Yield() directly (which would
//	suspend the interrupt handler, not the interrupted thread
//	which is what we wanted to context switch), we set a flag
//	so that once the interrupt handler is done, it will appear as
//	if the interrupted thread called Yield at the point it is
//	was interrupted.
//
//	For now, just provide time-slicing.  Only need to time slice
//      if we're currently running something (in other words, not idle).
//----------------------------------------------------------------------

void Alarm::CallBack() {
    Interrupt *interrupt = kernel->interrupt;
    MachineStatus status = interrupt->getStatus();

    //---------------------------MP3-------------------------------//
    kernel->scheduler->updateLevels();
    if (status != IdleMode) {
        Thread *t = kernel->currentThread;
        int level = t->getLevel();
        double RunningTicks = (double)kernel->stats->totalTicks - t->getStartRunningTick();

        if (level==1) {
            ListIterator<Thread*> iter(kernel->scheduler->getL1());

            while(!iter.IsDone()) { 
                Thread* thread = iter.Item();
                if (thread->getRemainingBurstTicks() < t->getRemainingBurstTicks()) {
                    interrupt->YieldOnReturn();
                    break;
                }
                if (thread->getRemainingBurstTicks() == t->getRemainingBurstTicks()) {
                    if (thread->getID() < t->getID()) interrupt->YieldOnReturn();
                    break;
                }
                iter.Next();
            }
        }

        if (level==2 && !kernel->scheduler->isL1Empty()) {
            interrupt->YieldOnReturn();
        } 
        if (level==3){
            if (!kernel->scheduler->isL1Empty() || !kernel->scheduler->isL2Empty())
                interrupt->YieldOnReturn();
            //cout << "Running Ticks = " << RunningTicks << endl;
            if (RunningTicks >= t->getTQ()) interrupt->YieldOnReturn();
        }

    }
    //---------------------------MP3-------------------------------//
}
