#ifndef SWITCHING_HPP
#define SWITCHING_HPP

#include <stddef.h>
#include <stdint.h>

extern int current_a2dp_idx;

void switch_to_a2dp(size_t idx);
void handle_rms_notification();

#endif
