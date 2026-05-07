#ifndef AUTH_H
#define AUTH_H

void auth_handshake(void);
int auth_loop(void);

extern volatile int g_running;
extern volatile int g_auth_ok;

#endif
