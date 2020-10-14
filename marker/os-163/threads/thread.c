#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"
#include <stdbool.h>

//indicating the thread states
// enum {
// 	READY = 0,
// 	RUNNING = 1,
// 	BLOCKED = 2,
// 	EXITED = 3,
// };

//indicating whether the thread id is occupied
enum {
	READY = 0,
	RUNNING = 1,
	BLOCKED = 2,
	EXITED = 3,
	EMPTY = -1,
};


/*Global Variables*/
struct node* currentThreadNode;
struct queue* ready_queue;
struct queue* exit_queue;
int Tid_list[THREAD_MAX_THREADS];

/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in Lab 3 ... */
};

/* This is the queue structure */
struct queue {
	struct node* head;
};

struct node {
	struct node* next;
	struct thread* node;
};

/* This is the thread control block */
struct thread {
	/* ... Fill this in ... */
	Tid id;
	//int state;
	bool isReturnFromQueue;
	void* stack_initial_addr;
	ucontext_t context;
};

/*All helper functions*/
//assign thread id
int assignID(struct thread* t){
	for (int i = 1; i < THREAD_MAX_THREADS; ++i){
		if (Tid_list[i] == EMPTY){
			t->id = i;
			Tid_list[i] = READY;
			return i;
		}
	}
	return THREAD_NOMORE;
}

void thread_stub(void (*fn) (void*), void *parg){
	fn(parg);
	thread_exit();
	assert(0);
}

//add a node to the end of queue
void enqueue(struct queue* myQueue, struct node* myNode){
	struct node* cur = myQueue->head;
	//if queue is empty, add at head
	if (cur == NULL){
		myQueue->head = myNode;
		return;
	}
	//else, add at end
	while (cur->next != NULL){
		cur = cur->next;
	}
	cur->next = myNode;
}

//delete the first node
struct node* dequeue(struct queue* myQueue){
	struct node* temp = myQueue->head;
	myQueue->head = myQueue->head->next;
	temp->next = NULL;
	return temp;
}

//delete the whole queue
void deleteQueue(struct queue* myQueue){
	while (myQueue->head != NULL){
		struct node* temp = myQueue->head;
		myQueue->head = myQueue->head->next;
		
		//delete the node
		Tid_list[temp->node->id] = EMPTY;
		temp->next = NULL;
		free(temp->node->stack_initial_addr);
		free(temp->node);
		free(temp);
	}
}

/*---------------------------------------------------------------------*/

void
thread_init(void)
{
	/* your optional code here */
	ready_queue = (struct queue*)malloc(sizeof(struct queue));
	exit_queue = (struct queue*)malloc(sizeof(struct queue));

	//set kernel thread
	currentThreadNode = (struct node*)malloc(sizeof(struct node));
	currentThreadNode->node = (struct thread*)malloc(sizeof(struct thread));
	currentThreadNode->node->id = 0;
	//currentThreadNode->node->state = RUNNING;

	//set up thread id list
	Tid_list[0] = RUNNING;
	for (int i = 1; i < THREAD_MAX_THREADS; ++i){
		Tid_list[i] = EMPTY;
	}

	
}

Tid
thread_id()
{
	return currentThreadNode->node->id;
}


Tid
thread_create(void (*fn) (void *), void *parg)
{
	//destroy all exited thread 
	if (exit_queue->head != NULL){
		deleteQueue(exit_queue);
	}

	struct thread* newThread = (struct thread*)malloc(sizeof(struct thread));
	if (newThread == NULL){
		//printf("could not allocate memory to create a thread\n");
		return THREAD_NOMEMORY;
	}

	//assign thread id
	if (assignID(newThread) == THREAD_NOMORE){
		//printf("the thread package cannot create more threads\n");
		return THREAD_NOMORE;
	}

	getcontext(&(newThread->context));

	newThread->context.uc_mcontext.gregs[REG_RIP] = (unsigned long)thread_stub;
	newThread->context.uc_mcontext.gregs[REG_RDI] = (unsigned long)fn;
	newThread->context.uc_mcontext.gregs[REG_RSI] = (unsigned long)parg;

	void* SP = malloc(THREAD_MIN_STACK);
	if (SP == NULL){
		//printf("the thread package could not allocate memory to create a stack of the desired size\n");
		return THREAD_NOMEMORY;
	}

	//save the initial address for memory free
	newThread->stack_initial_addr = SP;

	//stack pointer points to the top, and goes down
	SP += THREAD_MIN_STACK;

	//align to 16 bytes
	SP -= 8;

	newThread->context.uc_mcontext.gregs[REG_RSP] = (unsigned long)SP;
	newThread->context.uc_mcontext.gregs[REG_RBP] = (unsigned long)SP;

	struct node* threadNode = (struct node*)malloc(sizeof(struct node));
	threadNode->node = newThread;
	threadNode->next = NULL;
	//newThread->state = READY;

	enqueue(ready_queue, threadNode);
	return newThread->id;
}

Tid
thread_yield(Tid want_tid)
{
	if (want_tid == THREAD_ANY){

		//if ready queue is empty
		if (ready_queue->head == NULL){
			//printf("there are no more threads\n");
			return THREAD_NONE;
		}

		//else, switch to the head thread
		//set the flag, in case of infinite loop
		currentThreadNode->node->isReturnFromQueue = false;
		//move the head of ready queue to next
		struct node* temp = dequeue(ready_queue);
		//set the return value
		Tid return_id = temp->node->id;
		//update the current thread context
		getcontext(&(currentThreadNode->node->context));
		//check the flag
		if (currentThreadNode->node->isReturnFromQueue){
			currentThreadNode->node->isReturnFromQueue = false;
			return return_id;
		}
		currentThreadNode->node->isReturnFromQueue = true;
		//insert the current thread to ready queue
		//currentThreadNode->node->state = READY;
		Tid_list[currentThreadNode->node->id] = READY;
		enqueue(ready_queue, currentThreadNode);
		//switch to the next thread
		//temp->node->state = RUNNING;
		Tid_list[temp->node->id] = RUNNING;
		currentThreadNode = temp;
		setcontext(&(temp->node->context));
		//return after the above thread is finished
		return return_id;
	} else if (want_tid == THREAD_SELF || want_tid == currentThreadNode->node->id){
		currentThreadNode->node->isReturnFromQueue = false;
		//update the context
		getcontext(&(currentThreadNode->node->context));
		if (currentThreadNode->node->isReturnFromQueue){
			currentThreadNode->node->isReturnFromQueue = false;
			return currentThreadNode->node->id;
		}
		currentThreadNode->node->isReturnFromQueue = true;
		setcontext(&(currentThreadNode->node->context));
		return currentThreadNode->node->id;
	} else if (want_tid < 0 || want_tid >= THREAD_MAX_THREADS || Tid_list[want_tid] == EMPTY){
		//printf("the identifier tid does not correspond to a valid thread\n");
		return THREAD_INVALID;
	} else{
		if (Tid_list[want_tid] != READY){
			//printf("the identifier tid does not correspond to a valid thread\n");
			return THREAD_INVALID;
		}
		//find the object thread
		struct node* cur = ready_queue->head;
		struct node* pre;
		while (cur->node->id != want_tid){
			if (cur->next == NULL){
				return THREAD_INVALID;
			}
			pre = cur;
			cur = cur->next;
		}
		//take out the target from the queue
		struct node* temp = cur;
		if (ready_queue->head == cur){
			//if the target is the head
			ready_queue->head = ready_queue->head->next;
		} else{
			pre->next = temp->next;
		}
		temp->next = NULL;
		//set the return value
		Tid return_id = temp->node->id;
		//set the flag, in case of infinite loop
		currentThreadNode->node->isReturnFromQueue = false;
		//update the current thread context
		getcontext(&(currentThreadNode->node->context));
		//check the flag
		if (currentThreadNode->node->isReturnFromQueue){
			currentThreadNode->node->isReturnFromQueue = false;
			return return_id;
		}
		currentThreadNode->node->isReturnFromQueue = true;
		//insert the current thread to ready queue
		//currentThreadNode->node->state = READY;
		Tid_list[currentThreadNode->node->id] = READY;
		enqueue(ready_queue, currentThreadNode);
		//switch to the next thread
		Tid_list[temp->node->id] = RUNNING;
		//temp->node->state = RUNNING;
		currentThreadNode = temp;
		setcontext(&(temp->node->context));
		//return after the above thread is finished
		return return_id;
	}
	return THREAD_FAILED;
}

void
thread_exit()
{
	//if ready queue is empty
	if (ready_queue->head == NULL){
		//printf("there are no more threads\n");
		//destroy all exited thread 
		deleteQueue(exit_queue);
		exit(0);
	}
	//move the head of ready queue to next
	struct node* temp = dequeue(ready_queue);
	//insert the current thread to exit queue
	Tid_list[currentThreadNode->node->id] = EXITED;
	//currentThreadNode->node->state = EXITED;
	enqueue(exit_queue, currentThreadNode);
	//switch to the next thread
	Tid_list[temp->node->id] = RUNNING;
	//temp->node->state = RUNNING;
	currentThreadNode = temp;
	setcontext(&(temp->node->context));
	return;
}

Tid
thread_kill(Tid tid)
{
	if (tid < 0 || tid >= THREAD_MAX_THREADS || Tid_list[tid] != READY){
		//printf("the identifier tid does not correspond to a valid thread, or is the current thread\n");
		return THREAD_INVALID;
	}
	
	//find the object thread
	struct node* cur = ready_queue->head;
	struct node* pre;
	while (cur->node->id != tid){
		if (cur->next == NULL){
			return THREAD_INVALID;
		}
		pre = cur;
		cur = cur->next;
	}
	//take out the target from the queue
	struct node* temp = cur;
	if (ready_queue->head == cur){
		//if the target is the head
		ready_queue->head = ready_queue->head->next;
	} else{
		pre->next = temp->next;
	}
	temp->next = NULL;
	//delete the node
	free(temp->node->stack_initial_addr);
	free(temp->node);
	free(temp);
	Tid_list[tid] = EMPTY;
	return tid;
	
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	TBD();

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	TBD();
	free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	TBD();
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	TBD();
	return 0;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid)
{
	TBD();
	return 0;
}

struct lock {
	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	TBD();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

struct cv {
	/* ... Fill this in ... */
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	TBD();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}
