#include "BUpdaterCondor.h"

int main(int argc, char *argv[]){

	FILE *fd;
	job_registry_entry *en;
	time_t now;
	time_t purge_time=0;
	char *constraint=NULL;
	char *query=NULL;
	char *q=NULL;
	char *pidfile=NULL;
	
	poptContext poptcon;
	int rc=0;			     
	int version=0;
	int first=TRUE;
	time_t dgbtimestamp;
	
	struct poptOption poptopt[] = {     
		{ "nodaemon",      'o', POPT_ARG_NONE,   &nodmn, 	    0, "do not run as daemon",    NULL },
		{ "version",       'v', POPT_ARG_NONE,   &version,	    0, "print version and exit",  NULL },
		POPT_AUTOHELP
		POPT_TABLEEND
	};
		
	argv0 = argv[0];

	poptcon = poptGetContext(NULL, argc, (const char **) argv, poptopt, 0);
 
	if((rc = poptGetNextOpt(poptcon)) != -1){
		sysfatal("Invalid flag supplied: %r");
	}
	
	poptFreeContext(poptcon);
	
	if(version) {
		printf("%s Version: %s\n",progname,VERSION);
		exit(EXIT_SUCCESS);
	}   

	cha = config_read(NULL);
	if (cha == NULL)
	{
		fprintf(stderr,"Error reading config: ");
		perror("");
		return -1;
	}

	ret = config_get("bupdater_debug_level",cha);
	if (ret != NULL){
		debug=atoi(ret->value);
	}
	
	ret = config_get("bupdater_debug_logfile",cha);
	if (ret != NULL){
		debuglogname=strdup(ret->value);
	}
	if(debug <=0){
		debug=0;
	}
    
	if(debug){
		if((debuglogfile = fopen(debuglogname, "a+"))==0){
			debuglogfile =  fopen("/dev/null", "a+");
		}
	}
	
        ret = config_get("condor_binpath",cha);
        if (ret == NULL){
                if(debug){
			dgbtimestamp=time(0);
			fprintf(debuglogfile, "%s %s: key condor_binpath not found\n",iepoch2str(dgbtimestamp),argv0);
			fflush(debuglogfile);
		}
        } else {
                condor_binpath=strdup(ret->value);
        }
	
	ret = config_get("job_registry",cha);
	if (ret == NULL){
                if(debug){
			dgbtimestamp=time(0);
			fprintf(debuglogfile, "%s %s: key job_registry not found\n",iepoch2str(dgbtimestamp),argv0);
			fflush(debuglogfile);
		}
	} else {
		registry_file=strdup(ret->value);
	}
	
	ret = config_get("purge_interval",cha);
	if (ret == NULL){
                if(debug){
			dgbtimestamp=time(0);
			fprintf(debuglogfile, "%s %s: key purge_interval not found using the default:%d\n",iepoch2str(dgbtimestamp),argv0,purge_interval);
			fflush(debuglogfile);
		}
	} else {
		purge_interval=atoi(ret->value);
	}
	
	ret = config_get("finalstate_query_interval",cha);
	if (ret == NULL){
                if(debug){
			dgbtimestamp=time(0);
			fprintf(debuglogfile, "%s %s: key finalstate_query_interval not found using the default:%d\n",iepoch2str(dgbtimestamp),argv0,finalstate_query_interval);
			fflush(debuglogfile);
		}
	} else {
		finalstate_query_interval=atoi(ret->value);
	}
	
	ret = config_get("alldone_interval",cha);
	if (ret == NULL){
                if(debug){
			dgbtimestamp=time(0);
			fprintf(debuglogfile, "%s %s: key alldone_interval not found using the default:%d\n",iepoch2str(dgbtimestamp),argv0,alldone_interval);
			fflush(debuglogfile);
		}
	} else {
		alldone_interval=atoi(ret->value);
	}
	
	ret = config_get("bupdater_pidfile",cha);
	if (ret == NULL){
                if(debug){
			dgbtimestamp=time(0);
			fprintf(debuglogfile, "%s %s: key bupdater_pidfile not found\n",iepoch2str(dgbtimestamp),argv0);
			fflush(debuglogfile);
		}
	} else {
		pidfile=strdup(ret->value);
	}
	
	if( !nodmn ) daemonize();


	if( pidfile ){
		writepid(pidfile);
		free(pidfile);
	}
	
	config_free(cha);

	for(;;){
		/* Purge old entries from registry */
		now=time(0);
		if(now - purge_time > 86400){
			if(job_registry_purge(registry_file, now-purge_interval,0)<0){

				if(debug){
					dgbtimestamp=time(0);
					fprintf(debuglogfile, "%s %s: Error purging job registry %s\n",iepoch2str(dgbtimestamp),argv0,registry_file);
					fflush(debuglogfile);
				}
                        	fprintf(stderr,"%s: Error purging job registry %s :",argv0,registry_file);
                        	perror("");
				sleep(2);

			}else{
				purge_time=time(0);
			}
		}
	       
		rha=job_registry_init(registry_file, BY_BATCH_ID);
		if (rha == NULL)
		{
			if(debug){
				dgbtimestamp=time(0);
				fprintf(debuglogfile, "%s %s: Error initialising job registry %s\n",iepoch2str(dgbtimestamp),argv0,registry_file);
				fflush(debuglogfile);
			}
			fprintf(stderr,"%s: Error initialising job registry %s :",argv0,registry_file);
			perror("");
			sleep(2);
			continue;
		}

		IntStateQuery();
		
		fd = job_registry_open(rha, "r");
		if (fd == NULL)
		{
			if(debug){
				dgbtimestamp=time(0);
				fprintf(debuglogfile, "%s %s: Error opening job registry %s\n",iepoch2str(dgbtimestamp),argv0,registry_file);
				fflush(debuglogfile);
			}
			fprintf(stderr,"%s: Error opening job registry %s :",argv0,registry_file);
			perror("");
			sleep(2);
			continue;
		}
		if (job_registry_rdlock(rha, fd) < 0)
		{
			if(debug){
				dgbtimestamp=time(0);
				fprintf(debuglogfile, "%s %s: Error read locking job registry %s\n",iepoch2str(dgbtimestamp),argv0,registry_file);
				fflush(debuglogfile);
			}
			fprintf(stderr,"%s: Error read locking job registry %s :",argv0,registry_file);
			perror("");
			sleep(2);
			continue;
		}

		first=TRUE;
		
		while ((en = job_registry_get_next(rha, fd)) != NULL){

			/* Assign Status=4 and ExitStatus=-1 to all entries that after alldone_interval are still not in a final state(3 or 4)*/
			if((now-en->mdate>alldone_interval) && en->status!=REMOVED && en->status!=COMPLETED){
				AssignFinalState(en->batch_id);	
			}
			
			if((now-en->mdate>finalstate_query_interval) && en->status!=REMOVED && en->status!=COMPLETED){
				/* create the constraint that will be used in condor_history command in FinalStateQuery*/
				if(!first) strcat(query," ||");	
				if(first) first=FALSE;
				if((constraint=malloc(strlen(en->batch_id) + 14)) == 0){
					sysfatal("can't malloc constraint %r");
        			}
				sprintf(constraint," ClusterId==%s",en->batch_id);
				
				q=realloc(query,strlen(query)+strlen(constraint)+4);
				if(q != NULL){
					query=q;	
				}else{
					sysfatal("can't realloc query: %r");
				}
				strcat(query,constraint);
				free(constraint);
				runfinal=TRUE;
			}
			free(en);
		}
		
		if(runfinal){
			FinalStateQuery(query);
			runfinal=FALSE;
		}
		free(query);
		fclose(fd);		
		job_registry_destroy(rha);
		sleep(2);
	}
	
	return(0);
	
}


int
IntStateQuery()
{
/*
 Output format for status query for unfinished jobs for condor:
 batch_id   user      status     executable     exitcode   udate(timestamp_for_current_status)
 22018     gliteuser  2          /bin/sleep     0          1202288920

 Filled entries:
 batch_id
 status
 exitcode
 udate
 
 Filled by suhmit script:
 blah_id 
 
 Unfilled entries:
 wn_addr
 exitreason
*/

        FILE *fp;
	int len;
	char *line;
	char **token;
	int maxtok_t=0,j;
	job_registry_entry en;
	int ret;
	char *cp; 

	if((token=calloc(200 * sizeof *token,1)) == 0){
		sysfatal("can't malloc token %r");
	}
	if((command_string=malloc(strlen(condor_binpath) + NUM_CHARS)) == 0){
		sysfatal("can't malloc command_string %r");
	}
	
	sprintf(command_string,"%s/condor_q -constraint \"JobStatus != 1\" -format \"%%d \" ClusterId -format \"%%s \" Owner -format \"%%d \" JobStatus -format \"%%s \" Cmd -format \"%%s \" ExitStatus -format \"%%s\\n\" EnteredCurrentStatus|grep -v condorc-",condor_binpath);
	fp = popen(command_string,"r");

	if(fp!=NULL){
		while(!feof(fp) && (line=get_line(fp))){
			if(line && (strlen(line)==0 || strncmp(line,"JOBID",5)==0)){
				free(line);
				continue;
			}
			if ((cp = strrchr (line, '\n')) != NULL){
				*cp = '\0';
			}
	
			maxtok_t = strtoken(line, ' ', token);
		
			JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,token[0]);
			en.status=atoi(token[2]);
			en.exitcode=atoi(token[4]);
			en.udate=atoi(token[5]);
			JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"\0");
			JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"\0");
				
			if ((ret=job_registry_update(rha, &en)) < 0){
				if(ret != JOB_REGISTRY_NOT_FOUND){
					fprintf(stderr,"Append of record returns %d: ",ret);
					perror("");
				}
			}
		
			for(j=0;j<maxtok_t;j++){
				free(token[j]);
			}
			free(line);
		}
		pclose(fp);
	}

	free(token);
	free(command_string);
	return(0);
}

int
FinalStateQuery(char *query)
{
/*
 Output format for status query for finished jobs for condor:
 batch_id   user      status     executable     exitcode   udate(timestamp_for_current_status)
 22018     gliteuser  4          /bin/sleep     0          1202288920

 Filled entries:
 batch_id
 status
 exitcode
 udate
 
 Filled by suhmit script:
 blah_id 
 
 Unfilled entries:
 wn_addr
 exitreason
*/
        FILE *fp;
	int len;
	char *line;
	char **token;
	int maxtok_l=0,maxtok_t=0,i,j;
	job_registry_entry en;
	int ret;
	char *cp; 

	if((token=calloc(200 * sizeof *token,1)) == 0){
		sysfatal("can't malloc token %r");
	}
	if((command_string=malloc(NUM_CHARS + strlen(query) +strlen(condor_binpath))) == 0){
		sysfatal("can't malloc command_string %r");
	}

	sprintf(command_string,"%s/condor_history -constraint \"%s\" -format \"%%d \" ClusterId -format \"%%s \" Owner -format \"%%d \" JobStatus -format \"%%s \" Cmd -format \"%%s \" ExitStatus -format \"%%s\\n\" EnteredCurrentStatus",condor_binpath,query);
	fp = popen(command_string,"r");

	if(fp!=NULL){
		while(!feof(fp) && (line=get_line(fp))){
			if(line && (strlen(line)==0 || strncmp(line,"JOBID",5)==0)){
				free(line);
				continue;
			}
			if ((cp = strrchr (line, '\n')) != NULL){
				*cp = '\0';
			}
			
			maxtok_t = strtoken(line, ' ', token);
		
			JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,token[0]);
			en.status=atoi(token[2]);
			en.exitcode=atoi(token[4]);
			en.udate=atoi(token[5]);
                	JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"\0");
                	JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"\0");
		
			if ((ret=job_registry_update(rha, &en)) < 0){
				if(ret != JOB_REGISTRY_NOT_FOUND){
					fprintf(stderr,"Append of record %d returns %d: ",i,ret);
					perror("");
				}
			}
		
			for(j=0;j<maxtok_t;j++){
				free(token[j]);
			}
		}
	}

	free(token);
	free(command_string);
	return(0);
}

int AssignFinalState(char *batchid){

	job_registry_entry en;
	int ret,i;
	time_t now;

	now=time(0);
	
	JOB_REGISTRY_ASSIGN_ENTRY(en.batch_id,batchid);
	en.status=COMPLETED;
	en.exitcode=-1;
	en.udate=now;
	JOB_REGISTRY_ASSIGN_ENTRY(en.wn_addr,"\0");
	JOB_REGISTRY_ASSIGN_ENTRY(en.exitreason,"\0");
		
	if ((ret=job_registry_update(rha, &en)) < 0){
		if(ret != JOB_REGISTRY_NOT_FOUND){
			fprintf(stderr,"Append of record %d returns %d: ",i,ret);
			perror("");
		}
	}
	return(0);
}
