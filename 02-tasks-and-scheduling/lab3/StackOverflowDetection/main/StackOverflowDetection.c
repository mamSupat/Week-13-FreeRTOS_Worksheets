#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

#define LED_RUNNING    GPIO_NUM_2
#define LED_READY      GPIO_NUM_4
#define LED_BLOCKED    GPIO_NUM_5
#define LED_SUSPENDED  GPIO_NUM_18
#define LED_WARNING    GPIO_NUM_19   // แจ้งเตือน Stack Overflow

#define BUTTON1_PIN    GPIO_NUM_0
#define BUTTON2_PIN    GPIO_NUM_35

static const char *TAG = "TASK_STATES";

TaskHandle_t state_demo_task_handle = NULL;
TaskHandle_t control_task_handle = NULL;
TaskHandle_t external_delete_handle = NULL;

SemaphoreHandle_t demo_semaphore = NULL;

// ---------------- State name ----------------
const char* state_names[] = {
    "Running","Ready","Blocked","Suspended","Deleted","Invalid"
};
const char* get_state_name(eTaskState s){
    if(s<=eDeleted) return state_names[s];
    return state_names[5];
}

// ---------------- Exercise 1 : State Counter ----------------
volatile uint32_t state_changes[5] = {0};
void count_state_change(eTaskState old_s,eTaskState new_s){
    if(old_s!=new_s && new_s<=eDeleted){
        state_changes[new_s]++;
        ESP_LOGI(TAG,"State change: %s → %s (Count: %d)",
                 get_state_name(old_s),
                 get_state_name(new_s),
                 state_changes[new_s]);
    }
}

// ---------------- Exercise 2 : LED Display ----------------
void update_state_display(eTaskState s){
    gpio_set_level(LED_RUNNING,0);
    gpio_set_level(LED_READY,0);
    gpio_set_level(LED_BLOCKED,0);
    gpio_set_level(LED_SUSPENDED,0);
    switch(s){
        case eRunning:   gpio_set_level(LED_RUNNING,1); break;
        case eReady:     gpio_set_level(LED_READY,1); break;
        case eBlocked:   gpio_set_level(LED_BLOCKED,1); break;
        case eSuspended: gpio_set_level(LED_SUSPENDED,1); break;
        default:
            for(int i=0;i<3;i++){
                gpio_set_level(LED_RUNNING,1);
                gpio_set_level(LED_READY,1);
                gpio_set_level(LED_BLOCKED,1);
                gpio_set_level(LED_SUSPENDED,1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_RUNNING,0);
                gpio_set_level(LED_READY,0);
                gpio_set_level(LED_BLOCKED,0);
                gpio_set_level(LED_SUSPENDED,0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;
    }
}

// ---------------- State Demo Task ----------------
void state_demo_task(void *pv){
    int cycle=0; eTaskState old_s=eRunning;
    while(1){
        cycle++;
        ESP_LOGI(TAG,"=== Cycle %d ===",cycle);

        update_state_display(eRunning);
        count_state_change(old_s,eRunning); old_s=eRunning;
        for(int i=0;i<1000000;i++){ volatile int d=i*2; }

        update_state_display(eReady);
        count_state_change(old_s,eReady); old_s=eReady;
        vTaskDelay(pdMS_TO_TICKS(100));

        update_state_display(eBlocked);
        count_state_change(old_s,eBlocked); old_s=eBlocked;
        if(xSemaphoreTake(demo_semaphore,pdMS_TO_TICKS(2000))==pdTRUE)
            ESP_LOGI(TAG,"Got semaphore! RUNNING again");
        else
            ESP_LOGW(TAG,"Semaphore timeout!");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------------- Ready Demo Task ----------------
void ready_state_demo_task(void *pv){
    while(1){
        ESP_LOGI(TAG,"Ready state demo task running");
        for(int i=0;i<100000;i++){ volatile int d=i; }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// ---------------- Self Delete Task ----------------
void self_deleting_task(void *pv){
    int *life=(int*)pv;
    ESP_LOGI(TAG,"Self-delete task will live %d s",*life);
    for(int i=*life;i>0;i--){
        ESP_LOGI(TAG,"Countdown: %d",i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG,"Self-delete task → DELETED");
    vTaskDelete(NULL);
}

// ---------------- External Delete Task ----------------
void external_delete_task(void *pv){
    int c=0;
    while(1){
        ESP_LOGI(TAG,"External delete task run: %d",c++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------------- Step 3 : Detailed Monitor ----------------
void monitor_task_states(void){
    ESP_LOGI(TAG,"=== DETAILED TASK STATE MONITOR ===");
    TaskHandle_t tasks[]={state_demo_task_handle,control_task_handle,external_delete_handle};
    const char* names[]={"StateDemo","Control","ExtDelete"};
    for(int i=0;i<3;i++){
        if(tasks[i]){
            eTaskState s=eTaskGetState(tasks[i]);
            UBaseType_t p=uxTaskPriorityGet(tasks[i]);
            UBaseType_t st=uxTaskGetStackHighWaterMark(tasks[i]);
            ESP_LOGI(TAG,"%s: State=%s Prio=%d Stack=%d bytes",
                     names[i],get_state_name(s),p,st*sizeof(StackType_t));
        }
    }
}

// ---------------- Control Task ----------------
void control_task(void *pv){
    bool susp=false, ext_del=false; int cyc=0;
    ESP_LOGI(TAG,"Control Task start");
    while(1){
        cyc++;
        // Button 1 Suspend/Resume
        if(gpio_get_level(BUTTON1_PIN)==0){
            vTaskDelay(pdMS_TO_TICKS(50));
            if(!susp){
                ESP_LOGW(TAG,"=== SUSPEND Demo Task ===");
                vTaskSuspend(state_demo_task_handle);
                update_state_display(eSuspended); susp=true;
            }else{
                ESP_LOGW(TAG,"=== RESUME Demo Task ===");
                vTaskResume(state_demo_task_handle); susp=false;
            }
            while(gpio_get_level(BUTTON1_PIN)==0) vTaskDelay(pdMS_TO_TICKS(10));
        }
        // Button 2 Give Semaphore
        if(gpio_get_level(BUTTON2_PIN)==0){
            vTaskDelay(pdMS_TO_TICKS(50));
            ESP_LOGW(TAG,"=== GIVING SEMAPHORE ===");
            xSemaphoreGive(demo_semaphore);
            while(gpio_get_level(BUTTON2_PIN)==0) vTaskDelay(pdMS_TO_TICKS(10));
        }
        // Delete external after 15 s
        if(cyc==150 && !ext_del){
            ESP_LOGW(TAG,"Deleting external task");
            vTaskDelete(external_delete_handle); ext_del=true;
        }
        // Show monitor
        if(cyc%30==0) monitor_task_states();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ---------------- System Monitor ----------------
void system_monitor_task(void *pv){
    char *lb=malloc(1024); char *sb=malloc(1024);
    if(!lb||!sb){ESP_LOGE(TAG,"Alloc fail");vTaskDelete(NULL);}
    while(1){
        ESP_LOGI(TAG,"\n=== SYSTEM MONITOR ===");
        vTaskList(lb);
        ESP_LOGI(TAG,"Name\tState\tPrio\tStack\tNum\n%s",lb);
        vTaskGetRunTimeStats(sb);
        ESP_LOGI(TAG,"\nRuntime Stats:\n%s",sb);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ---------------- Step 2 : Stack Overflow Detection ----------------
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName){
    ESP_LOGE("STACK_OVERFLOW","❌ Task %s stack overflow!",pcTaskName);
    ESP_LOGE("STACK_OVERFLOW","System will restart...");
    for(int i=0;i<20;i++){
        gpio_set_level(LED_WARNING,1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(LED_WARNING,0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    esp_restart();
}

// ---------------- app_main ----------------
void app_main(void){
    ESP_LOGI(TAG,"=== FreeRTOS Task State + Stack Overflow Demo ===");
    gpio_config_t io={
        .mode=GPIO_MODE_OUTPUT,
        .pin_bit_mask=(1ULL<<LED_RUNNING)|(1ULL<<LED_READY)|
                      (1ULL<<LED_BLOCKED)|(1ULL<<LED_SUSPENDED)|
                      (1ULL<<LED_WARNING)
    };
    gpio_config(&io);
    gpio_config_t btn={
        .mode=GPIO_MODE_INPUT,
        .pin_bit_mask=(1ULL<<BUTTON1_PIN)|(1ULL<<BUTTON2_PIN),
        .pull_up_en=1
    };
    gpio_config(&btn);

    demo_semaphore=xSemaphoreCreateBinary();
    if(!demo_semaphore){ESP_LOGE(TAG,"Semaphore fail");return;}

    static int self_delete_time=10;
    xTaskCreate(state_demo_task,"StateDemo",4096,NULL,3,&state_demo_task_handle);
    xTaskCreate(ready_state_demo_task,"ReadyDemo",2048,NULL,3,NULL);
    xTaskCreate(control_task,"Control",3072,NULL,4,&control_task_handle);
    xTaskCreate(system_monitor_task,"Monitor",4096,NULL,1,NULL);
    xTaskCreate(self_deleting_task,"SelfDelete",2048,&self_delete_time,2,NULL);
    xTaskCreate(external_delete_task,"ExtDelete",2048,NULL,2,&external_delete_handle);

    ESP_LOGI(TAG,"All tasks created. Monitoring task states...");
}
