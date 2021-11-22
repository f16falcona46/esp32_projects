#include <Arduino.h>
#include "M5CoreInk.h"

Ink_Sprite InkPageSprite(&M5.M5Ink);

void ButtonTest(const char* str)
{
    InkPageSprite.clear();
    InkPageSprite.drawString(5,5, "RUNNING", &AsciiFont24x48);
    InkPageSprite.drawString(35,59,str);
    InkPageSprite.pushSprite();
    //delay(2000);
}

void setup() {

    M5.begin();
    if( !M5.M5Ink.isInit())
    {
        Serial.printf("Ink Init faild");
    }
    M5.M5Ink.clear();
    delay(1000);
    //creat ink refresh Sprite
    if( InkPageSprite.creatSprite(0,0,200,200,true) != 0 )
    {
        Serial.printf("Ink Sprite creat faild");
    }
    
    InkPageSprite.clear();
    InkPageSprite.drawString(5,5, "RUNNING", &AsciiFont24x48);
    InkPageSprite.pushSprite();
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable((gpio_num_t)BUTTON_UP_PIN, GPIO_INTR_LOW_LEVEL);
    gpio_pullup_en((gpio_num_t)BUTTON_UP_PIN);
    gpio_wakeup_enable((gpio_num_t)BUTTON_DOWN_PIN, GPIO_INTR_LOW_LEVEL);
    gpio_pullup_en((gpio_num_t)BUTTON_DOWN_PIN);
    gpio_wakeup_enable((gpio_num_t)BUTTON_MID_PIN, GPIO_INTR_LOW_LEVEL);
    gpio_pullup_en((gpio_num_t)BUTTON_MID_PIN);
    gpio_wakeup_enable((gpio_num_t)BUTTON_EXT_PIN, GPIO_INTR_LOW_LEVEL);
    gpio_pullup_en((gpio_num_t)BUTTON_EXT_PIN);
    gpio_wakeup_enable((gpio_num_t)BUTTON_PWR_PIN, GPIO_INTR_LOW_LEVEL);
    gpio_pullup_en((gpio_num_t)BUTTON_PWR_PIN);
}

void loop() {
    esp_light_sleep_start();
    if( M5.BtnUP.wasPressed()) ButtonTest("Btn UP Pressed");
    if( M5.BtnDOWN.wasPressed()) ButtonTest("Btn DOWN Pressed");
    if( M5.BtnMID.wasPressed()) ButtonTest("Btn MID Pressed");
    if( M5.BtnEXT.wasPressed()) ButtonTest("Btn EXT Pressed");
    if( M5.BtnPWR.wasPressed()){
        ButtonTest("Btn PWR Pressed");
        M5.M5Ink.powerHVON();
        M5.shutdown();
    }
    M5.update();
}
