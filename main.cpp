
#include "concurrentqueue.h"
#include "co_task.h"


int main()
{
    moodycamel::ConcurrentQueue<int> queue;
    queue.enqueue(34);

    
}