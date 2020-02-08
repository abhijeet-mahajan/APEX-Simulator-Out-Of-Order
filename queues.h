//#ifndef PROJECT_2_QUEUES_H
//#define PROJECT_2_QUEUES_H
//
//typedef struct PhysicalRegistersQueue {
//    int front;
//    int rear;
//    int size;
//    unsigned capacity;
//    int* array;
//}PhysicalRegistersQueue;
//
//int isRegisterQueueFull(struct PhysicalRegistersQueue* queue)
//{  return (queue->size == queue->capacity);  }
//
//int isRegisterQueueEmpty(struct PhysicalRegistersQueue* queue)
//{  return (queue->size == 0); }
//
//void insertRegister(struct PhysicalRegistersQueue* queue, int item) {
//    if (isRegisterQueueFull(queue))
//        return;
//    queue->rear = (queue->rear + 1)%queue->capacity;
//    queue->array[queue->rear] = item;
//    queue->size = queue->size + 1;
//}
//
//int getRegister(struct PhysicalRegistersQueue* queue) {
//    if (isRegisterQueueEmpty(queue))
//        return -1;
//    int item = queue->array[queue->front];
//    queue->front = (queue->front + 1)%queue->capacity;
//    queue->size = queue->size - 1;
//    return item;
//}
//
//int registerQueueFront(struct PhysicalRegistersQueue* queue) {
//    if (isRegisterQueueEmpty(queue))
//        return -1;
//    return queue->array[queue->front];
//}
//
//int registerQueueRear(struct PhysicalRegistersQueue* queue) {
//    if (isRegisterQueueEmpty(queue))
//        return -1;
//    return queue->array[queue->rear];
//}
//
//
//
//
//
//
//
//
//
//#endif //PROJECT_2_QUEUES_H
