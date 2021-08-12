int net_connect(unsigned char *host, int port, void (*status)(int, char *));
signed int net_receive(void);
void net_send(unsigned char c);
signed int net_get(unsigned char *c);
void net_send_string(unsigned char *s);
void net_disconnect(void);
int net_connected(void);
