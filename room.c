#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <jack/jack.h>
#include <sndfile.h>

#define MIN( x, y ) ( (x) < (y) ? (x) : (y) )

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
  float const * input_samples;
  float *       output_samples;
  size_t        n_samples;

  jack_port_t * in1;
  jack_port_t * in2;
  jack_port_t * out1;
  jack_port_t * out2;

  bool   running;
  bool   done;
  size_t where;
} app_t;

static void
app_connect( jack_port_id_t a,
             jack_port_id_t b,
             int            connect,
             void*          _app )
{
  static size_t count = 0;
  app_t * app = _app;
  if( !connect ) fail( "disconnects not allowed" );
  count += 1;
  if( count == 4 ) app->running = true;
}

static int
app_process( jack_nframes_t nframes, void * _app )
{
  app_t * app = _app;

  float const * in1  = jack_port_get_buffer( app->in1, nframes );
  float const * in2  = jack_port_get_buffer( app->in2, nframes );

  float *       out1 = jack_port_get_buffer( app->out1, nframes );
  float *       out2 = jack_port_get_buffer( app->out2, nframes );

  // synth 2.1: 0.944 -- g = 1.059
  // synth 2.2: 0.946 -- g = 1.057

  /* memset( out1, 0, nframes*sizeof(float) ); */
  /* memset( out2, 0, nframes*sizeof(float) ); */

  // roughly normalize gain
  // float g = 1.1f;

  if( app->running ) {
    nframes = MIN( nframes, (jack_nframes_t)(app->n_samples-app->where) );
    for( size_t i = 0; i < nframes; ++i ) {
      app->output_samples[ app->where ] = in1[i] * 1.059f;
      out1[i] = app->input_samples[ app->where++ ];

      app->output_samples[ app->where ] = in2[i] * 1.057f;
      out2[i] = app->input_samples[ app->where++ ];
    }
  }

  if( !nframes ) {
    app->running = false;
    app->done = true;
  }

  return 0;
}

static float *
load_input( char const * fname,
            uint64_t *   out_sample_rate,
            size_t *     out_sample_count )
{
  SF_INFO info[1];
  SNDFILE* f = sf_open( fname, SFM_READ, info );
  if( !f ) fail( "Failed to open file %s", fname );

  size_t  frames = (size_t)(info->frames * info->channels);
  float * ret    = malloc( sizeof(float) * frames );

  sf_count_t cnt = sf_readf_float( f, ret, info->frames );
  if( cnt != info->frames ) fail( "Expected %d samples, only got %d", info->frames, cnt );

  sf_close( f );

  *out_sample_rate  = (uint64_t)info->samplerate;
  *out_sample_count = frames;
  printf( "Loaded %zu samples (%zu sample rate)\n", *out_sample_count, *out_sample_rate );

  *out_sample_count = 4096*256;

  return ret;
}

static void
save_output( char const *  fname,
             float const * samples,
             uint64_t      sample_rate,
             size_t        sample_count )
{
  SF_INFO info[1];
  info->frames     = (int)sample_count / 2;
  info->samplerate = (int)sample_rate;
  info->channels   = 2;
  info->format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
  info->sections   = 0;
  info->seekable   = 0;

  SNDFILE* f = sf_open( fname, SFM_WRITE, info );
  if( !f ) fail( "open for write failed: %s", sf_strerror( f ) );

  sf_count_t cnt = sf_writef_float( f, samples, (int)sample_count/2 );
  if( cnt != (int)sample_count/2 ) fail( "failed to write to file" );

  sf_close( f );
}

int main( int     argc,
          char ** argv )
{
  /* char const * fname        = "/nas/data/music/mad-rush-sally-whitwell/01 - Glass, P - Glassworks - 1. Opening.flac"; */
  /* char const * output_fname = "out.wav"; */

  char const * fname        = argv[1];
  char const * output_fname = argv[2];

  uint64_t sample_rate, sample_count;
  app_t    app[1];
  app->input_samples  = load_input( fname, &sample_rate, &sample_count );
  app->output_samples = calloc( sizeof(float), sample_count );
  app->running        = false;
  app->done           = false;
  app->n_samples      = sample_count;
  app->where          = 0;

  jack_client_t * client = jack_client_open( "room", JackNoStartServer, NULL );
  if( !client ) fail( "Failed to create jack client" );

  jack_port_t * in1 = jack_port_register( client, "in1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0 );
  if( !in1 ) fail( "Failed to create input port" );

  jack_port_t * in2 = jack_port_register( client, "in2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0 );
  if( !in2 ) fail( "Failed to create input port" );

  jack_port_t * out1 = jack_port_register( client, "out1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );
  if( !out1 ) fail( "Failed to create output port" );

  jack_port_t * out2 = jack_port_register( client, "out2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );
  if( !out2 ) fail( "Failed to create output port" );

  app->in1  = in1;
  app->in2  = in2;
  app->out1 = out1;
  app->out2 = out2;

  jack_set_port_connect_callback( client, app_connect, app );
  jack_set_process_callback( client, app_process, app );

  jack_activate( client );

  /* ---- app runs here, waits until report of completion */
  while( !__atomic_load_n( &app->done, __ATOMIC_RELAXED ) ) {
    usleep( 100 );
  }

  /* ---- */

  jack_deactivate( client );
  jack_client_close( client );

  save_output( output_fname, app->output_samples, sample_rate, sample_count );
  free( app->output_samples );
  free( (void*)app->input_samples );
}
