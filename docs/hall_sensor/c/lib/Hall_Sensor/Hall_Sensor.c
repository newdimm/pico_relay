#include "Hall_Sensor.h"

void Hall_Sensor()
{
    DEV_Module_Init();
    while(1)
    {
        uint16_t ad_value = adc_read();
        if(DEV_Digital_Read(DOUT_PIN) == 0)
        {
            printf("The Magnet is near!!!\n");
			printf("ad_value:%4.2fV\n",ad_value*3.3/4096);
        }
        else
           printf("The Magnet is far!!!\n");
        DEV_Delay_ms(1000); 
    }

}
