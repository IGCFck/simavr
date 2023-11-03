#include "buzzer.h"
#include <stdio.h>
#include "sim_irq.h"
#include <stdlib.h>
#include <portaudio.h>
#include <time.h>
#include <sys/time.h>
#include "avr_ioport.h"

#define SAMPLE_RATE (40000)
#define FRAMES_PER_BUFFER (400)
#define CYCLES_PER_FRAME (400)

void e(PaError err)
{
    if (err != paNoError)
    {
        Pa_Terminate();
        fprintf(stderr, "An error occurred while using the portaudio stream\n");
        fprintf(stderr, "Error number: %d\n", err);
        fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
    }
}

static PaStream *stream;

float block[FRAMES_PER_BUFFER] = {0};
int blockIdx = 0;

int state = 0;

void avrCallback(
    avr_irq_t *irq,
    uint32_t value,
    void *param)
{
    cb_params_t *cb_params = (cb_params_t *)param;

    if (value == 1)
    {
        cb_params->buzzer->state |= (1 << cb_params->pin);
    }
    else
    {
        cb_params->buzzer->state &= ~(1 << cb_params->pin);
    }
}

float last = 0.0f;

unsigned long sampling_callback(
    struct avr_t * avr,
    avr_cycle_count_t when,
    void * param)
{
    const float values[] = {0.0f, 1.0f, -1.0f, 0.0f};
    block[blockIdx] = (last - values[state]) / 2 * (last > values[state] ? 1 : -1);
    last = values[state];
    blockIdx++;
    if (blockIdx >= FRAMES_PER_BUFFER){
       blockIdx = 0;
       Pa_WriteStream(stream, block, FRAMES_PER_BUFFER);
    }

    return when + CYCLES_PER_FRAME;
}

static void io_watcher(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
    state = (v & 0b00110000) >> 4;
}

void buzzer_init(
    avr_t *avr,
    buzzer_t *buzzer,
    const char *name)
{
    printf("Initializing buzzer\n");
    e(Pa_Initialize());
    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice();
    outputParameters.channelCount = 1;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    e(Pa_OpenStream(&stream, NULL, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, NULL, NULL));
    e(Pa_StartStream(stream));
	avr_register_io_write(avr, 35, io_watcher, NULL);
    avr_cycle_timer_register(avr, 0, sampling_callback, buzzer);
}

void buzzer_cleanup()
{
    Pa_Terminate();
    e(Pa_StopStream(stream));
    e(Pa_CloseStream(stream));
}
