#ifndef LINUX_AUDIO_H
#define LINUX_AUDIO_H

#include <stdbool.h>

void linux_audio_init(void);
void linux_audio_beep(void *context);
void linux_audio_destroy(void);

#endif /* LINUX_AUDIO_H */
