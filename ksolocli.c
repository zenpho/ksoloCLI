/*
  by zenpho@zenpho.co.uk - 2026 mar 13

  kzolocli is a commandline interface tool for communicating
  with ksoloti and axoloti hardware __without java editor__
  it is written in c and depends on libusb for communication
 
  kzolocli is very stable and extensively tested with v0.7
  ksoloti core hardware and MacOS 10.7 10.9 10.13 and 10.14
 
  my kludge dev-workflow replaces libusb/examples/xusb.c
  with the contents of this kzolocli.c file such that I may
  simply invoke "$ ./configure & make xusb" then I rename the
  resulting binary with "$ mv .libs/xusb kzolocli"
 
  in daily use I simply start ./kzolocli then plug in my kso
  waiting for a connection to be established before pressing
  'b' key to send any 'xpatch.bin' that I have saved and kept
  (I keep these with DAW project files and MIDI sequences)
 
  NOTE uploading a file named 'start.bin' will replace sd-card
  standalone autostart files (and requires manual hw restart)

  for more information about axoloti and ksoloti platforms
  see github.com/axoloti/axoloti
  and github.com/ksoloti/ksoloti
 
  see also my ksoloti firmware github.com/zenpho/ks1.0.12
*/

#include <libusb.h>  //v1.0.26 is good
#include <stdio.h>   //printf()...
#include <stdlib.h>  //memset()...
#include <string.h>  //strncmp()...
#include <unistd.h>  //posix...
#include <termios.h> //getch()
#include <libgen.h>  //basename()
#include <stdbool.h> //false...

//axoloti ksoloti usb comms
#define AXOLOTI_VID 0x16C0
#define AXOLOTI_PID 0x0444
#define IFACE_NUMBER 2
#define EP_OUT 0x02
#define EP_IN  0x82
#define TIMEOUT_MS 1000

//executable patch code begins at this hw address
#define P_ORIGIN 0x20011000

//commands and keys that activate them
void displayActions( void )
{
  printf( "============  actions  ============\n" );
  printf( "'d' debug    'b' binfile  'p' ping \n" );
  printf( "'v' version  'S' STOP     's' start\n" );
  printf( "'!' re-init  'q' quit     'h' help \n" );
}

//fetch a character without pressing return key
int getch( void )
{
  struct termios oldt,
                 newt;
  int            ch;
  
  tcgetattr( STDIN_FILENO, &oldt );
  newt = oldt;
  newt.c_lflag &= ~( ICANON | ECHO );
  tcsetattr( STDIN_FILENO, TCSANOW, &newt );
  ch = getchar();
  tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
  return ch;
}

//transmit message to axoloti hw "AxoV"
int txRequestAxoV( libusb_device_handle *handle )
{
  int           txCount = 0;
  unsigned int  len     = 4;
  unsigned char pkt[4]  = {'A','x','o','V'};

  int err = libusb_bulk_transfer(
    handle, EP_OUT,
    pkt, len,
    &txCount, TIMEOUT_MS
    );
   if( err )                       return err;
   if( txCount!=(int)sizeof(pkt) ) return LIBUSB_ERROR_IO;
  
   return 0;
}

//transmit message to axoloti hw "Axop"
int txPingAxop( libusb_device_handle *handle )
{
  int           txCount = 0;
  unsigned int  len     = 4;
  unsigned char pkt[4]  = {'A','x','o','p'};

  int err = libusb_bulk_transfer(
    handle, EP_OUT,
    pkt, len,
    &txCount, TIMEOUT_MS
  );
  if( err )                       return err;
  if( txCount!=(int)sizeof(pkt) ) return LIBUSB_ERROR_IO;
  
  return 0;
}

//transmit message to axoloti hw "AxoS"
int txStopAxoS( libusb_device_handle *handle )
{
  int           txCount = 0;
  unsigned int  len     = 4;
  unsigned char pkt[4]  = {'A','x','o','S'};

  int err = libusb_bulk_transfer(
    handle, EP_OUT,
    pkt, len,
    &txCount, TIMEOUT_MS
  );
  if( err )                       return err;
  if( txCount!=(int)sizeof(pkt) ) return LIBUSB_ERROR_IO;
  
  return 0;
}

//transmit message to axoloti hw "Axos"
int txStartAxos( libusb_device_handle *handle )
{
  int           txCount = 0;
  unsigned int  len     = 4;
  unsigned char pkt[4]  = {'A','x','o','s'};

  int err = libusb_bulk_transfer(
    handle, EP_OUT,
    pkt, len,
    &txCount, TIMEOUT_MS
  );
  if( err )                       return err;
  if( txCount!=(int)sizeof(pkt) ) return LIBUSB_ERROR_IO;
  
  return 0;
}

//transmit message to axoloti hw "Axoc"
int txCloseFileAxoc( libusb_device_handle *handle )
{
  int           txCount = 0;
  unsigned int  len     = 4;
  unsigned char pkt[4]  = {'A','x','o','c'};

  int err = libusb_bulk_transfer(
    handle, EP_OUT,
    pkt, len,
    &txCount, TIMEOUT_MS
  );
  if( err )                       return err;
  if( txCount!=(int)sizeof(pkt) ) return LIBUSB_ERROR_IO;
  
  return 0;
}

//display a buffer of bytes in hex and ascii
void hexdump( unsigned char *buf, int len )
{
  if( len < 1 ) return;

  for(int i=0; i<len; i++){
    printf( "%02x ", buf[i] );
    if( 0==((i+1)%8) ) printf( "\n" );
  }
  
  printf("\n");
  
  for(int i=0; i<len; i++){
    printf( "%c", buf[i] );
    if( 0==((i+1)%8) ) printf( "\n" );
  }
}

//receive one single message from axoloti hw
int rxPacketFrom( libusb_device_handle *handle, bool silent )
{
  int           rxCount = 0;        //count received bytes
  unsigned char buf[64];
  unsigned int  pos = 0;
  int           err = 0;
  int           timeout = 50;       //msec
  
  do
  {
    err = libusb_bulk_transfer(     //receive
      handle, EP_IN,
      buf+pos, 64-pos,
      &rxCount, timeout
    );
    pos += rxCount;
    
    if( err == LIBUSB_ERROR_TIMEOUT ) break;
    if( 0==rxCount ) break;
    if( pos >= 63 )  break;
    
    if( !silent )
    { printf( "Received %d bytes (pos %d)\n", rxCount, pos ); }
    
  }while( rxCount );
  
  if( !silent ) hexdump( buf, pos );
  
  if( err ) return err;
  
  return 0;
}

//receive multiple messages frmo axoloti hw
void rxMultiPacketsFrom( libusb_device_handle *handle, bool silent )
{
  int err = 0;
  
  if( !silent ) printf( "Debug rx buffer...\n" );
  for(int i=0; i<20; i++)
  {
    err = rxPacketFrom( handle, silent );
         if( LIBUSB_ERROR_IO==err )      break;
    else if( LIBUSB_ERROR_TIMEOUT==err ) break;
    else if( err ) printf("err %d on index %d\n", err, i);
  }
}

//transmit a fragment message to axoloti hw "AxoW"
//with reception of ack-packet responses
int txFragment(
  libusb_device_handle *handle,
  unsigned char *buf,
  unsigned int bsz,
  unsigned long offset )
{
  int           txCount = 0;
  unsigned char pkt[12];
  int           err = 0;
  
  pkt[0]  = 'A';
  pkt[1]  = 'x';
  pkt[2]  = 'o';
  pkt[3]  = 'W';
  pkt[4]  = (unsigned char)offset;
  pkt[5]  = (unsigned char)(offset >> 8);
  pkt[6]  = (unsigned char)(offset >> 16);
  pkt[7]  = (unsigned char)(offset >> 24);
  pkt[8]  = (unsigned char)bsz;
  pkt[9]  = (unsigned char)(bsz >> 8);
  pkt[10] = (unsigned char)(bsz >> 16);
  pkt[11] = (unsigned char)(bsz >> 24);
  
  err = libusb_bulk_transfer( //transmit AxoW header
    handle, EP_OUT,
    pkt, 12,
    &txCount, TIMEOUT_MS
  );
  if( err )         return err;
  if( txCount!=12 ) return LIBUSB_ERROR_IO;
  
  rxMultiPacketsFrom( handle, true ); //true 'be silent'
  
  err = libusb_bulk_transfer( //transmit buffer data
    handle, EP_OUT,
    buf, bsz,
    &txCount, TIMEOUT_MS
  );
  if( err )          return err;
  if( (unsigned int)txCount!=bsz ) return LIBUSB_ERROR_IO;
  
  rxMultiPacketsFrom( handle, true ); //true 'be silent'
  
  return 0;
}

//transmit a create-file message to axoloti hw "AxoC"
//with reception of ack-packet responses
int txCreateFileStartBin(
  libusb_device_handle *handle,
  unsigned int size )
{
  int           txCount = 0;
  unsigned char pkt[24]; //header + 'start.bin'
  int           err = 0;
  
  pkt[0]  = 'A';
  pkt[1]  = 'x';
  pkt[2]  = 'o';
  pkt[3]  = 'C';
  pkt[4]  = (unsigned char)size;
  pkt[5]  = (unsigned char)(size >> 8);
  pkt[6]  = (unsigned char)(size >> 16);
  pkt[7]  = (unsigned char)(size >> 24);
  pkt[8]  = 0;
  pkt[9]  = 'f';
                 //calculate timestamp
                 // int  d  = ((dy - 1980) * 512) | (dm * 32) | dd;
                 // int  t  = (th * 2048) | (tm * 32) | (ts / 2);
  pkt[10] = 0;   // pkt[10] = d & 0xff;
  pkt[11] = 0;   // pkt[11] =d >> 8;
  pkt[12] = 0;   // pkt[12] =t & 0xff;
  pkt[13] = 0;   // pkt[13] =t >> 8;
                 // int idx = 14;
                 //filename
  pkt[14] = 's'; // for(int j=0; j<filenameLen; j++){
  pkt[15] = 't'; //   pkt[idx++] = filename[j]
  pkt[16] = 'a'; // }
  pkt[17] = 'r'; // pkt[idx] = 0;
  pkt[18] = 't';
  pkt[19] = '.';
  pkt[20] = 'b';
  pkt[21] = 'i';
  pkt[22] = 'n';
  pkt[23] = 0;
  
  err = libusb_bulk_transfer( //transmit AxoA header
    handle, EP_OUT,
    pkt, 24,
    &txCount, TIMEOUT_MS
  );
  if( err )         return err;
  if( txCount!=12 ) return LIBUSB_ERROR_IO;
  
  rxMultiPacketsFrom( handle, true ); //true 'be silent'
  
  return 0;
}

//transmit an append-file message to axoloti hw "AxoA"
//with reception of ack-packet responses
int txAppendFragment(
  libusb_device_handle *handle,
  unsigned char *buf,
  unsigned int bsz )
{
  int           txCount = 0;
  unsigned char pkt[8];
  int           err = 0;
  
  pkt[0] = 'A';
  pkt[1] = 'x';
  pkt[2] = 'o';
  pkt[3] = 'A';
  pkt[4] = (unsigned char)bsz;
  pkt[5] = (unsigned char)(bsz >> 8);
  pkt[6] = (unsigned char)(bsz >> 16);
  pkt[7] = (unsigned char)(bsz >> 24);
  
  err = libusb_bulk_transfer( //transmit AxoA header
    handle, EP_OUT,
    pkt, 8,
    &txCount, TIMEOUT_MS
  );
  if( err )         return err;
  if( txCount!=8 ) return LIBUSB_ERROR_IO;
  
  rxMultiPacketsFrom( handle, true ); //true 'be silent'
  
  err = libusb_bulk_transfer( //transmit buffer data
    handle, EP_OUT,
    buf, bsz,
    &txCount, TIMEOUT_MS
  );
  if( err )          return err;
  if( (unsigned int)txCount!=bsz ) return LIBUSB_ERROR_IO;
  
  rxMultiPacketsFrom( handle, true ); //true 'be silent'
  
  return 0;
}

//read a binary file and transmit to axoloti hw
//with state-machine logic for various behaviours
void readBinFile( libusb_device_handle *handle )
{
  //we read from a local file file...
  FILE *inFile    = NULL;     //set below
  char inFilePath[512];       //match scanf %500s constraint below
  long inFileLen  = 0;        //calculated below
  long inFileOfs = 0;         //also used for hw mem offset calc
  long maxFileLen = 32768;    //constraint reasons unknown, hw mem?
  
  //we transmit file fragments as usb payload...
  unsigned char fragBuf[512]; //match maxFragLen constraint below
  long maxFragLen = 512;      //usb payload size
  
  //usb error state
  int err = 0;                //0=no error, see LIBUSB_ERROR_....
  
  //zero buffers
  for(int i=0; i<512; i++) fragBuf[i] = 0;
  for(int i=0; i<512; i++) inFilePath[i] = 0;
  
  //usage info
  printf( "=========== reminder ===========\n" );
  printf( "\n\n" );
  printf( " files named start.bin replace\n" );
  printf( " and autostart from SDTF CARD\n" );
  printf( "\n\n" );
  printf( "======== bin file choice ========\n" );
  printf( "1. drag-and-drop file\n" );
  printf( "2. confirm path correct\n" );
  printf( "3. press enter when ready\n ");
  printf( "\n\nFilepath: ");
  scanf( "%500s", inFilePath );

  //read the file
  inFile = fopen( inFilePath, "rb" );
  if( 0==inFile ) //abort?
  {
    fprintf( stderr, "Error missing bin file\n" );
    return;
  }
  
  //determine filename matches start.bin
  int startBinMode = true;
  
  //determine and report input file length
  printf( "OK %s ", inFilePath );
  fseek( inFile, 0, SEEK_END );
  inFileLen = ftell( inFile );
  rewind( inFile );
  printf("%ld bytes size...\n", inFileLen );
  
  //stop current patch
  printf( "OK stopping last patch\n" );
  txStopAxoS( handle );
  
  //are we transmitting sdcard /start.bin file?
  startBinMode = false;
  char *baseFilename = basename( inFilePath );
  startBinMode = !strcmp( baseFilename, "start.bin" );
  
  if( startBinMode) printf( "SD Card startup /start.bin \n" );
  else              printf( "Patch upload\n" );
  
  if( startBinMode ) txCreateFileStartBin( handle, inFileLen );
  
  //iterate thru file and transmit fragments to hw
  do
  {
    long readSz = fread( fragBuf, 1, maxFragLen, inFile );
    printf( "%ld bytes of %ld ok (offset %ld)\n",
            readSz, inFileLen, inFileOfs );

    long memOffset = P_ORIGIN + inFileOfs;
    
    if( startBinMode )
    {
      err = txAppendFragment( handle, fragBuf, readSz );
    }
    else
    {
      err = txFragment( handle, fragBuf, readSz, memOffset );
    }
    
    if( err )
    {
      printf( "Tx error %d\n", err );
      break;
    }
    
    inFileOfs += readSz;
  } while( inFileOfs < maxFileLen && inFileOfs < inFileLen );
  
  fclose( inFile );
  
  //are we transmitting sdcard /start.bin file?
  if( startBinMode )
  {
     txCloseFileAxoc( handle );
     printf( "OK - start.bin ready. Stopped patch.\n" );
     printf( "========= restart manually =========\n" );
     printf( "====  to confirm start.bin load  ===\n" );
     printf( "========= restart manually =========\n" );
  }
  else
  {
    if( inFileOfs == inFileLen )
    if( 0==err )
    {
      printf( "OK starting patch\n" );
      txStartAxos( handle );
    }
  }
}

//begin libusb connection to axoloti hw
int connectTo( 
	libusb_context *ctx,
  libusb_device_handle **handle )         //dblptr necessary?
{
  libusb_device **devList = NULL;         //alloc below
  ssize_t       count     = libusb_get_device_list(ctx, &devList);
  if( count < 0 ) return (int)count;      //abort

  int status = LIBUSB_ERROR_NO_DEVICE;    //default return value
  for( ssize_t i=0; i<count; i++ )        //iterate all devices
  {
    libusb_device *dev = devList[i];      //fetch
    struct libusb_device_descriptor desc; //filled below
    if( libusb_get_device_descriptor(dev, &desc) != 0 ) continue;

    if( AXOLOTI_VID==desc.idVendor )      //match vid && pid
    if( AXOLOTI_PID==desc.idProduct )
    {
      int err = libusb_open(dev, handle); //try to open
      if( *handle && 0==err )             //success?
      { 
        status = 0;                       //return value
        break;                            //stop iterating
      }
    }
  }
  libusb_free_device_list(devList, 1);    //cleanup
  return status;
}

int main(void) //(int argc, char **argv) unnecessary
{
  libusb_context       *ctx    = NULL;    //alloc below
  libusb_device_handle *handle = NULL;    //...

  int err = libusb_init(&ctx);
  if( err ) 
  {
   fprintf( stderr, "Bad libusb_init failed: %d\n", err );
   return 1;
  }
 
  err = connectTo( ctx, &handle );
  if ( err || NULL==handle ) 
  {
    fprintf( stderr, "Device not connected\n" );
    libusb_exit( ctx );
    return 1;
  }

  err = libusb_claim_interface( handle, IFACE_NUMBER );
  if( err )
  {
    fprintf( stderr, "Bad libusb_claim_interface(%d) failed: %d\n",
             IFACE_NUMBER, err );
    libusb_close( handle );
    libusb_exit( ctx );
    return 1;
  }

  bool silent = true;                   //true 'be silent'
  printf( "Wait" ); fflush( stdout );
  for(int i=0; i<10; i++)               //junk waiting msgs
  {
    printf( "." ); fflush( stdout );
    rxMultiPacketsFrom( handle, silent );
  }
  printf( "\n" ); fflush( stdout );
  
  displayActions();                     //help text

  //fetch firmwareID, crc, and entrypoint (P_ORIGIN)
  err = txRequestAxoV( handle ); //transmit
  if( err )
  {
    fprintf( stderr, "Tx failed: %d\n", err );
    libusb_release_interface( handle, IFACE_NUMBER );
    libusb_close( handle );
    libusb_exit( ctx );
    return 1;
  }
  
  silent = true;                        //be silent
  while( 1 )                            //breakout on 'q' quit
  {
    err = rxPacketFrom( handle, silent );
    if( err == LIBUSB_ERROR_TIMEOUT )   //timeout receive is ok
    {
      static int inhibitCounter = 0;    //but don't display often
      if( ++inhibitCounter > 10 )
      {
        printf( "Waiting...\n" );
        inhibitCounter = 0;
      }
    }
    else if( err == LIBUSB_ERROR_OVERFLOW ||
             err == LIBUSB_ERROR_PIPE )
    {
      libusb_release_interface( handle, IFACE_NUMBER );
      libusb_close( handle );
      connectTo( ctx, &handle );
      libusb_claim_interface( handle, IFACE_NUMBER );
    }
    else if ( err )
    {
      fprintf( stderr, "Rx error: %d\n", err );
    }
    else                                 //no errors
    {
      displayActions();
    }
    
    char ch = getch();
    if( ch == 'q' ) break;               //'q' quit
    
    if( ch == 'p' ||                     //any of these commands
        ch == 'v' ||
        ch == 'S' ||
        ch == 's' ||
        ch == 'd' ||
        ch == '!' )
    {
        silent = false;                  //enable packet display
    }
    else{ silent = true; }               //otherwise we are silent
    
    //// //// //// //// //// //// //// //// //// //// //// ////
    if( ch == 'h' ) displayActions();        //'h' help
    if( ch == 'b' ) readBinFile( handle );   //'b' binfile
    if( ch == 'p' ) txPingAxop( handle );    //'p' ping
    if( ch == 'v' ) txRequestAxoV( handle ); //'v' version
    if( ch == 'S' ) txStopAxoS( handle );    //'S' STOP
    if( ch == 's' ) txStartAxos( handle );   //'s' start
    
    if( ch == 'd' )                          // 'd' debug
    {
      rxMultiPacketsFrom( handle, silent );
    }
    
    if( ch == '!' )                          // '!' reinit
    {
      libusb_release_interface( handle, IFACE_NUMBER );
      libusb_close( handle );
      connectTo( ctx, &handle );
      libusb_claim_interface( handle, IFACE_NUMBER );
    }
    //// //// //// //// //// //// //// //// //// //// //// ////
  } //breakout on 'q' quit

  //cleanup
  libusb_release_interface( handle, IFACE_NUMBER );
  libusb_close( handle);
  libusb_exit( ctx );
  return 0;
}
