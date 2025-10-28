## basic_queue
![alt text](../img/basic_queue.png)

## basic_queue01
![alt text](../img/basic_queue01.png)

## basic_queue02
![alt text](../img/basic_queue02.png)

## queuesOverflow
![alt text](../img/queuesOverflow.png)

## queuesNon-blocking
![alt text](../img/queuesNon-blocking.png)

## คำถามทบทวน
1. เมื่อ Queue เต็ม การเรียก xQueueSend จะเกิดอะไรขึ้น?

ตอบ xQueueSend() จะส่งไม่สำเร็จและคืนค่า pdFAIL ซึ่งข้อความถูกทิ้งทันที

2. เมื่อ Queue ว่าง การเรียก xQueueReceive จะเกิดอะไรขึ้น?

ตอบ xQueueReceive() จะไม่มีข้อมูลให้รับและคืนค่า pdFAIL ให้ไปทำงานอื่นต่อได้เลย

3. ทำไม LED จึงกะพริบตามการส่งและรับข้อความ?

ตอบ กระพริบทุกครั้งที่ส่งหรือรับข้อความสำเร็จ เพื่อแสดงสถานะการทำงานของ Queue