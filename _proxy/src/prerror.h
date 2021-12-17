#ifndef PRERROR_H
#define PRERROR_H

// desctructors
#define E_DESTROY -1
#define S_DESTROY 0
// cache-put
#define E_NOSPACE -1
#define S_CHECK 0
// connections
#define E_RESOLVE -2
#define E_CONNECT -1
#define S_CONNECT 1

#define E_WMETHOD -5 // not get incoming, dropping reactant
#define E_NULL -4 //one of values is null
#define E_IO -3 // io error
#define E_PARSE -2 
#define E_BIGREQ -1 // request >  bufsize
#define S_PARSE 0

#define NO_SOCK -1

#define E_FLAG -19 //exiting by flag

#define E_COMPARE -1

#define E_SEND -6
#define S_SEND 1

/**
 *
 * Control defines (mb migrate to params?)
 *
 */

#define MTCLASS 32
#define WTCLASS 33

// Cache init:
#define CHUNK_SIZE_BYTES 4096
#define MAX_SIZE_BYTES 10*4096*1024
#define DEFAULT_THRESHOLD 75

// Client read:
#define CHUNKS_TO_READ 10



#endif