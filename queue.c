#include <threads.h>
#include <stdlib.h>
#include <stdio.h>

//------------------------------------------struct definitions--------------------------------------------------

//----------------------data queue-------------------

//data queue nodes: holds data passed in by enqueue(), and points at next nodes in data queue
struct DataNode
{
    void* data;
    struct DataNode* next;
};

//data queue: holds the nodes in the queue and has pointers to head and tail to support FIFO properties
struct DataQueue
{
    struct DataNode* head;
    struct DataNode* tail;
};

//---------------------waiter queue-----------------

//waiter queue node: holds sleeping threads waiting for retrieval of data
struct WaiterNode
{
    cnd_t cond; //each waiter has a private conditional variable to support personal awakening
    void* data; //for direct handing of data
    struct WaiterNode* next;
};
//waiter queue: holds nodes in the waiting queue
struct WaiterQueue
{
    struct WaiterNode* head;
    struct WaiterNode* tail;
};

//-------------------main container------------------
//we need a global container to reference the two queues, hold the lock and the visited counter
struct Container
{
    struct DataQueue data_q;
    struct WaiterQueue waiter_q;
    mtx_t lock;
    size_t visited_count;
};

//------------------------------------------initialization and destruction--------------------------------------------------

//now we can store a static (persistant) reference to that container
static struct Container* container = NULL;

void initQueue(void)
{
    //allocate memory for the global container
    container = (struct Container*)malloc(sizeof(struct Container));

    //initialize data and waiter queue pointers, counter
    container->data_q.head = NULL;
    container->data_q.tail = NULL;
    container->waiter_q.head = NULL;
    container->waiter_q.tail = NULL;
    container->visited_count = 0;
    
    //initialize lock
    mtx_init(&container->lock, mtx_plain);
}

void destroyQueue(void)
{
    if (container != NULL)
    {
        //destroy mutexx
        mtx_destroy(&container->lock);

        //free container memory
        free(container);

        //set the global pointer back to NULL so that initQueue can be called again later
        container = NULL;
    }
}

//------------------------------------------functional methods--------------------------------------------------

void enqueue(void* item)
{
    //lock mutex
    mtx_lock(&container->lock);

    //if there is a sleeping thread in the thread queue, hand the item directly to oldest one
    if (container->waiter_q.head != NULL)
    {
        //get oldest waiter
        struct WaiterNode* old_head = container->waiter_q.head;

        //update waiter queue head
        container->waiter_q.head = old_head->next;
        //in case that the thread was the only one in the queue, update tail to NULL
        if (container->waiter_q.head == NULL)
        {
            container->waiter_q.tail = NULL;
        }
        //give the item to the waiter directly
        old_head->data = item;

        //wake up the waiter
        cnd_signal(&old_head->cond);

        //unlock and return
        mtx_unlock(&container->lock);
        return;
    }
    //in case that the thread queue is empty, create a data node and add it to tail of data queue
    struct DataNode* new_data = (struct DataNode*)malloc(sizeof(struct DataNode));
    new_data->data = item;
    new_data->next = NULL;

    //if the current data queue is empty, new item is head of queue as well as tail
    if (container->data_q.head == NULL)
    {
        container->data_q.head = new_data;
        container->data_q.tail = new_data;
    }
    else
    {
    container->data_q.tail->next=new_data;
    container->data_q.tail = new_data;
    }
    //unlock
    mtx_unlock(&container->lock);
}

void* dequeue(void)
{
    void* ret = NULL;

    //lock mutex
    mtx_lock(&container->lock);

    //in case data queue is not empty, take the item from the head of data queue
    if (container->data_q.head != NULL)
    {
        struct DataNode* old_node = container->data_q.head; //pointer to the old head
        ret = old_node->data;
        container->data_q.head = old_node->next; //move head forward
        if (container->data_q.head == NULL) //in case queue became empty
        {
            container->data_q.tail = NULL;
        }
        container->visited_count++;

        mtx_unlock(&container->lock);

        free(old_node); //can be done outside lock frame (no other participant has access to this pointer)
        return ret;
    }
    //else, create a waiter node on stack and add it to tail of waiting queue, sleep on a private condition
    struct WaiterNode new_waiter;
    cnd_init(&new_waiter.cond);
    new_waiter.next = NULL;

    //add the new waiter thread to tail
    if (container->waiter_q.head == NULL)
    {
        container->waiter_q.head = &new_waiter;
        container->waiter_q.tail = &new_waiter;
    }
    else
    {
        container->waiter_q.tail->next = &new_waiter;
        container->waiter_q.tail = &new_waiter;
    }

    //sleep on the created condition
    cnd_wait(&new_waiter.cond, &container->lock);

    //wake up (lock is re-acquired), cleanup condition variable, unlock and return
    container->visited_count++;
    cnd_destroy(&new_waiter.cond);
    ret = new_waiter.data;
    mtx_unlock(&container->lock);
    
    return ret;
    //no need to free anything here since new_waiter was created on the stack
}

size_t visited(void)
{
    return container->visited_count;
}
