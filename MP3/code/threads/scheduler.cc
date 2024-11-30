// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "scheduler.h"

#include "copyright.h"
#include "debug.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler() {
    //readyList = new List<Thread *>;
    toBeDestroyed = NULL;

    //MP3
    L1 = new List<Thread *>;
    L2 = new List<Thread *>;
    L3 = new List<Thread *>;
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler() {
    //delete readyList;

    //MP3
    delete L1;
    delete L2;
    delete L3;
}

//------------------------------MP3----------------------------//
void Scheduler::updateLevel(List<Thread*>* list, int level){

    ListIterator<Thread*> iter(list); //iter points at the first thread.
    
    while(!iter.IsDone()) {
        Thread* thread = iter.Item();
        
        thread->updatePriority();
        int priority = thread->getPriority();
        if(level == 3 && priority > thread->MaxPriority[2]){
            popQueue(thread, L3, 3);
            pushQueue(thread, L2, 2);
        }
        else if(level == 2 && priority > thread->MaxPriority[1]) {
            popQueue(thread, L2, 2);
            pushQueue(thread, L1, 1);
        }
        iter.Next();
    }
}

void Scheduler::updateLevels() {
    
    updateLevel(L1, 1);
    updateLevel(L2, 2);
    updateLevel(L3, 3);
}

Thread* Scheduler::popQueue(Thread* t, List<Thread*>* poplist, int level) {
    DEBUG(dbgSche, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread ["
        << t->getID() << "] is removed from queue L[" << level << "]");
    poplist->Remove(t);
    return t;
}

void Scheduler::pushQueue(Thread* t, List<Thread*>* pushlist, int level) {
    DEBUG(dbgSche, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread ["
        << t->getID() << "] is inserted into queue L[" << level << "]");
    pushlist->Append(t);
}
//------------------------------MP3----------------------------//

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun(Thread *thread) {
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    // cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    //readyList->Append(thread);

    //---------------------------MP3-------------------------------//
    int priority = thread->getPriority();
    if (priority <= thread->MaxPriority[2]) {
        pushQueue(thread, L3, 3);
    } else if (priority <= thread->MaxPriority[1]) {
        pushQueue(thread, L2, 2);
    } else {
        pushQueue(thread, L1, 1);
    }
    thread->setReadyTick((double)kernel->stats->totalTicks);
    //---------------------------MP3-------------------------------//
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun() {
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    /*if (readyList->IsEmpty()) {
        return NULL;
    } else {
        return readyList->RemoveFront();
    }*/

    //---------------------------MP3-------------------------------//
    if (!L1->IsEmpty()) {
        ListIterator<Thread*> iter(L1);
        
        Thread *minBurstThread = NULL;
        for (minBurstThread=iter.Item(); !iter.IsDone(); iter.Next()) {
            Thread *thread = iter.Item();
            if (thread->getRemainingBurstTicks() < minBurstThread->getRemainingBurstTicks()) {
                minBurstThread = thread;
            } else if (thread->getRemainingBurstTicks() == minBurstThread->getRemainingBurstTicks()) {
                if (thread->getID() < minBurstThread->getID()) minBurstThread = thread;
            }
        }
        return popQueue(minBurstThread, L1, 1);

    } else if (!L2->IsEmpty()) {
        ListIterator<Thread*> iter(L2);
        
        Thread *maxPriorityThread = NULL;
        for (maxPriorityThread=iter.Item(); !iter.IsDone(); iter.Next()) {
            Thread *thread = iter.Item();
            if (thread->getPriority() > maxPriorityThread->getPriority()) {
                maxPriorityThread = thread;
            } else if (thread->getPriority() == maxPriorityThread->getPriority()) {
                if (thread->getID() < maxPriorityThread->getID()) maxPriorityThread = thread;
            }
        }
        return popQueue(maxPriorityThread, L2, 2);

    } else if (!L3->IsEmpty()) {
        return popQueue(L3->Front(), L3, 3);

    } else return NULL;
    //---------------------------MP3-------------------------------//
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void Scheduler::Run(Thread *nextThread, bool finishing) {
    Thread *oldThread = kernel->currentThread;

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {  // mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
        toBeDestroyed = oldThread;
    }

    if (oldThread->space != NULL) {  // if this thread is a user program,
        oldThread->SaveUserState();  // save the user's CPU registers
        oldThread->space->SaveState();
    }

    oldThread->CheckOverflow();  // check if the old thread
                                 // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running

    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    //---------------------------MP3-------------------------------//
    nextThread->setStartRunningTick(kernel->stats->totalTicks);

    DEBUG(dbgSche, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread ["
        << nextThread->getID() << "] is now selected for execution, thread ["
        << oldThread->getID() << "] is replaced, and it has executed ["
        << oldThread->getTotalBurstTicks() << "] ticks")
    
    SWITCH(oldThread, nextThread);

    oldThread->setStartRunningTick(kernel->stats->totalTicks);
    //---------------------------MP3-------------------------------//

    // we're back, running oldThread

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();  // check if thread we were running
                           // before this one has finished
                           // and needs to be cleaned up

    if (oldThread->space != NULL) {     // if there is an address space
        oldThread->RestoreUserState();  // to restore, do it.
        oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void Scheduler::CheckToBeDestroyed() {
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
        toBeDestroyed = NULL;
    }
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void Scheduler::Print() {
    //cout << "Ready list contents:\n";
    //readyList->Apply(ThreadPrint);

    //MP3
    cout << "L1 contents:\n";
    L1->Apply(ThreadPrint);
    cout << "L2 contents:\n";
    L2->Apply(ThreadPrint);
    cout << "L3 contents:\n";
    L3->Apply(ThreadPrint);
}
