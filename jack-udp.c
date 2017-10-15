#include <stdio.h> /* C99 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include <unistd.h> /* POSIX */
#include <pthread.h>
#include <sndfile.h>

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

//Transmision OK. Lectura y escritura en JACK ports.

//Struct del paquete local d.
typedef struct {
  int buffer_size;                  //Internal buffer size in frames.
  f32 *j_buffer;                    //Internal buffer of 32 bit floats.
  int fd;                           //Socket File descriptor
  struct sockaddr_in address;       //IP address
  int channels;                     //Num channels
  jack_port_t *j_port[MAX_CHANNELS];//Jack ports = Channels
  jack_ringbuffer_t *rb;            //Pointer to a jack ring buffer
  pthread_t c_thread;               //Hilo
  int pipe[2];                      //Interprocess communication pipe. Thread related.
  char *name;                       //Client name. Default is PID.
} jackudp_t;


//Inicializacion del paquete d
static void jackudp_init(jackudp_t *d) {
  d->buffer_size = 4096;
  d->channels = 32;
  d->name = NULL;
}


//Paquete network p.
typedef struct {
  int index;                //Identificador del paquete
  u16 channels;             //Num channels
  u16 frames;               //Num frames
  u8 data[PAYLOAD_BYTES];   //Payload
} packet_t;


//Network byte order
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
void packet_sendto(int fd, packet_t *p, struct sockaddr_in address) {
  packet_hton(p); //Network byte order
  sendto_exactly(fd, (unsigned char *)p, sizeof(packet_t), address); //Network
}

void packet_recv(int fd, packet_t *p, int flags) {
  recv_exactly(fd, (char *)p, sizeof(packet_t), flags); //Network
  packet_ntoh(p); //Network byte order
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

  eprintf("FAILURE.\n");
  FAILURE;
}



//*************************** JACK CLIENTS (LOCAL) ***************************/

/************************************************************ JACK CLIENT SEND*/
// Write data from the JACK input ports to the ring buffer.
//jack_nframes_t n = Frames/Period.
int jackudp_send(jack_nframes_t nframes, void *PTR ) {
  jackudp_t *d = (jackudp_t *) PTR; //Paquete D
  float *in[MAX_CHANNELS];          //Puntero a los Jack ports
  int i, j;                         //Iteradores auxiliares

  //Los punteros in apuntan a lo mismo que los Jack port.
  //Se hace un for para igual cada i a un channel.
  for(i = 0; i < d->channels; i++) {
    in[i] = (float *) jack_port_get_buffer(d->j_port[i], nframes);
  }
  //El buffer del paquete d se llena con la informacion de los Jack ports.
  for(i = 0; i < nframes; i++) {  //Siendo n = 1024 (Frames/Period)
    for(j = 0; j < d->channels; j++) {
      //El j_buffer guarda los datos de cada frame para cada uno
      //de los canales. in[Canal][Frame]
      d->j_buffer[(i*d->channels)+j] = (f32) in[j][i];
    }
  }

  //Comprueba si hay espacio en buffer.
  int bytes_available = (int) jack_ringbuffer_write_space(d->rb);
  int bytes_to_write = nframes * sizeof(f32) * d->channels;
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


/************************************************************ JACK CLIENT RECV*/
// Write data from ring buffer to JACK output ports.
int jackudp_recv (jack_nframes_t nframes, void *PTR)
{
  jackudp_t *d = (jackudp_t *) PTR; //Paquete D
  if(nframes >= d->buffer_size) {
    eprintf("jack-udp recv: JACK buffer size exceeds limit\n");
    return -1;
  }

  //Conversion del RingBuffer a Jack Ports
  //Le dice los punteros out que apunten al mismo sitio que los Jack ports.
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



/************************ THREADS (NETWORK) ******************************/

//SENDER
// Read data from ring buffer and write to udp port. Packets are
// always sent with a full payload.
void *jackudp_send_thread(void *PTR) {
   jackudp_t *d = (jackudp_t *) PTR;     //Paquete interno
   packet_t p;                           //Network package
   p.index = 0;                          //Inicializa el indice a 0
   int localIndex = 0;


   while(1) {
     jack_ringbuffer_wait_for_read(d->rb, PAYLOAD_BYTES, d->pipe[0]);

     localIndex++;
     p.index = localIndex;
     //eprintf("Indice nuevo paquete. (%d) %d\n", p.index, localIndex);
     p.channels = d->channels;
     p.frames = PAYLOAD_SAMPLES / d->channels;

     jack_ringbuffer_read_exactly(d->rb, (char *)&(p.data), PAYLOAD_BYTES);
     packet_sendto(d->fd,  &p, d->address);
   }
   return NULL;
}

//RECEIVER
// Read data from UDP port and write to ring buffer.
void *jackudp_recv_thread(void *PTR) {
  jackudp_t *d = (jackudp_t *) PTR; //Paquete D = Interno
  packet_t p;                       //Paquete P = Network
  int next_packet = -1;

  //Fichero
  FILE *filed;
  if ((filed = fopen ("Test", "w")) == NULL) {
      eprintf("Error opening the file. \n");
      FAILURE;
  }
  else {
      eprintf("File created. \n");
  }

  while(1) {
    //Llama al metodo para recibir 1 paquete de P.
    packet_recv(d->fd, &p, 0);

    //Comprobaciones del indice y numero de canales
    if(p.index != next_packet/* || next_packet != -1*/) {
      eprintf("jack-udp recv: out of order packet "
              "arrival (Esperado, Recibido) (%d, %d)\n",
          next_packet, p.index);
      //FAILURE;
    }
    if(p.channels != d->channels) {
      eprintf("jack-udp recv: channel mismatch packet "
              "arrival (Esperado, Recibido) (%d != %d)\n",
	      p.channels, d->channels);
      FAILURE;
    }

    //Comprueba el espacio que tiene para escribir en el RingBuffer
    int bytes_available = (int) jack_ringbuffer_write_space(d->rb);
    //Si no hay espacio, avisa.
    if(PAYLOAD_BYTES > bytes_available) {
      eprintf("jack-udp recv: buffer overflow (%d > %d)\n",
	      (int) PAYLOAD_BYTES, bytes_available);
    } else {
      jack_ringbuffer_write_exactly(d->rb,
				    (char *) p.data,
				    (size_t) PAYLOAD_BYTES);
      //Fichero
      fprintf(filed, "%d \n", p.index);
    }

    //Actualiza el indice del paquete que debe llegar.
    next_packet = p.index + 1;
  }
  return NULL;
}



/*********************************** MAIN *************************************/
int main (int argc, char **argv) {
  jackudp_t d;
  int c;                    //Opcion del menu.
  int port_n = 57160;       //Puerto comunicacion.
  char *hostname =  NULL;
  jackudp_init(&d);         //Inicializa el paquete interno d
  int recv_mode;            //Bool receiver/sender mode.

  //Switch que elige dependiendo del parametro que le manden.
  while((c = getopt(argc, argv, "b:n:p:h:s:r")) != -1) {
    switch(c) {
    case 'b': //-b  Set the ring buffer size in frames (default=4096).
      d.buffer_size = atoi(optarg);
      break;
    case 'n': //-n  Set the client name (default=\"jack-udp-PID\").
      d.name = optarg;
      break;
    case 'p': //-p  Set the port number (default=57160).
      port_n = atoi (optarg);
      break;
    case 'h': //-h  Set the number of channels (default=2).
      d.channels = atoi (optarg);
      break;
    case 's': //-s  Set the remote addrress to send to (default=\"127.0.0.1\").
      hostname = optarg;
      recv_mode = 0;  //Receiver mode = False.
      break;
    case 'r': //-r  Set receiver mode.\n");
      recv_mode = 1; //Receiver mode = True.
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

  //Crea el socket UDP, default protocol y le devuelve el file descriptor.
  d.fd = socket_udp(0);


  if(recv_mode) { //Receiver mode = 1 = True
    eprintf("Receiver mode selected. \n");
    //void bind_inet(int fd, const char *hostname, int port)
    bind_inet(d.fd, NULL, port_n);
    eprintf("Connected on port: %d\n", port_n);
  } 
  else { //Sender mode
    eprintf("Sender mode selected. \n");
    //void init_sockaddr_in(struct sockaddr_in *name,
    //       const char *hostname,
    //       uint16_t port)
    init_sockaddr_in(&(d.address),
             hostname ? hostname : "127.0.0.1", //Si no hay, direccion por defecto.
		     port_n);
  }

  //Total = Num frames * Num channels * 32 bits
  d.buffer_size *= d.channels * sizeof(f32);
  //xmalloc(). The motto is succeed or die. If it fails to allocate memory,
  //it will terminate your program and print an error message
  d.j_buffer = xmalloc(d.buffer_size);
  //Allocates a ringbuffer data structure of a specified size.
  d.rb = jack_ringbuffer_create(d.buffer_size);
  //The pipe is then used for communication either between the parent or child
  //processes, or between two sibling processes.
  xpipe(d.pipe);
  //Cliente jack
  jack_client_t *client = NULL;

  //Irrelevante ya que solo vamos a usar el que es por defecto.
  //Both cases. Open an external client session with a JACK server.
  if(d.name) {
    client = jack_client_open(d.name,JackNullOption,NULL);
  } 
  else {
    client = jack_client_unique("jack-udp");
  }

  //Sensibilidad de los mensajes de error a mostrar de Jack. Minimo.
  jack_set_error_function(jack_client_minimal_error_handler);
  //Register a function (and argument) to be called if and when the
  //JACK server shuts down the client thread
  jack_on_shutdown(client, jack_client_minimal_shutdown_handler, 0);
  //Tell the Jack server to call @a process_callback whenever there is
  //work be done, passing @a arg as the second argument.
  jack_set_process_callback(client, recv_mode ? jackudp_recv : jackudp_send, &d);
  //Registra los puertos en Jack. Segun el número que sea (channels)
  //y si son output or input dependiendo de sender o receiver.
  jack_port_make_standard(client, d.j_port, d.channels, recv_mode/*, false*/);
  //Activa el cliente del servidor Jack local.
  jack_client_activate(client);

  //Una vez que ha dejado listo el cliente del servidor Jack local
  //crea un hilo para enviar o recibir paquetes dependiendo del
  //modo elegido al inicio. Llama al metodo que le corresponda
  //pasando el argumento d.
  pthread_create(&(d.c_thread),
                 NULL,
                 recv_mode ? jackudp_recv_thread : jackudp_send_thread,
                 &d);

  //The pthread_join() function suspends execution of
  //the calling thread until the target thread terminates
  pthread_join(d.c_thread, NULL);
  //Cierra el socket del tipo UDP creado.
  close(d.fd);
  // The caller must arrange for a call to jack_ringbuffer_free()
  //to release the memory associated with the ringbuffer.
  jack_ringbuffer_free(d.rb);
  //Cierra el cliente jack creado.
  jack_client_close(client);
  //Cierra las communication pipes de los procesos.
  close(d.pipe[0]);
  close(d.pipe[1]);
  //Libera el espacio reservado para el buffer.
  free(d.j_buffer);
  return EXIT_SUCCESS;
}
