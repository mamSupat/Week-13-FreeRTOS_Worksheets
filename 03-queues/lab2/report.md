## producer_consumer
![alt text](../../img/producer_consumer.png)

## producer_Balanced
![alt text](../../img/producer_Balanced.png)

## producer_MoreProducers
![alt text](../../img/producer_MoreProducers.png)

## producer_FewerConsumers
![alt text](../../img/producer_FewerConsumers.png)

## producer_Priority
![alt text](../../img/producer_Priority.png)

## producer_Graceful
![alt text](../../img/producer_Graceful.png)

## producer_Performance
![alt text](../../img/producer_Performance.png)

## คำถามทบทวน
1. ในทดลองที่ 2 เกิดอะไรขึ้นกับ Queue?

ตอบ Queue เต็มบ่อย เพราะผู้ผลิตเพิ่มขึ้นมากกว่าผู้บริโภค ทำให้สินค้าถูก Drop หลายครั้ง

2. ในทดลองที่ 3 ระบบทำงานเป็นอย่างไร?

ตอบ ระบบเกิดคอขวด ผู้บริโภคทำงานไม่ทัน สินค้าค้างใน Queue จำนวนมาก ประสิทธิภาพลดลง

3. Load Balancer แจ้งเตือนเมื่อไหร่?

ตอบ เมื่อจำนวนสินค้าภายใน Queue เกินค่ากำหนด (เช่น 8 ชิ้นขึ้นไป) ระบบจะแจ้งเตือนว่าเกิดภาวะโหลดสูง