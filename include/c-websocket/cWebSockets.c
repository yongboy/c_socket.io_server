/*******************************************************************

WebSocket Protocol Implementation

+ WebSocket versions:
	- 13
+ Dependencies:
	- sha1.h and sha1.c from http://www.packetizer.com/security/sha1/ (included)
	- base64.h and base64.c (included)
+ Known bugs:
	- WEBSOCKET_generate_handshake: sha.Message_Digest[i] => sha1_part fails if leading zero is found

Author: Marcin Kelar ( marcin.kelar@gmail.com )
*******************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> /* uint8_t */
#include <ctype.h> /* isdigit */
#include <stddef.h> /* size_t */
#include "cWebSockets.h"
#include "md5.h"

int xdigit( char digit ){
	int val;

	if( '0' <= digit && digit <= '9' ) {
		val = digit -'0';
	} else if( 'a' <= digit && digit <= 'f' ) {
		val = digit -'a'+10;
	} else if( 'A' <= digit && digit <= 'F' ) {
		val = digit -'A'+10;
	} else {
		val = -1;
	}

	return val;
}

int xstr2str( char *buf, unsigned bufsize, const char *in ){
	unsigned inlen;
	unsigned i, j;

	if( !in ) {
		return -1;
	}

	inlen = strlen( in );
	if( inlen % 2 != 0 ) {
		return -2;
	}

	for( i = 0; i < inlen; i++ ) {
		if( xdigit( in[i] ) < 0 ) {
			return -3;
		}
	}

	if( !buf || bufsize < inlen / 2 + 1 ) {
		return -4;
	}

	for( i = 0, j = 0; i < inlen; i += 2, j++ ) {
		buf[ j ] = xdigit( in[ i ] ) * 16 + xdigit( in[ i+1 ]);
	}

	buf[ inlen/2 ] = '\0';

	return inlen / 2+1;
}

/*
void REQUEST_get_header_value( const char *data, const char *requested_key )
@data - entire data received with socket
@requested_key - requested key
@dst - pointer to char array where the result will be stored,
@dst_len - size of @dst */
void REQUEST_get_header_value( const char *data, const char *requested_key, char *dst, unsigned int dst_len ) {
	char *src = ( char * )malloc( 65535 * sizeof( char ) );
	char *result_handler;
	char *result;
	char *tmp_header_key;
	int i = 0;

	strncpy( src, data, 65535 );

	tmp_header_key = strstr( ( char* )src, requested_key );
	if( tmp_header_key == NULL ) {
		dst = NULL;
		return;
	}

	result_handler = ( char * )malloc( 1024 * sizeof( char ) );
	result = ( char* )calloc( 256, sizeof( char ) );

	strncpy( result_handler, tmp_header_key, 1024 );
	tmp_header_key = NULL;

	while( ( result[ i ] = result_handler[ i ] ) != '\015' ) {
		if( result_handler[ i ] != '\015' ) {
			 i++;
		}
	}
	result[ i ]= '\0';

	free( result_handler );
	result_handler = NULL;

	strncpy( dst, strstr( result, ": " ) + 2, dst_len );
	free( src );
	src = NULL;
	free( result );
	result = NULL;

}

static uint32_t doStuffToObtainAnInt32(const char *key)
{
	char res_decimals[15] = "";
	char *tail_res = res_decimals;
	uint8_t space_count = 0;
	uint8_t i = 0;
	do {
		if (isdigit(key[i]))
			strncat(tail_res++, &key[i], 1);
		if (key[i] == ' ')
			space_count++;
	} while (key[++i]);

	return ((uint32_t) strtoul(res_decimals, NULL, 10) / space_count);
}

int WEBSOCKET_generate_handshake_76(const char *url, const char *data, char *dst, const unsigned int dst_len ) {
	char origin[ 512 ];
	char host[ 512 ];
	char additional_headers[ 2048 ];

	REQUEST_get_header_value( data, "Origin: ", origin, 512);
	REQUEST_get_header_value( data, "Host: ", host, 512);

	if( origin != NULL && host != NULL ) {
		sprintf( additional_headers, "Sec-WebSocket-Origin: %s\r\nSec-WebSocket-Location: ws://%s%s\r\n", origin, host, url);
	} else {
		sprintf( additional_headers, "Sec-WebSocket-Origin: %s\r\nSec-WebSocket-Location: %s\r\n", "null", "null" );
	}

	char key1_ori[50];
	char key2_ori[50];
	REQUEST_get_header_value( data, "Sec-WebSocket-Key1: ", key1_ori, 50);
	REQUEST_get_header_value( data, "Sec-WebSocket-Key2: ", key2_ori, 50);


	char key3[8];
	memcpy(key3, data + (strlen(data) - 8), 8);


	char chrkey1[4];
	char chrkey2[4];
	int key1 = doStuffToObtainAnInt32(key1_ori);
	int key2 = doStuffToObtainAnInt32(key2_ori);
	char i;
	for (i = 0; i < 4; i++)
		chrkey1[i] = key1 << (8 * i) >> (8 * 3);
	for (i = 0; i < 4; i++)
		chrkey2[i] = key2 << (8 * i) >> (8 * 3);

	char raw_md5[16];
	char keys[16];
	memcpy(keys, chrkey1, 4);
	memcpy(&keys[4], chrkey2, 4);
	memcpy(&keys[8], key3, 8);
	md5(raw_md5, keys, sizeof (keys)*8);

	snprintf( dst, dst_len, WEBSOCKET_HANDSHAKE_RESPONSE_76, additional_headers);
	int output_len = strlen(dst);
	memcpy(dst + output_len, raw_md5, 16);

	return output_len + 16;
}

/*
void WEBSOCKET_generate_handshake( const char *data, char *dst, unsigned int dst_len )
@data - entire data received with socket
@dst - pointer to char array where the result will be stored
@dst_len - size of @dst */
void WEBSOCKET_generate_handshake( const char *data, char *dst, const unsigned int dst_len ) {
	char origin[ 512 ];
	char host[ 512 ];
	char additional_headers[ 2048 ];
	char sec_websocket_key[ 512 ];
	char sec_websocket_key_sha1[ 512 ];
	char sha1_part[ 32 ];
	SHA1Context sha;
	unsigned char sha1_hex[ 512 ];
	unsigned char sha1_tmp[ 512 ];
	unsigned char sec_websocket_accept[ 512 ];
	int source_len;
	int i;

	memset( sha1_hex, '\0', 512 );
	memset( sha1_tmp, '\0', 32 );
	memset( sec_websocket_accept, '\0', 512 );

	REQUEST_get_header_value( data, "Origin:", origin, 512 );
	REQUEST_get_header_value( data, "Host:", host, 512 );

	if( origin != NULL && host != NULL ) {
		sprintf( additional_headers, "Origin: %s\r\nHost: %s", origin, host );
	} else {
		sprintf( additional_headers, "Origin: %s\r\nHost: %s", "null", "null" );
	}

	REQUEST_get_header_value(data, WEBSOCKET_KEY_HEADER, sec_websocket_key, 512 );
	if( sec_websocket_key == NULL ) {
		dst = NULL;
		return;
	}

	strncat( sec_websocket_key, WEBSOCKET_MAGIC_STRING, 512 );

	SHA1Reset( &sha );
	SHA1Input( &sha, ( const unsigned char * ) sec_websocket_key, strlen( sec_websocket_key ) );
	SHA1Result( &sha );

	for( i = 0; i < 5; i++ ) {
		snprintf( sha1_part, 32, "%x", sha.Message_Digest[i] );
		strncat( sha1_tmp, sha1_part, 512 );
	}

	strncpy( sec_websocket_key_sha1, sha1_tmp, 512 );
	source_len = xstr2str( sha1_hex, 512, sec_websocket_key_sha1 );
	base64_encode( sha1_hex, source_len - 1, sec_websocket_accept, 512 );

	snprintf( dst, dst_len, WEBSOCKET_HANDSHAKE_RESPONSE, additional_headers, sec_websocket_accept );

}

/*
int WEBSOCKET_set_content( const char *data, int data_length, unsigned char *dst )
@data - entire data received with socket
@data_length - size of @data
@dst - pointer to char array where the result will be stored
@dst_len - size of @dst
@return - WebSocket frame size */
int WEBSOCKET_set_content( const char *data, int data_length, unsigned char *dst, const unsigned int dst_len ) {
	unsigned char *message = ( unsigned char * )malloc( 65535 * sizeof( char ) );
	int i;
	int data_start_index;

	message[0] = 129;

	if( data_length <= 125 ) {
		message[1] = ( unsigned char )data_length;
		data_start_index = 2;
	} else if( data_length > 125 && data_length <= 65535 ) {
		message[1] = 126;
		message[2] = ( unsigned char )( ( data_length >> 8 ) & 255 );
		message[3] = ( unsigned char )( ( data_length ) & 255 );
		data_start_index = 4;
	} else {
		message[1] = 127;
		message[2] = ( unsigned char )( ( data_length >> 56 ) & 255 );
		message[3] = ( unsigned char )( ( data_length >> 48 ) & 255 );
		message[4] = ( unsigned char )( ( data_length >> 40 ) & 255 );
		message[5] = ( unsigned char )( ( data_length >> 32 ) & 255 );
		message[6] = ( unsigned char )( ( data_length >> 24 ) & 255 );
		message[7] = ( unsigned char )( ( data_length >> 16 ) & 255 );
		message[8] = ( unsigned char )( ( data_length >> 8 ) & 255 );
		message[9] = ( unsigned char )( ( data_length ) & 255 );
		data_start_index = 10;
	}

	for( i = 0; i < data_length; i++ ) {
		message[ data_start_index + i ] = ( unsigned char )data[i];
	}

	for( i = 0; i < data_length+ data_start_index; i++ ) {
		dst[i] = ( unsigned char )message[ i ];
	}

	if( message ) {
		free( message );
		message = NULL;
	}

	return i;
}

/*
int WEBSOCKET_get_content( const char *data, int data_length, unsigned char *dst )
@data - entire data received with socket
@data_length - size of @data
@dst - pointer to char array, where the result will be stored
@return - size of @dst */
int WEBSOCKET_get_content( const char *data, int data_length, unsigned char *dst, const unsigned int dst_len ) {
	unsigned int i, j;
	unsigned char mask[4];
	unsigned int packet_length = 0;
	unsigned int length_code = 0;
	int index_first_mask = 0;
	int index_first_data_byte = 0;

	if( ( unsigned char )data[0] != 129 ) {
		dst = NULL;
		if( ( unsigned char )data[0] == 136 ) {
			/* WebSocket client disconnected */
			return -2;
		}
		/* Unknown error */
		return -1;
	}

	length_code = ((unsigned char) data[1]) & 127;

	if( length_code <= 125 ) {
		index_first_mask = 2;

		mask[0] = data[2];
		mask[1] = data[3];
		mask[2] = data[4];
		mask[3] = data[5];
	} else if( length_code == 126 ) {
		index_first_mask = 4;

		mask[0] = data[4];
		mask[1] = data[5];
		mask[2] = data[6];
		mask[3] = data[7];
	} else if( length_code == 127 ) {
		index_first_mask = 10;

		mask[0] = data[10];
		mask[1] = data[11];
		mask[2] = data[12];
		mask[3] = data[13];
	}

	index_first_data_byte = index_first_mask + 4;

	packet_length = data_length - index_first_data_byte;

	for( i = index_first_data_byte, j = 0; i < data_length; i++, j++ ) {
		dst[ j ] = ( unsigned char )data[ i ] ^ mask[ j % 4];
	}

	return packet_length;
}

/*
short WEBSOCKET_valid_connection( const char *data )
@data - entire data received with socket
@return - 0 = false / 1 = true */
short WEBSOCKET_valid_connection( const char *data ) {
	char *connection_header = ( char * )malloc( 64 * sizeof( char ) );
	short result = 0;

	REQUEST_get_header_value( data, "Connection:", connection_header, 64 );

	if( connection_header == NULL ) {
		return 0;
	}

	result = ( strstr( data, WEBSOCKET_KEY_HEADER ) != NULL && ( strstr( connection_header, "Upgrade" ) != NULL || strstr( connection_header, "upgrade" ) != NULL) );

	if( connection_header ) {
		free( connection_header );
		connection_header = NULL;
	}

	return result;
}

/*
int WEBSOCKET_client_version( const char *data )
@data - entire data received with socket
@return - value from client's Sec-WebSocket-Version key */
int WEBSOCKET_client_version( const char *data ) {
	char *version_header = ( char * )malloc( 32 * sizeof( char ) );
	int result;

	REQUEST_get_header_value( data, "Sec-WebSocket-Version:", version_header, 32 );

	if( version_header == NULL ) {
		return -1;
	}

	result = atoi( version_header );

	if( version_header ) {
		free( version_header );
		version_header = NULL;
	}

	return result;
}
