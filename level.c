#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <jack/jack.h>
#include <sndfile.h>

static inline void
__attribute__((noreturn))
fail( char * fmt, ... )
{
  va_list vl;
  va_start( vl, fmt );
  fprintf( stderr, "FAIL: " );
  vfprintf( stderr, fmt, vl );
  fprintf( stderr, "\n" );
  va_end( vl );
  abort();
}

typedef struct
{
  double        in_val;
  jack_port_t * in;
  jack_port_t * out;
} app_t;

static int
app_process( jack_nframes_t nframes, void * _app )
{
  app_t * app = _app;

  float const * in  = jack_port_get_buffer( app->in, nframes );
  float *       out = jack_port_get_buffer( app->out, nframes );

  for( size_t i = 0; i < nframes; ++i ) out[i] = 1.0f;

  double alpha = 0.8; // arbitrary
  for( size_t i = 0; i < nframes; ++i ) {
    double f = (double)in[i] * 1.059;
    app->in_val = alpha * f + (1 - alpha)*app->in_val;
  }

  return 0;
}

int main()
{
  app_t app[1];

  jack_client_t * client = jack_client_open( "level", JackNoStartServer, NULL );
  if( !client ) fail( "Failed to create jack client" );

  jack_port_t * in = jack_port_register( client, "in", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0 );
  if( !in ) fail( "Failed to create input port" );

  jack_port_t * out = jack_port_register( client, "out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );
  if( !out ) fail( "Failed to create output port" );

  app->in  = in;
  app->out = out;

  jack_set_process_callback( client, app_process, app );

  jack_activate( client );

  /* ---- app runs here, waits until report of completion */
  while( true ) {
    usleep( 1e6 );
    printf( "in: %f\n", app->in_val );
  }

  /* ---- */

  jack_deactivate( client );
  jack_client_close( client );
}
