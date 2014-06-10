/*
*    Copyright 2005-2006 Intel Corporation
* 
*    Licensed under the Apache License, Version 2.0 (the "License");
*    you may not use this file except in compliance with the License.
*    You may obtain a copy of the License at
* 
*        http://www.apache.org/licenses/LICENSE-2.0
* 
*    Unless required by applicable law or agreed to in writing, software
*    distributed under the License is distributed on an "AS IS" BASIS,
*    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*    See the License for the specific language governing permissions and
*    limitations under the License.
*/



/* -------------------------------------------------------------------------------- *
 * DTWAMP 0.1 - Responder															*
 *																					*
 * Adaptado por 																	*
 *																					*
 * Carlos Lamb - carloslamb(at)gmail.com											*
 * Jéferson Nobre -  jcnobre(at)unisinos.br											*
 *																					*
 * Centro de Ciências Exatas e Tecnológicas 										*
 * Universidade do Vale do Rio dos Sinos (UNISINOS)									*
 * São Leopoldo - RS - Brasil														*
 *																					*
 *																					*
 *  Este código foi desenvolvido com base no dtnperf-server 2.5.1					*
 *																					*
 * -------------------------------------------------------------------------------- *
 */



#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dtn_api.h>



#define TEMPSIZE 1024
#define BUFSIZE 16
#define BUNDLE_DIR_DEFAULT "/var/dtn/dtnperf"
#define OUTFILE "dtnbuffer.rcv"
#define MAXSIZE 256


/* ------------------------------
 *  Global variables and options
 * ------------------------------ */

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;

// values between [square brackets] are default
const char *progname ;
int use_file = 0;        // if set to 0, memorize received bundles into memory (max 50000 bytes) [1]
// if set to 1, memorize received bundles into a file (-f) [0]
int debug = 0;        // if set to 1, show debug messages [1]
int debug_level = 0;
char * endpoint = "/dtnperf:/dest";     // endpoint (-e) ["/dtnperf:/dest"]


/* ------------------------
 *  Function Prototypes
 * ------------------------ */
void print_usage(char*);
void parse_options(int, char**);
dtn_endpoint_id_t* parse_eid(dtn_handle_t, dtn_endpoint_id_t*, char*);



// Conteúdo Adaptado do TWAMP Controller para o Bundle Payload

typedef struct
{
	u_int32_t seqno_sender;
	struct timeval timestamp1_sender;
	char padding_sender[20];
}
twamp_controller_payload_t;



// Conteúdo Adaptado do TWAMP Responder para o Bundle Payload

typedef struct
{
	u_int32_t seqno_reflector;
	struct timeval timestamp2_receive_reflector;
	struct timeval timestamp3_reflector;
	u_int32_t seqno_sender;
	struct timeval timestamp1_sender;
}
twamp_reflector_payload_t;




/* -------------------------------------------------
 * main
 * ------------------------------------------------- */
int main(int argc, char** argv)
{

	/* -----------------------
	 *  variables declaration
	 * ----------------------- */
	int ret;
	dtn_handle_t handle;
	dtn_endpoint_id_t local_eid;
	dtn_reg_info_t reginfo;
	dtn_reg_id_t regid;
	dtn_bundle_spec_t spec;
	dtn_bundle_payload_t payload;
	dtn_bundle_payload_location_t pl_type = DTN_PAYLOAD_MEM; 	// payload saved into memory or into file
	int count = 0;
	
		
	
	twamp_controller_payload_t recebido_payload;
	
	twamp_reflector_payload_t volta_payload;
	
	
	struct timeval t2, t3, now;						 //variáveis de tempo para pegar o timestamp e manipulação de tempo
	
	
	dtn_bundle_payload_t manda_payload;
	
	dtn_bundle_id_t bundle_id;
	
	dtn_bundle_spec_t spec_volta;
		
	struct tm* ptm;
	
	char time_string[40];
	
	
	/* -------
	 *  begin
	 * ------- */

	// parse command-line options
	parse_options(argc, argv);
	
	
	
	// open the ipc handle
	
    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);

	if (err != DTN_SUCCESS)
	{
		fprintf(stderr, "fatal error opening dtn handle: %s\n",
		        dtn_strerror(err));
		exit(1);
	}
	

	// build a local tuple based on the configuration of local dtn router
	
	dtn_build_local_eid(handle, &local_eid, endpoint);
	

	// create a new registration based on this eid
	if ((debug) && (debug_level > 0))
		printf("[debug] registering to local daemon...");
	memset(&reginfo, 0, sizeof(reginfo));
	dtn_copy_eid(&reginfo.endpoint, &local_eid);
	reginfo.flags = DTN_REG_DEFER;
	reginfo.regid = DTN_REGID_NONE;
	reginfo.expiration = 0;
	if ((ret = dtn_register(handle, &reginfo, &regid)) != 0)
	{
		fprintf(stderr, "error creating registration: %d (%s)\n",
		        ret, dtn_strerror(dtn_errno(handle)));
		exit(1);
	}
	
	
	if (debug)
		printf("regid 0x%x\n", regid);


	printf("Waiting for Bundles...\n");

		
	// infinite loop, waiting for bundles
	while (1)
	{
		// wait until receive a bundle
		memset(&spec, 0, sizeof(spec));
		memset(&payload, 0, sizeof(payload));

		
		//sleep (5);
		
		
		//Recebe o Bundle do Controller
		//Nesta versão, não tratamos as flags, apenas os conteúdos dos payloads que são necessários para o cálculo two-way	
		
			
		if ((ret = dtn_recv(handle, &spec, pl_type, &payload, -1)) < 0)
		{
			fprintf(stderr, "error getting recv reply: %d (%s)\n",
			    ret, dtn_strerror(dtn_errno(handle)));
			exit(1);
		}
		
		
		// TIMESTAMP 2 (Timestamp quando o Responder recebe os dados do Controller)
		gettimeofday(&t2, NULL);
		
		now = t2;
		
		ptm = localtime (&now.tv_sec);
		
		strftime (time_string, sizeof(time_string), "%H:%M:%S %d-%m-%Y", ptm);
		
		printf("\n Bundle Received at %s \n", time_string);
		
		count++;	
						
		size_t len;
		if (pl_type == DTN_PAYLOAD_MEM)
		{
			len = payload.buf.buf_len;
			memcpy(&recebido_payload, payload.buf.buf_val, sizeof(recebido_payload));
		}
		else
		{
			exit(1);
		}
		
			
		printf ("============================================================\n");
		printf (" Bundle Received Details  \n");
		printf (" Source: %s\n", spec.source.uri);
		
		printf ("------------------------------------------------------------\n \n");
		
		printf ("Payload Data Received from Controller \n");
		
		printf("\n Lenght's size of Controller Payload: %zu bytes from %s\n", len, spec.source.uri);
						
		printf ("\n \t Controller's Sequence number  = %d \n", recebido_payload.seqno_sender);
		
		printf ("\t Controller's Timestamp = %u \n", (((u_int)recebido_payload.timestamp1_sender.tv_sec) - 946684800) );
				
		printf ("\t Controller's Packet Padding = %s \n \n", recebido_payload.padding_sender);
			
		printf ("------------------------------------------------------------\n \n");
		
		printf ("Payload Data from Responder   \n");
		
		
		volta_payload.seqno_reflector = recebido_payload.seqno_sender;
				
		volta_payload.timestamp2_receive_reflector = t2;
		
		volta_payload.seqno_sender = recebido_payload.seqno_sender;
		
		volta_payload.timestamp1_sender = recebido_payload.timestamp1_sender;
		

		fflush(stdout);
		
		
		//sleep (3);

		
		memset(&spec_volta, 0, sizeof(spec_volta));
		
		memset(&manda_payload, 0, sizeof(manda_payload));
		
		memset(&bundle_id, 0, sizeof(bundle_id));
		
		
		spec_volta = spec;
		
		spec_volta.source = spec.dest;
		
		spec_volta.dest = spec.source;
		
			
		int g = 0;
		
		
		// TIMESTAMP 3 (Timestamp de saída do Responder)
		gettimeofday(&t3, NULL);
		
		
		//sleep (4);
		
		volta_payload.timestamp3_reflector = t3;	
							
		dtn_set_payload(&manda_payload, DTN_PAYLOAD_MEM, (char*)&volta_payload, sizeof(volta_payload));
		
		
		//Envio do bundle para o Controller	
		
		if ((ret = dtn_send(handle, regid, &spec_volta, &manda_payload, &bundle_id)) != 0) 
		{
			fprintf(stderr, "error sending bundle: %d (%s)\n",
				ret, dtn_strerror(dtn_errno(handle)));
			exit(1);
		}
		
		
		g = sizeof(volta_payload);
		
		printf ("\n Lenght's size of Responder Payload = %zu bytes from %s \n", g, spec_volta.source.uri);
		
		printf ("\n \t Responder's Sequence number = %d \n", volta_payload.seqno_reflector);
			
		printf ("\t Responder's Received Timestamp = %u \n", (((u_int)volta_payload.timestamp2_receive_reflector.tv_sec) - 946684800) );
				
		printf ("\t Responder's Output Timestamp = %u \n", (((u_int)volta_payload.timestamp3_reflector.tv_sec) - 946684800) );		
		
		printf ("\t Controller's Sequence number = %d \n", volta_payload.seqno_sender);
			
		printf ("\t Controller's Timestamp = %u \n \n", (((u_int)volta_payload.timestamp1_sender.tv_sec) - 946684800) );
			
		printf ("------------------------------------------------------------\n \n");
		
		printf (" Total Bundles received so far: %u \n \n", count);
		
		printf ("============================================================\n");
		
		printf ("\n");
		
		
	} // while(1)

	// if this was ever changed to gracefully shutdown, it would be good to call:
	dtn_close(handle);

	return 0;

} //main

/* -------------------------------
 *        Utility Functions
 * ------------------------------- */

 
/* -------------
 *  print_usage
 * ------------- */
void print_usage(char* progname)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "SYNTAX: %s [options]\n", progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, " -D, --debug <level>\tDebug messages [0-1], if level is not indicated assume level=0.\n");
	fprintf(stderr, " -h, --help\tHelp.\n");
	fprintf(stderr, "\n");
	exit(1);
}


/* ---------------
 *  parse_options
 * --------------- */
void parse_options (int argc, char** argv)
{
	char c, done = 0;

	while (!done)
	{

		static struct option long_options[] =
		    {
			    {"help", no_argument, 0, 'h'},
			    {"debug", required_argument, 0, 'D'},
			};

		int option_index = 0;
		c = getopt_long(argc, argv, "hD:", long_options, &option_index);

		switch (c)
		{
        case 'h':            // show help
			print_usage(argv[0]);
			exit(1);
			return ;
		case 'D':            // debug messages
			debug = 1;
			if (optarg != NULL)
				debug_level = atoi(optarg);
			break;
		
		case '?':
			break;

		case (char)(-1):
			done = 1;
			break;
		default:
			print_usage(argv[0]);
			exit(1);
		}
	}

#define CHECK_SET(_arg, _what)                                          \
    if (_arg == 0) {                                                    \
        fprintf(stderr, "\nSYNTAX ERROR: %s must be specified\n", _what);      \
        print_usage(argv[0]);                                                  \
        exit(1);                                                        \
    }

}


/* ----------------------------
 * parse_tuple
 * ---------------------------- */
dtn_endpoint_id_t* parse_eid(dtn_handle_t handle, dtn_endpoint_id_t* eid, char * str)
{
	// try the string as an actual dtn tuple
	if (!dtn_parse_eid_string(eid, str))
	{
		return eid;
	}
	// build a local tuple based on the configuration of our dtn
	// router plus the str as demux string
	else if (!dtn_build_local_eid(handle, eid, str))
	{
		if (debug)
			fprintf(stdout, "%s (local)\n", str);
		return eid;
	}
	else
	{
		fprintf(stderr, "invalid eid string '%s'\n", str);
		exit(1);
	}
} // end parse_tuple
