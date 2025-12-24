#include <stdio.h> // Printf para depuración
#include "freertos/FreeRTOS.h" // Núcleo de FreeRTOS
#include "freertos/task.h" // Tareas y delays
#include "driver/adc.h" // Manejo de ADC (MAX4466)
#include "driver/dac.h" // Manejo de DAC (LM386)
#include "rom/ets_sys.h" // Funciones de bajo nivel (ets_delay_us)

// PLACA Y MÓDULOS
// ESPWROOM32 XX5R69 ← MAX4466: G36 (ADC1_CH0), 3V3, GND
//                   ←   LM386: G25 (DAC1), 5V, GND ← Bocina (3W 4Ω): 5V, GND

// PINES DE LA PLACA
#define MIC_ADC_CHANNEL ADC1_CHANNEL_0 // GPIO36 entrada analógica (MAX4466 OUT)
#define SPEAKER_DAC_CHANNEL DAC_CHANNEL_1 // GPIO25 salida analógica (LM386 IN)

// PUNTO DE PARTIDA
void app_main()
{
    // CONFIGURACIONES
    // Configurar ADC
    adc1_config_width(ADC_WIDTH_BIT_12); // Resolución de 12 bits (0 - 4095)
    adc1_config_channel_atten(MIC_ADC_CHANNEL, ADC_ATTEN_DB_11); // Rango completo 0 - 3.3V
    
    // Configurar DAC
    dac_output_enable(SPEAKER_DAC_CHANNEL); // Habilita DAC interno (0 – 255 → 0 – 3.3V)
    
    // Bucle principal: lectura directa del micrófono y reproducción
    while (1)
    {
        // Leer señal analógica del MAX4466
        int mic_val = adc1_get_raw(MIC_ADC_CHANNEL); // Rango ADC: 0 - 4095

        // Conversión directa de 12 bits a 8 bits para el DAC (4095 / 255 ≈ 16)
        int dac_val = mic_val / 16;

        // Limitar rango del DAC por seguridad
        if (dac_val > 255)
        {
            dac_val = 255;
        }

        if (dac_val < 0)
        {
            dac_val = 0;
        }

        // Enviar señal al DAC (LM386)
        dac_output_voltage(SPEAKER_DAC_CHANNEL, dac_val);

        // Retardo aproximado ~20 kHz
        ets_delay_us(50);
    }
}
