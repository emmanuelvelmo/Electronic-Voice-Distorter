#include <stdio.h> // Printf para depuración
#include "freertos/FreeRTOS.h" // Núcleo de FreeRTOS
#include "freertos/task.h" // Tareas y delays
#include "driver/adc.h" // Manejo de ADC (MAX4466)
#include "driver/dac.h" // Manejo de DAC (LM386)
#include "rom/ets_sys.h" // Funciones de bajo nivel (ets_delay_us)
#include <math.h> // Funciones matemáticas para procesamiento de señal

// PLACA Y MÓDULOS
// ESPWROOM32 XX5R69 ← MAX4466: G36 (ADC1_CH0), 3V3, GND
//                   ←   LM386: G25 (DAC1), 5V, GND ← Bocina (3W 4Ω): 5V, GND

// PINES DE LA PLACA
#define MIC_ADC_CHANNEL ADC1_CHANNEL_0 // G36 entrada analógica (MAX4466 OUT)
#define SPEAKER_DAC_CHANNEL DAC_CHANNEL_1 // G25 salida analógica (LM386 IN)

// PARÁMETROS DE DISTORSIÓN DE VOZ
#define FACTOR_AGUDEZ 1.5 // Factor para aumentar frecuencia (1.0 = normal, >1.0 = más agudo)
#define TAMANO_BUFFER 256 // Tamaño del buffer para procesamiento
#define RETARDO_MUESTREO_US 50 // Retardo entre muestras en microsegundos (~20 kHz)
#define AMPLITUD_MAXIMA 255 // Valor máximo para DAC (8 bits)

// VARIABLES GLOBALES PARA PROCESAMIENTO
int16_t buffer_entrada[TAMANO_BUFFER]; // Buffer para muestras de entrada
int16_t buffer_salida[TAMANO_BUFFER]; // Buffer para muestras procesadas
uint32_t indice_buffer = 0; // Índice actual en el buffer
float fase_acumulada = 0.0; // Fase acumulada para modulación
float frecuencia_modulacion = 200.0; // Frecuencia de modulación en Hz para efecto vibrato

// FUNCIONES
// Aplica efecto de voz aguda (pitch shifting simplificado)
int16_t aplicar_efecto_agudo(int16_t muestra_entrada, uint32_t indice_tiempo)
{
    // Aplicar modulación de frecuencia simple (vibrato) para efecto más agudo
    float modulacion = sin(2.0 * M_PI * frecuencia_modulacion * indice_tiempo / 20000.0) * 0.3;
    
    // Escalar frecuencia por factor de agudez con modulación
    float factor_frecuencia = FACTOR_AGUDEZ * (1.0 + modulacion);
    
    // Acumular fase para simular cambio de frecuencia
    fase_acumulada += factor_frecuencia;
    
    if (fase_acumulada >= TAMANO_BUFFER)
    {
        fase_acumulada -= TAMANO_BUFFER;
    }
    
    // Interpolación lineal simple para reconstruir señal
    uint32_t indice_entero = (uint32_t)fase_acumulada;
    float fraccion = fase_acumulada - indice_entero;
    uint32_t siguiente_indice = (indice_entero + 1) % TAMANO_BUFFER;
    
    // Obtener muestras del buffer
    int16_t muestra_actual = buffer_entrada[indice_entero];
    int16_t muestra_siguiente = buffer_entrada[siguiente_indice];
    
    // Interpolar entre muestras
    float muestra_interpolada = muestra_actual + fraccion * (muestra_siguiente - muestra_actual);
    
    // Aplicar ganancia para compensar pérdida de amplitud
    muestra_interpolada *= 1.2;
    
    // Limitar amplitud
    if (muestra_interpolada > 2047)
    {
        muestra_interpolada = 2047;
    }
    
    if (muestra_interpolada < -2048)
    {
        muestra_interpolada = -2048;
    }
    
    return (int16_t)muestra_interpolada;
}

// Procesa buffer completo aplicando efecto de voz aguda
void procesar_buffer_voz()
{
    for (uint32_t i = 0; i < TAMANO_BUFFER; i++)
    {
        // Aplicar efecto de voz aguda a cada muestra
        buffer_salida[i] = aplicar_efecto_agudo(buffer_entrada[i], i);
    }
}

// Normaliza y convierte muestra para DAC
uint8_t preparar_muestra_dac(int16_t muestra_procesada)
{
    // Convertir de 12 bits con signo a 8 bits sin signo
    // Primero eliminar signo: de -2048..2047 a 0..4095
    uint16_t muestra_sin_signo = muestra_procesada + 2048;
    
    // Escalar de 12 bits (0-4095) a 8 bits (0-255)
    uint8_t valor_dac = muestra_sin_signo / 16;
    
    // Limitar rango por seguridad
    if (valor_dac > AMPLITUD_MAXIMA)
    {
        valor_dac = AMPLITUD_MAXIMA;
    }
    
    return valor_dac;
}

// PUNTO DE PARTIDA
void app_main()
{
    // CONFIGURACIONES
    // Configurar ADC
    adc1_config_width(ADC_WIDTH_BIT_12); // Resolución de 12 bits (0 - 4095)
    adc1_config_channel_atten(MIC_ADC_CHANNEL, ADC_ATTEN_DB_11); // Rango completo 0 - 3.3V
    
    // Configurar DAC
    dac_output_enable(SPEAKER_DAC_CHANNEL); // Habilita DAC interno (0 – 255 → 0 – 3.3V)
    
    // Inicializar buffers
    for (uint32_t i = 0; i < TAMANO_BUFFER; i++)
    {
        buffer_entrada[i] = 0;
        buffer_salida[i] = 0;
    }
    
    printf("Iniciando distorsión de voz (efecto agudo)\n");
    printf("Factor de agudez: %.1fx\n", FACTOR_AGUDEZ);
    
    uint32_t contador_muestras = 0;
    uint32_t indice_salida = 0;
    
    // Bucle principal: captura, procesamiento y reproducción
    while (1)
    {
        // Leer señal analógica del MAX4466
        int lectura_adc = adc1_get_raw(MIC_ADC_CHANNEL); // Rango ADC: 0 - 4095
        
        // Convertir a valor con signo (centrado en 2048)
        int16_t muestra_entrada = lectura_adc - 2048;
        
        // Almacenar en buffer circular de entrada
        buffer_entrada[indice_buffer] = muestra_entrada;
        
        // Incrementar índice de buffer
        indice_buffer = (indice_buffer + 1) % TAMANO_BUFFER;
        
        // Cuando el buffer está lleno, procesar
        if (indice_buffer == 0)
        {
            procesar_buffer_voz();
            indice_salida = 0; // Reiniciar índice de salida
        }
        
        // Obtener muestra procesada para salida
        int16_t muestra_procesada;
        
        if (indice_buffer > 0)
        {
            // Usar buffer de salida si está disponible
            if (indice_salida < TAMANO_BUFFER)
            {
                muestra_procesada = buffer_salida[indice_salida];
                indice_salida++;
            }
            else
            {
                // Si no hay muestra procesada, usar entrada directa
                muestra_procesada = muestra_entrada;
            }
        }
        else
        {
            // Si el buffer está vacío, usar entrada directa
            muestra_procesada = muestra_entrada;
        }
        
        // Preparar muestra para DAC
        uint8_t valor_dac = preparar_muestra_dac(muestra_procesada);
        
        // Enviar señal procesada al DAC (LM386)
        dac_output_voltage(SPEAKER_DAC_CHANNEL, valor_dac);
        
        // Incrementar contador de tiempo para modulación
        contador_muestras++;
        
        // Retardo para frecuencia de muestreo constante (~20 kHz)
        ets_delay_us(RETARDO_MUESTREO_US);
        
        // Cambiar frecuencia de modulación periódicamente para efecto más natural
        if (contador_muestras % 10000 == 0)
        {
            frecuencia_modulacion = 180.0 + (rand() % 40); // Entre 180-220 Hz
        }
    }
}
