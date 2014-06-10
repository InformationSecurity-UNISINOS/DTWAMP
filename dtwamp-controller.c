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
 * DTWAMP 0.1 - Controller															*
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
 *  Este código foi desenvolvido com base no dtnperf-client 2.8						*
 *																					*
 *																					*
 *																					*
 *																					*			
 * -------------------------------------------------------------------------------- *
 */

 

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "includes.h"
#include "utils.h"
#include "bundle_tools.h"
#include <signal.h>


// max payload (in bytes) if bundles are stored into memory
#define MAX_MEM_PAYLOAD 50000


//cálculo dos timestamps
#define TIMEVAL_DIFF_MSEC(t1, t2) \
    ((unsigned long int)(((t1).tv_sec  - (t2).tv_sec)  * 1000) + \
     (((t1).tv_usec - (t2).tv_usec) / 1000))

	 
/* ---------------------------------------------
 * Values inside [square brackets] are defaults
 * --------------------------------------------- */

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;


// global options

int debug = 0;    // if set to 1, many debug messages are shown [0]
int debug_level = 0;



typedef struct
{
	int expiration;				// expiration time (sec) [3600]
	int disable_fragmentation;	// disable bundle fragmentation [0]
}
dtn_options_t;


typedef struct
{
	char op_mode ;    		// operative mode (t = time_mode, b = bundle_mode) [t]
	int transmission_time;	// seconds of transmission [0]
	int qtd_bundles;        // número de bundles a enviar [0]
	int window;				// trasmission window [1]
	int slide_on_custody;	// flag sliding window on custody receipts [0] 
	dtn_reg_id_t regid;   	// registration ID (-i option) [DTN_REGID_NONE]
	dtn_bundle_payload_location_t payload_type;	// the type of data source for the bundle [DTN_PAYLOAD_FILE]
}
dtnperf_options_t;


typedef struct
{
	dtnperf_options_t *dtnperf_opt;
	dtn_options_t *dtn_opt;
}
global_options_t;



// Conteúdo Adaptado do TWAMP Controller para o Bundle Payload

typedef struct 
{
    u_int32_t seqno;
    struct timeval timestamp1;
	char padding[20];
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



// specified options for bundle tuples
char * arg_replyto = NULL; // replyto_tuple
char * arg_source = NULL; // source_tuple
char * arg_dest = NULL; // destination_tuple



/* -------------------------------
 *       function interfaces
 * ------------------------------- */

void parse_options(int, char**, dtnperf_options_t *, dtn_options_t *);
void print_usage(char* progname);

void check_options(dtnperf_options_t *perf_opt, dtn_options_t *dtn_opt);


void init_dtnperf_options(dtnperf_options_t *);
void init_dtn_options(dtn_options_t*);
void set_dtn_options(dtn_bundle_spec_t *, dtn_options_t*);


//CTRL+C handling
void sigint(int sig);


// Utility
long double calc_exec_time ( long long unsigned sec, long long unsigned sec_no );
long long unsigned calc_epoch_time ( long long unsigned dtn_time );
long double calc_timestamp ( long long unsigned sec ) ;


/* -----------------------
 *  variables declaration
 * ----------------------- */

int ret;                        // result of DTN-registration


send_information_t* send_info;


struct timeval t1, t4, comeca, termina, now, fim;      // variáveis de tempo para pegar o timestamp e manipulação de tempo


int first_bundle_ever = 0;      // Check if the first bundle is sent


// DTN variables
dtn_handle_t handle;
dtn_reg_info_t reginfo;
dtn_bundle_spec_t bundle_spec;
dtn_bundle_spec_t reply_spec;
dtn_bundle_id_t bundle_id;
dtn_bundle_payload_t send_payload;
dtn_bundle_payload_t reply_payload;
char demux[64];


dtn_bundle_spec_t final_spec;
dtn_bundle_payload_t calculo_payload;


twamp_controller_payload_t payload_contents;
twamp_reflector_payload_t payload_final;


int sent_bundles;               // number of bundles sent

int bundles_ready;

int tempo = 0;
int total_bundles = 0;



/* -----------------------
 *        M A I N
 * ----------------------- */
int main(int argc, char *argv[])
{

	char padding[20];	
	strcpy (padding, "01234567890123456789");
	
	int seqno = 0;
	int k = 0;
	
	double tempo_operacao;
	
	unsigned long int media1, media2, media3, x1, x2, x3;				//variáveis para cálculo da média da operação inteira
	
	media1 = 0;
	media2 = 0;
	media3 = 0;
	x1 = 0;
	x2 = 0;
	x3 = 0;
			
		
	dtnperf_options_t dtnperf_options;
	dtn_options_t dtn_options;
	
	
	// Init options
	init_dtnperf_options(&dtnperf_options);
	init_dtn_options(&dtn_options);


	// Parse and check command line options
	parse_options(argc, argv, &dtnperf_options, &dtn_options);

	check_options(&dtnperf_options, &dtn_options);

	
	if (dtnperf_options.op_mode == 't') 
		printf ("\n Transmission Time required = %u second(s) \n", tempo);
	else if (dtnperf_options.op_mode == 'b')
		printf ("\n Number of Bundles requested to send = %u bundle(s) \n", total_bundles);
	else
		printf ("\n Invalid option"); 	
		
	
	// Connect to DTN Daemon
	
    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);


	if (err != DTN_SUCCESS)
	{
		fprintf(stderr, "fatal error opening dtn handle: %s\n", dtn_strerror(err));
		exit(1);
	}

	
	/* -----------------------------------------------------
	 *   initialize and parse bundle src/dest/replyto EIDs
	 * ----------------------------------------------------- */

	memset(&bundle_spec, 0, sizeof(bundle_spec));


	// SOURCE is local EID + demux string (with optional file path)
	sprintf(demux, "/dtnperf:/src_%d",getpid());
	dtn_build_local_eid(handle, &bundle_spec.source, demux);

	
	// DEST host is specified at runtime, demux is hardcoded
	sprintf(demux, "/dtnperf:/dest");
	strcat(arg_dest, demux);
	
	if (parse_eid(handle, &bundle_spec.dest, arg_dest) == NULL)
	{
		fprintf(stderr, "fatal error parsing dtn EID: invalid eid string '%s'\n", arg_dest);
		exit(1);
	}

	
	// REPLY-TO (if none specified, same as the source)
	if (arg_replyto == NULL)
	{
		dtn_copy_eid(&bundle_spec.replyto, &bundle_spec.source);
	}
	else
	{
		sprintf(demux, "/dtnperf:/src_%d", getpid());
		strcat(arg_replyto, demux);
		parse_eid(handle, &bundle_spec.dest, arg_replyto);
	}

	

	/* ------------------------
	 * set DTN options
	 * ------------------------ */

	set_dtn_options(&bundle_spec, &dtn_options);


	/* ----------------------------------------------
	 * create a new registration based on the source
	 * ---------------------------------------------- */

	memset(&reginfo, 0, sizeof(reginfo));

	dtn_copy_eid(&reginfo.endpoint, &bundle_spec.replyto);

	
	reginfo.flags = DTN_REG_DEFER;
	reginfo.regid = dtnperf_options.regid;
	reginfo.expiration = 30;

	
	if ((ret = dtn_register(handle, &reginfo, &dtnperf_options.regid)) != 0)
	{
		fprintf(stderr, "error creating registration: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
		exit(1);
	}
		
		
	/* ------------------------------------------------------------------------------
	 * select the operative-mode (between Time_Mode and Bundle_Mode)
	 * ------------------------------------------------------------------------------ */
	
	
	if ((dtnperf_options.op_mode == 't') && (tempo > 0))	// TIME MODE
	{

		sent_bundles = 0;
		
		bundles_ready = dtnperf_options.window;

		// Create the array for the bundle send info
		
		if (dtnperf_options.slide_on_custody==1)
		{
			
			send_info = (send_information_t*) malloc((dtnperf_options.window+1000) * sizeof(send_information_t));
			init_info(send_info, dtnperf_options.window+1000);
		}
		else
		{
			send_info = (send_information_t*) malloc(dtnperf_options.window * sizeof(send_information_t));
			init_info(send_info, dtnperf_options.window);
		}

		
		gettimeofday(&comeca, NULL);					//cálculo do tempo de transmissão e tempo total da transmissão
		
		termina = set (0);								//cálculo do tempo de transmissão
		termina.tv_sec = comeca.tv_sec + tempo;
		
		
		seqno = 1;
		
		
		for (now.tv_sec = comeca.tv_sec;
		      now.tv_sec <= termina.tv_sec;
		      gettimeofday(&now, NULL))			
		{

			// TIMESTAMP 1 (Timestamp de saída do Controller)
			gettimeofday(&t1, NULL);
		
		
			//sleep (5);
		
		
			payload_contents.seqno = seqno;
			payload_contents.timestamp1 = t1;
			strcpy(payload_contents.padding, padding);	
			
						
			// Fill the payload
			memset(&send_payload, 0, sizeof(send_payload));
		
			dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM, (char*)&payload_contents, sizeof(payload_contents));

		
			//CTRL+C handling
			signal(SIGINT, &sigint);
		
			
			//Envio do bundle para o Responder
			memset(&bundle_id, 0, sizeof(bundle_id));

			if ((ret = dtn_send(handle, dtnperf_options.regid, &bundle_spec, &send_payload, &bundle_id)) != 0)
				{
					fprintf(stderr, "error sending bundle: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
					exit(1);
				}
		
		
			int g = 0;
			g = sizeof(payload_contents);
		
			printf ("\n Lenght's size of Controller Payload: %d bytes of %s \n \n", g, bundle_spec.source.uri);
		
					
			//sleep (2);
		
		
			memset(&final_spec, 0, sizeof(final_spec));
			memset(&calculo_payload, 0, sizeof(calculo_payload));

			printf("\n Waiting for bundles...");
			
		
			//Recebe o Bundle do Responder
			//Nesta versão, não tratamos as flags, apenas os conteúdos dos payloads que são necessários para o cálculo two-way

		
			if ((ret = dtn_recv(handle, &final_spec, DTN_PAYLOAD_MEM, &calculo_payload, -1)) < 0)
			{
				fprintf(stderr, "error getting recv reply: %d (%s)\n",
			        ret, dtn_strerror(dtn_errno(handle)));
				exit(1);
			}
		
		
			// TIMESTAMP 4 (Timestamp quando o Controller recebe os dados do Responder)
			gettimeofday(&t4, NULL);
		
		
			printf("\n Bundle Received \n \n");
		
			printf ("============================================================\n");
			printf (" Bundle Received Details\n");
			printf (" Source: %s\n", final_spec.source.uri);
			printf ("------------------------------------------------------------\n \n");	
		
		
			memcpy(&payload_final, calculo_payload.buf.buf_val, sizeof(payload_final));
		
		
			printf(" Lenght's size of Responder Payload: %zu bytes of %s\n",
		    calculo_payload.buf.buf_len,
		    final_spec.source.uri);
		
			printf ("------------------------------------------------------------\n \n");
		
			printf ("Payload Data Received from Responder \n");
		
		
			printf ("\n \t Responder's Sequence number = %d \n", payload_final.seqno_reflector);
			
			printf ("\t Responder's Received Timestamp = %u \n", (((u_int)payload_final.timestamp2_receive_reflector.tv_sec) - 946684800));
				
			printf ("\t Responder's Output Timestamp = %u \n", (((u_int)payload_final.timestamp3_reflector.tv_sec) - 946684800));

			printf ("\t Controller's Sequence number = %u \n", (u_int)payload_final.seqno_sender);
				
			printf ("\t Controller's Timestamp = %u \n", (((u_int)payload_final.timestamp1_sender.tv_sec) - 946684800));
				
			printf ("\n ------------------------------------------------------------\n \n");
		
		
			// Se o debug for ativado, imprime os valores dos timestamps
			if (debug)
			{
		
				printf (" T1 tv sec = %u \n", (u_int)t1.tv_sec);
				printf (" T1 tv usec = %u \n", (u_int)t1.tv_usec);
		
				printf (" T2 tv sec = %u \n", (u_int)payload_final.timestamp2_receive_reflector.tv_sec);
				printf (" T2 tv usec = %u \n", (u_int)payload_final.timestamp2_receive_reflector.tv_usec);
				
				printf (" T3 tv sec = %u \n", (u_int)payload_final.timestamp3_reflector.tv_sec);
				printf (" T3 tv usec = %u \n", (u_int)payload_final.timestamp3_reflector.tv_usec);
		
				printf (" T4 tv sec = %u \n", (u_int)t4.tv_sec);
				printf (" T4 tv usec = %u \n", (u_int)t4.tv_usec);
				
				printf ("------------------------------------------------------------\n \n");
			}
			
			
			x1 = (TIMEVAL_DIFF_MSEC(payload_final.timestamp2_receive_reflector,t1));
			media1 = media1 + x1;
						
			x2 = (TIMEVAL_DIFF_MSEC(t4,payload_final.timestamp3_reflector));
			media2 = media2 + x2;
			
			x3 = (TIMEVAL_DIFF_MSEC(t4,t1));
			media3 = media3 + x3;
			
			
			// Imprime os Resultados do cálculo round-trip de cada direção
			
			printf ("Results \n");
		
			printf ("\n Controller's Output Time until arrival at Responder = %ld ms\n", x1);
			
			printf ("\n Responder's Output Time until arrival at Controller = %ld ms\n", x2);
		
			printf ("\n Controllers' Output Total Time until get back to it = %ld ms\n", x3);
		
			printf ("\n");
		
			printf ("------------------------------------------------------------\n \n");
		
		
			sent_bundles++;
		
			seqno++;
		
			printf(" %d bundle(s) send(s) in total, each with %zu payload's bytes \n \n", sent_bundles, g);
		
			printf ("============================================================\n");
		
		}			//for
	} 			    // End of Time Mode

	else if ((dtnperf_options.op_mode == 'b') && (total_bundles > 0))     	//BUNDLE MODE
	{
	
		sent_bundles = 0;
		
		bundles_ready = dtnperf_options.window;

		// Create the array for the bundle send info
		
		if (dtnperf_options.slide_on_custody==1)
		{
			
			send_info = (send_information_t*) malloc((dtnperf_options.window+1000) * sizeof(send_information_t));
			init_info(send_info, dtnperf_options.window+1000);
		}
		else
		{
			send_info = (send_information_t*) malloc(dtnperf_options.window * sizeof(send_information_t));
			init_info(send_info, dtnperf_options.window);
		}

					
        gettimeofday(&comeca, NULL);			//cálculo do tempo total da transmissão
			
		seqno = 1;
		
		
		for (k = 0; k < total_bundles; k++)			
		{

			// TIMESTAMP 1 (Timestamp de saída do Controller)
			gettimeofday(&t1, NULL);
		
		
			//sleep (5);
		
		
			payload_contents.seqno = seqno;
			payload_contents.timestamp1 = t1;
			strcpy(payload_contents.padding, padding);	
			
					
			// Fill the payload
			memset(&send_payload, 0, sizeof(send_payload));
		
			dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM, (char*)&payload_contents, sizeof(payload_contents));

		
			//CTRL+C handling
			signal(SIGINT, &sigint);
		
				
			//Envio do bundle para o Responder
			memset(&bundle_id, 0, sizeof(bundle_id));
		
			if ((ret = dtn_send(handle, dtnperf_options.regid, &bundle_spec, &send_payload, &bundle_id)) != 0)
			{
				fprintf(stderr, "error sending bundle: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
				exit(1);
			}
		
		
			int g = 0;
			g = sizeof(payload_contents);
		
			printf ("\n Lenght's size of Controller Payload: %d bytes of %s \n \n", g, bundle_spec.source.uri);
		
			
			//sleep (2);
		
		
			memset(&final_spec, 0, sizeof(final_spec));
			memset(&calculo_payload, 0, sizeof(calculo_payload));

			printf("\n Waiting for bundles...");
			
			
			//Recebe o Bundle do Responder
			//Nesta versão, não tratamos as flags, apenas os conteúdos dos payloads que são necessários para o cálculo two-way
			
			
			if ((ret = dtn_recv(handle, &final_spec, DTN_PAYLOAD_MEM, &calculo_payload, -1)) < 0)
			{
				fprintf(stderr, "error getting recv reply: %d (%s)\n",
			        ret, dtn_strerror(dtn_errno(handle)));
				exit(1);
			}
		
		
			// TIMESTAMP 4 (Timestamp quando o Controller recebe os dados do Responder)
			gettimeofday(&t4, NULL);
		
			printf("\n Bundle Received \n \n");
				
			printf ("============================================================\n");
			printf (" Bundle Received Details \n");
			printf (" Source: %s\n", final_spec.source.uri);
			printf ("------------------------------------------------------------\n \n");	
		
		
			memcpy(&payload_final, calculo_payload.buf.buf_val, sizeof(payload_final));
			
			
			printf(" Lenght's size of Responder Payload: %zu bytes de %s\n",
		    calculo_payload.buf.buf_len,
		    final_spec.source.uri);
				
			printf ("------------------------------------------------------------\n \n");
		
			printf ("Payload Data Received from Responder  \n");
		
			printf ("\n \t Responder's Sequence number  = %d \n", payload_final.seqno_reflector);
			
			printf ("\t Responder's Received Timestamp = %u \n", (((u_int)payload_final.timestamp2_receive_reflector.tv_sec) - 946684800));
				
			printf ("\t Responder's Output Timestamp  = %u \n", (((u_int)payload_final.timestamp3_reflector.tv_sec) - 946684800));

			printf ("\t Controller's Sequence number = %u \n", (u_int)payload_final.seqno_sender);
				
			printf ("\t Controller's Timestamp  = %u \n", (((u_int)payload_final.timestamp1_sender.tv_sec) - 946684800));
				
				
			printf ("\n ------------------------------------------------------------\n \n");
		
		
			// Se o debug for ativado, imprime os valores dos timestamps
		
			if (debug)
			{
				printf (" T1 tv sec = %u \n", (u_int)t1.tv_sec);
				printf (" T1 tv usec = %u \n", (u_int)t1.tv_usec);
		
				printf (" T2 tv sec = %u \n", (u_int)payload_final.timestamp2_receive_reflector.tv_sec);
				printf (" T2 tv usec = %u \n", (u_int)payload_final.timestamp2_receive_reflector.tv_usec);
				
				printf (" T3 tv sec = %u \n", (u_int)payload_final.timestamp3_reflector.tv_sec);
				printf (" T3 tv usec = %u \n", (u_int)payload_final.timestamp3_reflector.tv_usec);
		
				printf (" T4 tv sec = %u \n", (u_int)t4.tv_sec);
				printf (" T4 tv usec = %u \n", (u_int)t4.tv_usec);
				
				printf ("------------------------------------------------------------\n \n");
			}
			
			
			x1 = (TIMEVAL_DIFF_MSEC(payload_final.timestamp2_receive_reflector,t1));
			media1 = media1 + x1;
						
			x2 = (TIMEVAL_DIFF_MSEC(t4,payload_final.timestamp3_reflector));
			media2 = media2 + x2;
			
			x3 = (TIMEVAL_DIFF_MSEC(t4,t1));
			media3 = media3 + x3;
			
			
			// Imprime os Resultados do cálculo round-trip de cada direção
			
			printf ("Results \n");
		
			printf ("\n n Controller's Output Time until arrival at Responder  = %ld ms\n", x1);
			
			printf ("\n Responder's Output Time until arrival at Controller = %ld ms\n", x2);
		
			printf ("\n Controllers' Output Total Time until get back to it = %ld ms\n", x3);
		
			printf ("\n");
		
			printf ("------------------------------------------------------------\n \n");
			
			
			sent_bundles++;
		
			seqno++;
		
			printf(" %d bundle(s) send(s) in total, each with  %zu payload's bytes \n \n", sent_bundles, g);
		
			printf ("============================================================\n");
		
		}       //for
	}    		//End of Bundle Mode
	else        // só aceita TIME MODE ou BUNDLE MODE
	{
		// This should not be executed (written only for debug purpouse)
		fprintf(stderr, "ERROR: invalid operative mode! Specify -t or -b\n");
		exit(3);
	}


	//Timestamp para calcular final da operação
	gettimeofday(&fim, NULL);
	
	
	tempo_operacao = ((double)(fim.tv_sec - comeca.tv_sec) * 1000.0 + (double)(fim.tv_usec - comeca.tv_usec) / 1000.0) / 1000.0;
	
	
	printf ("\n Averages of this operation \n");
		
	printf ("\n Controller's Output Average until arrival at Responder: %ld ms\n", (media1/sent_bundles));
	
	printf ("\n Responder's Output Average until arrival at Controller: %ld ms\n", (media2/sent_bundles));
	
	printf ("\n Controllers' Output Total Time average until get back to it: %ld ms\n", (media3/sent_bundles));
	
	printf ("\n \n Total Operation Time = %.1f seconds \n",tempo_operacao);
	
	
	// Close the DTN handle -- IN DTN_2.1.1 SIMPLY RETURNS -1
	if ((debug) && (debug_level > 0))
		printf("[debug] closing DTN handle...");

	if (dtn_close(handle) != DTN_SUCCESS)
	{
		fprintf(stderr, "fatal error closing dtn handle: %s\n", strerror(errno));
		
		exit(1);
	}

	if ((debug) && (debug_level > 0))
		printf(" done\n");
	

	free(send_info);

	
	// Final carriage return
	printf("\n");

	return 0;
} // End main




/* ----------------------------------------
 *           UTILITY FUNCTIONS
 * ---------------------------------------- */


/* ----------------------------
 * print_usage
 * ---------------------------- */
void print_usage(char* progname)
{
	fprintf(stderr, "DTNperf ver 2.7\nSYNTAX: %s "
	        "-d <dest_eid> "
	        "[-t <sec>] [options]\n", progname);
	fprintf(stderr, "\n\
 -d, --destination <eid>    Destination eid (required).\n\
 -t, --time <sec>    Time-mode: seconds of transmission.\n\
 -b, -- Qtd of Bundles <number>    Bundle-mode: number of bundles for the transmission.\n\
 Options \n\
 -w, --window <size>  Size of transmission window, i.e. max number of bundles \"in flight\" (not still ACKed by a \"delivered\" status reports); default =1.\n\
 -C, --custody [SONC||Slide_on_Custody] Enable both custody transfer and \"custody accepted\" status reports; if SONC||Slide_on_Custody is set, a bundle in the transmission window can be ACKed also by the \"custody accepted\" status report sent by the first custodian but the source.\n\
 -u, --nofragment       Disable bundle fragmentation.\n\
Other options:\n\
 -D, --debug <level>    Debug messages [0-1-2], if the level is not indicated assume level=0.\n\
 -h, --help     Help.\n\
 -e, --expiration <time> expiration time in seconds (default: one hour).\n");
	exit(1);
} // end print_usage



void init_dtnperf_options(dtnperf_options_t *opt)
{
	opt->op_mode = 't';
	opt->transmission_time = 0;
	opt->qtd_bundles = 0;
	opt->window = 1;
	opt->slide_on_custody = 0;
	opt->regid = DTN_REGID_NONE;
	opt->payload_type = DTN_PAYLOAD_MEM;
}



void init_dtn_options(dtn_options_t* opt)
{
	opt->expiration = 3600; // expiration time (sec) [3600]
	opt->disable_fragmentation = 1; //disable bundle fragmentation[0]   padrão é 0, mas deixar como não fragmentar
}



void set_dtn_options(dtn_bundle_spec_t *bundle_spec, dtn_options_t *opt)
{
	// Bundle expiration
	bundle_spec->expiration = opt->expiration;
			
	//Disable bundle fragmentation

	if (opt->disable_fragmentation)
	{
		bundle_spec->dopts |= DOPTS_DO_NOT_FRAGMENT;

		if ((debug) && (debug_level > 0))
			printf("DO_NOT_FRAGMENT ");

	}

} // end set_dtn_options



/* ----------------------------
 * parse_options
 * ---------------------------- */
void parse_options(int argc, char**argv, dtnperf_options_t *perf_opt, dtn_options_t *dtn_opt)
{
	char c, done = 0;

	while (!done)
	{
		static struct option long_options[] =
		    {
			    {"destination", required_argument, 0, 'd'},
			    {"time", required_argument, 0, 't'},
			    {"bundle", required_argument, 0, 'b'},
			    {"custody", optional_argument, 0, 'C'},
                {"window", required_argument, 0, 'w'},
			    {"help", no_argument, 0, 'h'},
			    {"debug", required_argument, 0, 'D'},
			    {"nofragment", no_argument, 0, 'u'},
			    {"expiration", no_argument, 0, 'e'},
			};

		int option_index = 0;
		c = getopt_long(argc, argv, "hD:C:w:d:t:b:u:e:", long_options, &option_index);

		switch (c)
		{
        case 'h':
			print_usage(argv[0]);
			exit(0);
			return ;

		case 'C':
			if ((optarg!=NULL && (strncmp(optarg, "SONC", 4)==0||strncmp(optarg, "Slide_on_Custody", 16)==0))||((argv[optind]!=NULL)&&(strncmp(argv[optind], "SONC", 4)==0||strncmp(argv[optind], "Slide_on_Custody", 16)==0))){
				perf_opt->slide_on_custody=1;
			}
			break;

		case 'w':
			perf_opt->window = atoi(optarg);
			break;

		
		case 'd':
			arg_dest = optarg;
			break;

		case 'D':
			debug = 1;
			if (optarg != NULL)
				debug_level = atoi(optarg);
			break;

		case 't':
			perf_opt->op_mode = 't';
			perf_opt->transmission_time = atoi(optarg);
			tempo = atoi(optarg);
			break;
			
		case 'b':
			perf_opt->op_mode = 'b';
			perf_opt->qtd_bundles = atoi(optarg);
			total_bundles = atoi(optarg);
			break;

		
		case 'u':
			dtn_opt->disable_fragmentation = 1;
			break;

		case 'e':
			dtn_opt->expiration = atoi(optarg);
			break;

		case '?':
			break;

		case (char)(-1):
			done = 1;
			break;

		default:
			// getopt already prints an error message for unknown option characters
			print_usage(argv[0]);
			exit(1);
		} // --switch
	} // -- while


#define CHECK_SET(_arg, _what)                                          	\
    if (_arg == 0) {                                                    	\
        fprintf(stderr, "\nSYNTAX ERROR: %s must be specified\n", _what);   \
        print_usage(argv[0]);                                               \
        exit(1);                                                        	\
    }

	CHECK_SET(arg_dest, "destination tuple");
	CHECK_SET(perf_opt->op_mode, "-t or -b");
} // end parse_options



/* ----------------------------
 * check_options
 * ---------------------------- */
void check_options(dtnperf_options_t *perf_opt, dtn_options_t *dtn_opt)
{
	(void)dtn_opt;
	// checks on values
	
	if ((perf_opt->op_mode == 't') && (perf_opt->transmission_time <= 0))
	{
		fprintf(stderr, "\nSYNTAX ERROR: (-t option) you should specify a positive time\n\n");
		exit(2);
	}
	
	
	if ((perf_opt->op_mode == 'b') && (perf_opt->qtd_bundles <= 0))
	{
		fprintf(stderr, "\nSYNTAX ERROR: (-b option) you should specify a positive value of bundles\n\n");
		exit(2);
	}
	
	
	if (perf_opt->window <= 0)
	{
		fprintf(stderr, "\nSYNTAX ERROR: (-w option) you should specify a positive value of window\n\n");
		exit(2);
	}

	if ((perf_opt->op_mode == 't') && (perf_opt->window == 0))
	{
		fprintf(stderr, "\nSYNTAX ERROR: you cannot use -w option in Time-Mode\n\n");
		exit(2);
	}

	
} // end check_options



void sigint(int sig)
{
	(void)sig;

	exit(0);	
}




//long double calc_exec_time ( long long unsigned sec, long long unsigned sec_no ) 
//{
//  return ((long double)(sec) - start.tv_sec) + (((long double)(sec_no) - start.tv_usec) / 1000000.0 ) ;
//}

//long double calc_timestamp ( long long unsigned sec ) {
//  return ((long double)(sec) - start.tv_sec);
//}

//long long unsigned calc_epoch_time ( long long unsigned dtn_time ) 
//{
//  long long unsigned offset = 946684800; // 2000-01-01 DTN epoch
//  return dtn_time + offset;
//}
