#include <stdio.h> // 
#include "esp_log.h" // 
#include "freertos/FreeRTOS.h" // 
#include "freertos/task.h" // 
#include "driver/ledc.h" // 
#include "driver/adc.h" // 
#include "driver/dac.h" // 
#include "esp_adc_cal.h" // 
#include "rom/ets_sys.h" // 
#include "esp_system.h" // 

// 2AC7Z-ESPWROOM32 <- KY-037: G34, 3V3, GND; LM386: G25, GND

// Pines
#define MIC_ADC_CHANNEL ADC1_CHANNEL_6 // GPIO34 KY-037 AO
#define SPEAKER_DAC_CHANNEL DAC_CHANNEL_1 // GPIO25 LM386

void app_main() {
    // Configurar ADC
    adc1_config_width(ADC_WIDTH_BIT_12); // 12 bits
    adc1_config_channel_atten(MIC_ADC_CHANNEL, ADC_ATTEN_DB_11); // Rango 0-3.3V

    // Configurar DAC
    dac_output_enable(SPEAKER_DAC_CHANNEL);

    while (1) {
        // Leer micrófono
        int mic_value = adc1_get_raw(MIC_ADC_CHANNEL); // 0-4095

        // Centrar señal en 128 y amplificar variación
        int centered = mic_value - 2048; // Quitar offset DC
        int amplified = centered * 2; // Ganancia digital (ajustable)
        int dac_value = 128 + amplified / 16; // Ajustar a 0-255

        // Limitar rango del DAC
        if (dac_value > 255) dac_value = 255;
        if (dac_value < 0) dac_value = 0;

        // Enviar a DAC
        dac_output_voltage(SPEAKER_DAC_CHANNEL, dac_value);

        // Retardo ~20 kHz
        ets_delay_us(50);
    }
}
