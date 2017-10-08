#include <stdio.h> /* C99 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include <unistd.h> /* POSIX */
#include <pthread.h>

#include "c-common/byte-order.h"
#include "c-common/failure.h"
#include "c-common/file.h"
#include "c-common/jack-client.h"
#include "c-common/jack-port.h"
#include "c-common/jack-ringbuffer.h"
#include "c-common/memory.h"
#include "c-common/network.h"
#include "c-common/print.h"

#define MAX_CHANNELS      32
#define PAYLOAD_SAMPLES   256
#define PAYLOAD_BYTES     (PAYLOAD_SAMPLES*sizeof(f32))


//jackudp_t = Package from jack ports to ringbuffer
typedef struct
{
  int buffer_size;
  f32 *j_buffer;                    //Jack buffer
  int fd;                           //File descriptor?
  struct sockaddr_in address;       //IP address
  int channels;                     //Num channels
  jack_port_t *j_port[MAX_CHANNELS];//Jack ports = Channels
  jack_ringbuffer_t *rb;            //Ring buffer
  pthread_t c_thread;
  int pipe[2];                      //Comunication pipe
  char *name;                       //Name of what?
} jackudp_t;


//El objeto d con el tamaño del buffer de ??? y el número de canales
static void jackudp_init(jackudp_t *d)
{
  d->buffer_size = 4096;
  d->channels = 2;
  d->name = NULL;
}


//packet_t = Package from ring buffer to UDP port
typedef struct
{
  u32 index;                //Identificador?
  u16 channels;             //Num channels
  u16 frames;               //Num frames?
  u8 data[PAYLOAD_BYTES];   //Payload
} packet_t;


//Network byte order?
void packet_ntoh(packet_t *p)
{
  p->index = ntoh_i32(p->index);
  p->channels = ntoh_i16(p->channels);
  p->frames = ntoh_i16(p->frames);
  u8 *d = p->data;
  u32 i = p->channels * p->frames;
  while(i--) {
    ntoh32_buf(d, d);
    d += 4;
  }
}


//Network byte order
void packet_hton(packet_t *p)
{
  u8 *d = p->data;
  int i = p->channels * p->frames;
  while(i--){
    hton32_buf(d, d);
    d += 4;
  }
  p->index = hton_i32(p->index);
  p->channels = hton_i16(p->channels);
  p->frames = hton_i16(p->frames);
}


//Metodos de enviar y recibir paquetes UDP
void packet_sendto(int fd, packet_t *p, struct sockaddr_in address)
{
  packet_hton(p);
  sendto_exactly(fd, (unsigned char *)p, sizeof(packet_t), address); //Network
}

void packet_recv(int fd, packet_t *p, int flags)
{
  recv_exactly(fd, (char *)p, sizeof(packet_t), flags); //Network
  packet_ntoh(p);
}


/* Read data from UDP port and write to ring buffer. */
//Teoricamente no hace falta tocar de momento.
void *jackudp_recv_thread(void *PTR)
{
  jackudp_t *d = (jackudp_t *) PTR; //Paquete D = Interno
  packet_t p;                       //Paquete P = Network
  int next_packet = -1;

  while(1) {
    packet_recv(d->fd, &p, 0);
    //Comprobaciones del indice y numero de canales
    if(p.index != next_packet && next_packet != -1) {
      eprintf("jack-udp recv: out of order packet arrival (%d, %d)\n",
	      next_packet, (int)p.index);
      //FAILURE;
    }
    if(p.channels != d->channels) {
      eprintf("jack-udp recv: channel mismatch packet arrival (%d != %d)\n",
	      p.channels, d->channels);
      //FAILURE;
    }

    int bytes_available = (int) jack_ringbuffer_write_space(d->rb);
    if(PAYLOAD_BYTES > bytes_available) {
      eprintf("jack-udp recv: buffer overflow (%d > %d)\n",
	      (int) PAYLOAD_BYTES, bytes_available);
    } else {
      jack_ringbuffer_write_exactly(d->rb,
				    (char *) p.data,
				    (size_t) PAYLOAD_BYTES);
    }
    next_packet = p.index + 1;
    next_packet %= INT32_MAX;
  }
  return NULL;
}


/* Write data from ring buffer to JACK output ports. */
//Este se cambia del ring buffer a archivo
int jackudp_recv (jack_nframes_t nframes, void *PTR)
{
  jackudp_t *d = (jackudp_t *) PTR; //Paquete D
  if(nframes >= d->buffer_size) {
    eprintf("jack-udp recv: JACK buffer size exceeds limit\n");
    return -1;
  }

  //Conversion del RingBuffer a Jack Ports
  //Le dice los punteros outq que apunten al mismo sitio que los Jack ports.
  int i, j;
  float *out[MAX_CHANNELS];
  for(i = 0; i < d->channels; i++) {
    out[i] = (float *) jack_port_get_buffer(d->j_port[i], nframes);
  }

  //Comprueba si tiene informacion para el Jack
  int nsamples = nframes * d->channels;
  int nbytes = nsamples * sizeof(f32);
  int bytes_available = (int) jack_ringbuffer_read_space(d->rb);
  //Si no tiene suficiente informacion, reseta los punteros out.
  if(nbytes > bytes_available) {
    eprintf("jack-udp recv: buffer underflow (%d > %d)\n",
	    nbytes, bytes_available);
    for(i = 0; i < nframes; i++) {
      for(j = 0; j < d->channels; j++) {
	out[j][i] = (float) 0.0;
      }
    }

  }
  //Pero si tiene suficiente informacion se la da a Jack ports, mediante
  //los punteros out.
  else {
    jack_ringbuffer_read_exactly(d->rb, (char *)d->j_buffer, nbytes);
    for(i = 0; i < nframes; i++) {
      for(j = 0; j < d->channels; j++) {
            out[j][i] = (float) d->j_buffer[(i * d->channels) + j];
      }
    }
  }
  return 0;
}


/* Read data from ring buffer and write to udp port. Packets are
   always sent with a full payload. */
void *jackudp_send_thread(void *PTR) {
  jackudp_t *d = (jackudp_t *) PTR;
  packet_t p;
  p.index = 0;
  while(1) {
    jack_ringbuffer_wait_for_read(d->rb, PAYLOAD_BYTES, d->pipe[0]);
    p.index += 1;
    p.index %= INT32_MAX;
    p.channels = d->channels;
    p.frames = PAYLOAD_SAMPLES / d->channels;
    jack_ringbuffer_read_exactly(d->rb, (char *)&(p.data), PAYLOAD_BYTES);
    packet_sendto(d->fd,  &p, d->address);
  }
  return NULL;
}


/* Write data from the JACK input ports to the ring buffer. */
//Aqui se cambia de jack input a lectura de archivo
int jackudp_send(jack_nframes_t n, void *PTR ) {
  jackudp_t *d = (jackudp_t *) PTR; //Paquete D
  float *in[MAX_CHANNELS];
  int i, j;

  //Los punteros in apuntan a lo mismo que los Jack port.
  for(i = 0; i < d->channels; i++) {
    in[i] = (float *) jack_port_get_buffer(d->j_port[i], n);
  }
  //El buffer del paquete d se llena con la informacion de los Jack ports.
  for(i = 0; i < n; i++) {
    for(j = 0; j < d->channels; j++) {
      d->j_buffer[(i*d->channels)+j] = (f32) in[j][i];
    }
  }

  //Comprueba si hay espacio en buffer.
  int bytes_available = (int) jack_ringbuffer_write_space(d->rb);
  int bytes_to_write = n * sizeof(f32) * d->channels;
  if(bytes_to_write > bytes_available) {
    eprintf ("jack-udp send: buffer overflow error (UDP thread late)\n");
  } else {
    jack_ringbuffer_write_exactly(d->rb,
				    (char *) d->j_buffer, bytes_to_write );
  }

  char b = 1;
  if(write(d->pipe[1], &b, 1)== -1) {
    eprintf ("jack-udp send: error writing communication pipe.\n");
    FAILURE;
  }
  return 0;
}


//Basicamente un printf con la interfaz del menu.
void jackudp_usage (void)
{
  eprintf("Usage: jack-udp [ options ] mode\n");
  eprintf("   -b  Set the ring buffer size in frames (default=4096).\n");
  eprintf("   -n  Set the client name (default=\"jack-udp-PID\").\n");
  eprintf("   -p  Set the port number (default=57160).\n");
  eprintf("   -h  Set the number of channels (default=2).\n");
  eprintf("   -s  Set the remote addrress to send to (default=\"127.0.0.1\").\n");
  eprintf("   -r  Set receiver mode.\n");
  FAILURE;
}



int main (int argc, char **argv) {
  jackudp_t d;
  int c;
  int port_n = 57160;
  char *hostname =  NULL;
  jackudp_init(&d);
  int recv_mode;

  //Establece las opciones posibles del programa antes de empezar
  while((c = getopt(argc, argv, "b:n:p:h:s:r")) != -1) {
    switch(c) {
    case 'b': //-b  Set the ring buffer size in frames (default=4096).\n");
      d.buffer_size = atoi(optarg);
      break;
    case 'n': //-n  Set the client name (default=\"jack-udp-PID\").\n");
      d.name = optarg;
      break;
    case 'p': //-p  Set the port number (default=57160).\n");
      port_n = atoi (optarg);
      break;
    case 'h': //-h  Set the number of channels (default=2).\n");
      d.channels = atoi (optarg);
      break;
    case 's': //-s  Set the remote addrress to send to (default=\"127.0.0.1\").\n");
      hostname = optarg;
      recv_mode=0;
      break;
    case 'r': //-r  Set receiver mode.\n");
      recv_mode=1;
      break;
    default:
      eprintf ("jack-udp: Illegal option %c.\n", c);
      jackudp_usage ();
      break;
    }
  }

  //Error por demasiados canales
  if(d.channels < 1 || d.channels > MAX_CHANNELS) {
    eprintf("jack-udp: illegal number of channels: %d\n", d.channels);
    FAILURE;
  }

  //int recv_mode = (strcmp(argv[optind], "recv") == 0); //Receiver selected
  d.fd = socket_udp(0);


  if(recv_mode) { //Receiver
    eprintf("Receiver mode selected. \n");
    bind_inet(d.fd, NULL, port_n);
  } 
  else { //Sender
    eprintf("Sender mode selected. \n");
    init_sockaddr_in(&(d.address),
		     hostname ? hostname : "127.0.0.1",
		     port_n);
  }

  d.buffer_size *= d.channels * sizeof(f32);
  d.j_buffer = xmalloc(d.buffer_size);
  d.rb = jack_ringbuffer_create(d.buffer_size);
  xpipe(d.pipe);
  jack_client_t *client = NULL;

  if(d.name) {
    client = jack_client_open(d.name,JackNullOption,NULL);
  } 
  else {
    client = jack_client_unique("jack-udp");
  }

  jack_set_error_function(jack_client_minimal_error_handler);
  jack_on_shutdown(client, jack_client_minimal_shutdown_handler, 0);
  jack_set_process_callback(client, recv_mode ? jackudp_recv : jackudp_send, &d);
  jack_port_make_standard(client, d.j_port, d.channels, recv_mode/*, false*/);
  jack_client_activate(client);
  pthread_create(&(d.c_thread),NULL,recv_mode ? jackudp_recv_thread : jackudp_send_thread,&d);
  pthread_join(d.c_thread, NULL);
  close(d.fd);
  jack_ringbuffer_free(d.rb);
  jack_client_close(client);
  close(d.pipe[0]);
  close(d.pipe[1]);
  free(d.j_buffer);
  return EXIT_SUCCESS;
}
