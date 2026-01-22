#ifndef RAOP_RTP_H
#define RAOP_RTP_H

void raop_rtp_start(int control_port, int timing_port);
void raop_rtp_stop(void);
void raop_rtp_flush(void);
void raop_rtp_set_volume(float volume_db);

#endif
