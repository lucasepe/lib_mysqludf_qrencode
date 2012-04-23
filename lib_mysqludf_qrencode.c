/*
 * lib_mysqludf_qrencode.c:  MySQL UDF for QRCode generation.
 *
 * Copyright (C) 2011 by Luca Sepe ( luca.sepe@gmail.com )
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *	
 *
 * COMMENTS ON USING THIS CODE:
 *
 * Dependencies:
 * 
 * 	libpng		: http://www.libpng.org/pub/png/libpng.html
 * 	libqrencode	: http://fukuchi.org/works/qrencode/index.en.html
 *
 * To compile:
 *
 *	gcc -shared -o lib_mysqludf_qrencode.so lib_mysqludf_qrencode.c \
 *		$(mysql_config --cflags) -lpng -lqrencode -DMYSQL_DYNAMIC_PLUGIN
 *
 *
 *	returns the qrcode of specified strings
 *
 * 		input parameters:
 * 			text (string)
 *
 * 		output:
 * 			PNG QRCode image (blob)
 *
 *
 * To register these functions:
 *
 * 		CREATE FUNCTION qrencode RETURNS STRING SONAME 'lib_mysqludf_qrencode.so';
 *		CREATE FUNCTION qrencode_info RETURNS STRING SONAME 'lib_mysqludf_qrencode.so';
 *
 * To get rid of the functions:
 *
 * 		DROP FUNCTION qrcode;
 *		DROP FUNCTION qrcode_info;
 *
 * To list all installed functions:
 *
 *		SELECT * FROM mysql.func;
 *
 *
 *
 * If you have permission problems installing function:
 *
 * [1] - sudo vi /etc/apparmor.d/usr.sbin.mysqld
 *
 * [2] - add the path:  /usr/lib/mysql/plugin
 *
 * [3] - sudo /etc/init.d/apparmor restart
 *
 *
 */

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(WIN32)
#define DLLEXP __declspec(dllexport) 
#else
#define DLLEXP
#endif


#ifdef STANDARD
#include <stdio.h>
#include <string.h>
#ifdef __WIN__
typedef unsigned __int64 ulonglong;	
typedef __int64 longlong;
#else
typedef unsigned long long ulonglong;
typedef long long longlong;
#endif /*__WIN__*/
#else
#include <my_global.h>
#include <my_sys.h>
#endif
#include <mysql.h>
#include <m_ctype.h>
#include <m_string.h>		
#include <ctype.h>

#include <qrencode.h>
#include <png.h>



#ifdef HAVE_DLOPEN

#define LIBVERSION "lib_mysqludf_qrencode version 0.0.3"



/**
 * lib_mysqludf_qrencode_info
 */
DLLEXP
my_bool qrencode_info_init(
	UDF_INIT *initid,	
	UDF_ARGS *args,	
	char *message
);

DLLEXP
void qrencode_info_deinit(
	UDF_INIT *initid
);

DLLEXP
char* qrencode_info(
	UDF_INIT *initid,	
	UDF_ARGS *args,	
	char* result,	
	unsigned long* length,	
	char *is_null,	
	char *error
);

/**
 * lib_mysqludf_qrencode
 */
DLLEXP
my_bool qrencode_init( 
	UDF_INIT* initid, 
	UDF_ARGS* args, 
	char* message 
);

DLLEXP
void qrencode_deinit( 
	UDF_INIT* initid 
);

DLLEXP
char * qrencode( 
	UDF_INIT* initid, 
	UDF_ARGS* args, 
	unsigned long *length, 
	char* is_null, 
	char *error 
);



/* structure to store PNG image bytes */
struct mem_encode
{
  char *buffer;
  size_t size;
};



void
my_png_write_data( png_structp png_ptr, png_bytep data, png_size_t length )
{
	struct mem_encode* p=(struct mem_encode*)png_ptr->io_ptr;
	size_t nsize = p->size + length;

	/* allocate or grow buffer */
	if(p->buffer)
		p->buffer = realloc(p->buffer, nsize);
	else
		p->buffer = malloc(nsize);

	if(!p->buffer) {
		png_error( png_ptr, "Write Error" );
		fprintf( stderr, "Write Error\n" );
		fflush( stderr );	
	}

	/* copy new bytes to end of buffer */
	memset(p->buffer + p->size, 0, length);
	memcpy(p->buffer + p->size, data, length);
	p->size += length;
}

/* This is optional but included to show how png_set_write_fn() is called */
void
my_png_flush(png_structp png_ptr)
{
}



int qrcode_to_png( struct mem_encode * state, QRcode *qrcode )
{
	int size = 5;

	int margin = 4;

	png_structp png_ptr;
	png_infop info_ptr;
	
	unsigned char *row, *p, *q;
	int x, y, xx, yy, bit;
	int realwidth;

	/* initialise - put this before png_write_png() call */
	state->buffer = NULL;
	state->size = 0;

	realwidth = (qrcode->width + margin * 2) * size;
	row = (unsigned char *)malloc((realwidth + 7) / 8);
	if ( row == NULL ) {
		fprintf(stderr, "Failed to allocate memory.\n");
		fflush(stderr);
		return 1;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if ( png_ptr == NULL ) {
		fprintf(stderr, "Failed to initialize PNG writer.\n");
		fflush(stderr); 
		return 1;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if ( info_ptr == NULL ) {
		fprintf(stderr, "Failed to initialize PNG write.\n");
		fflush(stderr);
		return 1;
	}

	if ( setjmp(png_jmpbuf(png_ptr)) ) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fprintf(stderr, "Failed to write PNG image.\n");
		fflush(stderr);
		return 1;
	}

	/* if my_png_flush() is not needed, change the arg to NULL */
	png_set_write_fn(png_ptr, state, my_png_write_data, NULL/*my_png_flush*/);
      
	png_set_IHDR(png_ptr, info_ptr,
		realwidth, realwidth,
		1,
		PNG_COLOR_TYPE_GRAY,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	png_write_info( png_ptr, info_ptr );

	/* top margin */
	memset(row, 0xff, (realwidth + 7) / 8);
	for(y=0; y<margin * size; y++) {
		png_write_row(png_ptr, row);
	}

	/* data */
	p = qrcode->data;
	for(y=0; y<qrcode->width; y++) {
		bit = 7;
		memset(row, 0xff, (realwidth + 7) / 8);
		q = row;
		q += margin * size / 8;
		bit = 7 - (margin * size % 8);
        for(x=0; x<qrcode->width; x++) {
			for(xx=0; xx<size; xx++) {
		        *q ^= (*p & 1) << bit;
		        bit--;
                if(bit < 0) {
					q++;
					bit = 7;
                }
			}
			p++;
		}

		for(yy=0; yy<size; yy++) {
			png_write_row(png_ptr, row);
        }
	}

	/* bottom margin */
	memset(row, 0xff, (realwidth + 7) / 8);
	for(y=0; y<margin * size; y++) {
		png_write_row(png_ptr, row);
	}

	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	free(row);

	fprintf( stderr, "size = %u\n", state->size );
	fflush(stderr);

	return 0;
}



/**
 * lib_mysqludf_qrencode_info
 */
my_bool qrencode_info_init(
	UDF_INIT *initid,	
	UDF_ARGS *args,	
	char *message
) {
	my_bool status;
	if ( args->arg_count!=0 ){
		strcpy(
			message,
			"No arguments allowed (udf: lib_mysqludf_qrencode_info)"
		);
		status = 1;
	} else {
		status = 0;
	}
	return status;
}

void qrencode_info_deinit(
	UDF_INIT *initid
) {

}

char* qrencode_info(
	UDF_INIT *initid,	
	UDF_ARGS *args,	
	char* result,	
	unsigned long* length,	
	char *is_null,	
	char *error
) {
	strcpy( result, LIBVERSION );
	*length = strlen( LIBVERSION );
	return result;
}


my_bool 
qrencode_init( UDF_INIT* initid, UDF_ARGS* args, char* message ) {

	if ( args->arg_count != 1 ) {
		strcpy(message,"wrong number of arguments: qrcode() requires one argument");
    	return 1;
  	}

	args->arg_type[0] = STRING_RESULT;

	initid->maybe_null	= 1;
	initid->ptr = (char*)NULL;

	return 0;
}


void 
qrencode_deinit( UDF_INIT* initid ) {
	if ( initid->ptr )
		free( initid->ptr );
}


char *
qrencode( UDF_INIT* initid, UDF_ARGS* args, unsigned long *length, char* is_null, char *error ) {

	struct mem_encode state;
	state.buffer = NULL;
	state.size = 0;

	QRcode *qrcode = NULL;
	qrcode = QRcode_encodeString( args->args[0], 0, QR_ECLEVEL_H, QR_MODE_8, 1);
	if ( qrcode ) {
		qrcode_to_png( &state, qrcode );
		if ( state.buffer ) {
			initid->ptr = state.buffer;
			*length = state.size;
			fprintf( stderr, "qrcode size = %lu\n", *length );
			fflush(stderr);

		} else {
			*is_null = 1;
			*error = 1;
        	initid->ptr = 0;		
		}
		QRcode_free( qrcode );
	} else {
		*is_null = 1;
		*error = 1;
        initid->ptr = 0;	
	}
	
	return initid->ptr;
}


#endif /* HAVE_DLOPEN */

