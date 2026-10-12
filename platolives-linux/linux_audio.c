/*
 * PlatoLives - Native SDL2 Audio Subsystem
 * High-fidelity CDC PLATO IV 880 Hz Bell and Buzzer Synthesizer.
 */

#include "linux_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static SDL_AudioDeviceID g_audio_dev = 0;
static int16_t *g_beep_samples = NULL;
static uint32_t g_beep_bytes = 0;

void linux_audio_init(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        printf("[Audio] Inizializzazione SDL_INIT_AUDIO non riuscita: %s (Modalita muta attiva)\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec wanted, obtained;
    memset(&wanted, 0, sizeof(wanted));
    wanted.freq = 44100;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 1;
    wanted.samples = 1024;
    wanted.callback = NULL; /* Modalita non-bloccante SDL_QueueAudio */

    g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
    if (g_audio_dev == 0) {
        printf("[Audio] Impossibile aprire dispositivo audio predefinito: %s (Muto)\n", SDL_GetError());
        return;
    }

    /* Sintesi precalcolata CDC PLATO Bell: 880 Hz, durata 90 ms */
    int sample_rate = obtained.freq;
    float duration_sec = 0.090f;
    int num_samples = (int)(sample_rate * duration_sec);
    g_beep_bytes = (uint32_t)(num_samples * sizeof(int16_t));
    g_beep_samples = (int16_t *)malloc(g_beep_bytes);

    if (g_beep_samples) {
        float f0 = 880.0f; /* Tono A5 PLATO canonico */
        float f1 = 1760.0f; /* Seconda armonica vintage */
        for (int i = 0; i < num_samples; i++) {
            float t = (float)i / (float)sample_rate;
            /* Attacco dolce di 5ms, poi decadimento esponenziale rapido anti-click */
            float env = (t < 0.005f) ? (t / 0.005f) : expf(-18.0f * (t - 0.005f));
            float wave = 0.88f * sinf(2.0f * (float)M_PI * f0 * t) +
                         0.12f * sinf(2.0f * (float)M_PI * f1 * t);
            float sample_val = wave * env * 22000.0f;
            if (sample_val > 32767.0f) sample_val = 32767.0f;
            if (sample_val < -32768.0f) sample_val = -32768.0f;
            g_beep_samples[i] = (int16_t)sample_val;
        }
    }

    SDL_PauseAudioDevice(g_audio_dev, 0);
    printf("[Audio] Sottosistema Audio SDL2 avviato con successo (44.1 kHz, PLATO Buzzer sintetizzato).\n");
}

void linux_audio_beep(void *context) {
    (void)context;
    if (g_audio_dev == 0 || !g_beep_samples) return;

    /* Svuota eventuale audio arretrato per latenza zero e accoda il beep */
    SDL_ClearQueuedAudio(g_audio_dev);
    SDL_QueueAudio(g_audio_dev, g_beep_samples, g_beep_bytes);
}

void linux_audio_destroy(void) {
    if (g_audio_dev != 0) {
        SDL_CloseAudioDevice(g_audio_dev);
        g_audio_dev = 0;
    }
    if (g_beep_samples) {
        free(g_beep_samples);
        g_beep_samples = NULL;
    }
    g_beep_bytes = 0;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}
