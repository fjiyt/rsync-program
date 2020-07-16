#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <utime.h>
#include <sys/time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>

#define SECOND_TO_MICRO 1000000
#define BUFFER_SIZE 1024

void ssu_runtime(struct timeval *begin_t, struct timeval *end_t);//수행시간 측정함수
void ssu_backup(char *srcpath, char *dstpath);//파일 동기화 시작함수
void *ssu_sync_file(void *arg);//dst파일에 파일 동기화
//void make_tmpfile(DIR *dp);//SIGINT를 대비해 .tmp 파일 작성
char *real_path(char *filename,char *tmp);//절대경로 얻는 함수
int is_access(char *path);//접근가능한 파일인지 확인 함수
void print_usage();//사용법 출력함수

struct thread_data{//쓰레드에 전달할 인자
	char *dstpath;//dst디렉토리 경로
	char *filepath;//src 파일이름
};

char savedpath[BUFFER_SIZE];//현재경로 저장
int srcfilesize,fileReadsize,fileWritesize,fileBufsize;//파일읽기쓰기할때 사이즈 저장

int main(int argc, char *argv[])
{
	struct timeval begin_t, end_t;
	gettimeofday(&begin_t,NULL);//현재시간 저장

	memset(savedpath,0,BUFFER_SIZE);
	getcwd(savedpath,BUFFER_SIZE);//현재경로 저장

	char *srcpath;//src의 경로
	char *dstpath;//dst의 경로
	srcpath=(char*)malloc(sizeof(char)*BUFFER_SIZE);
	dstpath=(char*)malloc(sizeof(char)*BUFFER_SIZE);

	if(argc<3){
		fprintf(stderr,"usage: %s [option] <src> <dst>\n",argv[0]);
		exit(1);
	}
	if(!strcmp(argv[1],"-r")){//-r 옵션인 경우
	}
	else if(!strcmp(argv[1],"-t")){//-t 옵션인 경우
	}
	else if(!strcmp(argv[1],"-m")){//-m 옵션인 경우
	}
	else{//옵션이 없는 경우
		if(argc=!3)
		{
			fprintf(stderr,"usage:%s [option] <src> <dst>\n",argv[0]);
			exit(1);
		}

		/*src, dst 절대경로변환,존재여부 확인*/
		srcpath = real_path(argv[1],srcpath);
		dstpath = real_path(argv[2],dstpath);

		/*src, dst 접근권한 확인*/
		int ret=0;
		if((ret=is_access(srcpath))==0)
		{
			print_usage();
			exit(1);
		}
		if((ret=is_access(dstpath))==0)
		{
			print_usage();
			exit(1);
		}
		/*동기화 작업*/
		ssu_backup(srcpath,dstpath);
	}

	gettimeofday(&end_t,NULL);
	ssu_runtime(&begin_t,&end_t);
	exit(0);
}

void ssu_runtime(struct timeval *begin_t,struct timeval *end_t)
{//프로그램 수행시간 측정 함수
	end_t->tv_sec -= begin_t->tv_sec; //수행시간(초단위)

	if(end_t->tv_usec < begin_t->tv_usec){
		end_t->tv_sec--; //초단위를 하나빼줌
		end_t->tv_usec += SECOND_TO_MICRO;
	}

	end_t->tv_usec -= begin_t->tv_usec; //수행시간 (마이크로 단위)
	printf("Runtime: %ld:%06ld(sec:usec)\n",end_t->tv_sec, end_t->tv_usec);
}
void ssu_backup(char *srcpath, char *dstpath)
{//파일 백업 함수
	struct stat src_stat;
	struct dirent *s_dirp;
//	DIR *d_dp;
	DIR *s_dp;
	char *pch;
	char src[BUFFER_SIZE];//파일 이름
	char *dst;//파일 이름
	int num;//디렉토리 내 파일개수 저장

	/*실제 파일이름 구하기*/
	pch=strrchr(srcpath,'/');
	memset(src,0,BUFFER_SIZE);
	strcpy(src,pch+1);//src 파일이름

	/*src 파일 확인*/
	if(lstat(srcpath,&src_stat)<0){
		fprintf(stderr,"stat error src\n");
		exit(1);
	}

	if(!S_ISDIR(src_stat.st_mode))//디렉토리가 아닐경우. 즉 파일일 경우
	{
		pthread_t tid;
		struct thread_data thread_data_array;
//		struct stat statbuf;

		thread_data_array.dstpath=dstpath;
		thread_data_array.filepath=srcpath;

		//src파일 정보 ssu_sync_file로 옮기기
		if(pthread_create(&tid,NULL,ssu_sync_file,(void *)&thread_data_array)!=0){
			fprintf(stderr,"pthread_create error\n");
			exit(1);
		}
		pthread_join(tid,NULL);
	}

	else{//디렉토리일 경우

/*		chdir(srcpath);//src디렉토리로 이동

		pthread_t tid[num];
		struct thread_data thread_data_array[num];
		struct stat statbuf;//stat 구조체
		int i=0;//임의의 변수
		char filepath[BUFFER_SIZE];
		memset(filepath,0,BUFFER_SIZE);

		if((s_dp=opendir(src))==NULL){//src디렉토리 오픈
			fprintf(stderr,"opendir error for %s\n",src);
			exit(1);
		}

		while((s_dirp=readdir(s_dp))!=NULL)//src디렉토리 읽기. 파일을 다 읽을때까지 반복
		{
			if(stat(s_dirp->d_name,&statbuf)==-1){//src디렉토리 파일의 stat 속성
				fprintf(stderr,"stat error for %s\n",s_dirp->d_name);
				exit(1);
			}

			if((statbuf.st_mode & S_IFMT)==S_IFDIR){//디렉토리인 경우 건너뜀
				continue;
			}

			//디렉토리가 아닌 파일인 경우.
			//dst디렉토리로 파일 복사
			else{
				sprintf(filepath,"%s/%s",srcpath,s_dirp->d_name);
				thread_data_array[i].filepath=filepath;
				thread_data_array[i].dstpath=dstpath;

				//src파일 정보 ssu_sync_file로 옮기기
				if(pthread_create(&tid[i],NULL,ssu_sync_file,(void *)&thread_data_array[i])!=0){
				fprintf(stderr,"pthread_create error\n");
				exit(1);
				}
				pthread_join(tid[i],NULL);
			}
		}
		chdir(savedpath);*/
	}
}

void *ssu_sync_file(void *arg){
	struct utimbuf timebuf;//utimbuf 생성
	struct thread_data *data;//쓰레드함수에 전달할 구조체
	char *filepath;//src파일 경로 저장
	char *dstpath;//dst 파일 경로 저장
	char dst[BUFFER_SIZE];//dst디렉토리 파일명
	char buf[BUFFER_SIZE];
	char filename[BUFFER_SIZE];//src파일의 이름저장
	char *pch;
	time_t mtime;//수정시간
	size_t fsize;//파일 사이즈
	struct stat dst_stat;//dst_stat 구조체 할당
	struct stat src_stat;//src_stat 구조체 할당
	struct dirent *dirp;
	struct stat statbuf;
	DIR *d_dp;
	FILE *fp1;
	FILE *fp2;

	sleep(1);

	data= (struct thread_data*)arg;
	dstpath=data->dstpath;
	filepath=data->filepath;

	/*src file의 stat 구조체*/
	if(lstat(filepath,&src_stat)<0){
		fprintf(stderr,"stat error src\n");
		exit(1);
	}
	//utimbuf에 정보저장
	timebuf.actime = statbuf.st_atime;//파일의 접근시간
	timebuf.modtime = statbuf.st_mtime;//파일의 수정시간
	mtime=statbuf.st_mtime;
	fsize=statbuf.st_size;

	/*src file 오픈*/
	if((fp1=fopen(filepath,"r"))<0){
		fprintf(stderr,"open error for srcfile\n");
		exit(1);
	}
	pch=strrchr(filepath,'/');
	memset(filename,0,BUFFER_SIZE);
	strcpy(filename,pch+1);//dst 파일이름

	//src file의 사이즈 구하기
	fseek(fp1,0L,SEEK_END);
	srcfilesize=ftell(fp1);
	fseek(fp1,0L,SEEK_SET);

	/*dst 디렉토리의 실제 파일이름*/
	pch=strrchr(dstpath,'/');
	memset(dst,0,BUFFER_SIZE);
	strcpy(dst,pch+1);//dst 파일이름

	/*dst 디렉토리 파일인지 확인*/
	if(lstat(dstpath,&dst_stat)<0){
		fprintf(stderr,"stat error dst\n");
		exit(1);
	}

	if(!S_ISDIR(dst_stat.st_mode))//디렉토리 파일이 아니라면 usage 출력
	{
		print_usage();
		exit(1);
	}

	else{//디렉토리 파일일 경우-> 디렉토리 오픈(참)
		//.tmp 파일을 dir 디렉토리에 복사

		if((d_dp=opendir(dst))==NULL){//dst디렉토리 오픈
			fprintf(stderr,"opendir error for dst\n");
			exit(1);
		}

	//	make_tmpfile(d_dp); //tmp 파일 생성

		if((d_dp=opendir(dst))==NULL){//tmp파일 생성시 디렉토리 닫기때문에 다시 오픈
			fprintf(stderr,"opendir error for dst\n");
			exit(1);
		}

		while((dirp=readdir(d_dp))!=NULL)//디렉토리 내 파일 읽기
		{
			chdir(dstpath);//dst파일로 이동
			if(strcmp(dirp->d_name,".") && strcmp(dirp->d_name,"..")){
				if(!strcmp(dirp->d_name,filename)){//파일이름이 같은 파일이 있을경우
					//파일사이즈와 수정시간 비교
					lstat(dirp->d_name,&statbuf);

					/*이름, 사이즈, 시간 같을경우*/
					if((statbuf.st_mtime==mtime) && (statbuf.st_size==fsize))
					{
						return NULL;//덮어씌우지 않는다
					}

					else{//이름은 같으나, 파일사이즈와 수정시간 다름
						/*src 파일에서 데이터 읽어오고 (read) 
						  dst 파일에 데이터 복사 (write) */
						if((fp2=fopen(dirp->d_name,"w+"))<0){
							fprintf(stderr,"open error for dstpath\n");
							exit(1);
						}
						while(srcfilesize>0){
							memset(buf,0,BUFFER_SIZE);
							if(srcfilesize > BUFFER_SIZE){
								fileBufsize=BUFFER_SIZE;
							}
							else{
								fileBufsize=srcfilesize;
							}
							fileReadsize=fread(buf,sizeof(char),fileBufsize,fp1);
							fileWritesize=fwrite(buf,sizeof(char),fileReadsize,fp2);
							srcfilesize-=fileBufsize;
						}//파일 데이터 복사

						//파일 수정시간 똑같이. 사이즈도 변경
						if(utime(dirp->d_name,&timebuf)<0){
							fprintf(stderr,"%s : utime error",dirp->d_name);
							exit(1);
						}

						/*ssu_write_log 
						 파일이름과 바이트수 기록*/
						fclose(fp2);
						return NULL;
					}
				}
			}
			//같은 파일이 없는 경우
		if((fp2=fopen(filename,"w+"))<0){
			fprintf(stderr,"open error for dstpath\n");
			exit(1);
		}
		while(srcfilesize>0){
			memset(buf,0,BUFFER_SIZE);
			if(srcfilesize > BUFFER_SIZE){
				fileBufsize=BUFFER_SIZE;
			}
			else{
				fileBufsize=srcfilesize;
			}
			fseek(fp2,0,SEEK_END);
			fileReadsize=fread(buf,sizeof(char),fileBufsize,fp1);
			fileWritesize=fwrite(buf,sizeof(char),fileReadsize,fp2);
			srcfilesize-=fileBufsize;
			fclose(fp2);
		}//파일 데이터 복사

		//파일 수정시간 똑같이. 사이즈도 변경
		if(utime(filename,&timebuf)<0){
			fprintf(stderr,"%s : utime error",dirp->d_name);
			exit(1);
		}
		return NULL;
		/*ssu_write_log 파일이름과 바이트수 기록*/
		}
	}

	fclose(fp1);
	chdir(savedpath);
	return NULL;
}
/*void make_tmpfile(DIR *dp){//tmpfile 생성. SIGINT를 대비해서 생성
	struct dirent *dirp;
	char tmpfile[BUFFER_SIZE];
	char buf[BUFFER_SIZE];
	FILE *fp1;
	FILE *fp2;
	memset(tmpfile,0,BUFFER_SIZE);

	while((dirp=readdir(dp))!=NULL){//dst 디렉토리 읽기
		if(strcmp(dirp->d_name,".")&&strcmp(dirp->d_name,"..")){
			sprintf(tmpfile,"%s.tmp",dirp->d_name);
			if((fp1=fopen(dirp->d_name,"r"))<0){
				fprintf(stderr,"open error for dst orgin file\n");
				exit(1);
			}
			if((fp2=fopen(tmpfile,"w+"))<0){
				fprintf(stderr,"open error for dst tmpfile\n");
				exit(1);
			}

			fseek(fp1,0L,SEEK_END);
			srcfilesize=ftell(fp1);
			fseek(fp1,0L,SEEK_SET);

			while(srcfilesize>0){//파일 데이터 읽어서 .tmp파일에 저장
				memset(buf,0,BUFFER_SIZE);
				if(srcfilesize > BUFFER_SIZE){
					fileBufsize=BUFFER_SIZE;
				}
				else{
					fileBufsize=srcfilesize;
				}
				fileReadsize=fread(buf,sizeof(char),fileBufsize,fp1);//원래 파일에서 읽기
				fileWritesize=fwrite(buf,sizeof(char),fileReadsize,fp2);//tmp파일에 복사
				srcfilesize-=fileBufsize;
			}//파일 데이터 복사
		}
	}
	closedir(dp);
	return;
}
*/
char* real_path(char *filename,char *tmp)
{
	if(filename[0]=='.')//상대경로인 경우
	{
		if(realpath(filename,tmp)==NULL)//실제로 존재하는지 확인
		{
			print_usage();
		//	fprintf(stderr,"this file doesn't exist.\n");
			exit(1);
		}

		return tmp;
	}
	else if(filename[0]=='/')//절대경로인 경우
	{
		if(access(filename,F_OK)!=0)
		{
			print_usage();
			exit(1);
		}
		else
			return filename;//realpath할 필요없음
	}
	else
	{
		if(realpath(filename,tmp)==NULL)
		{
			print_usage();
			exit(1);
		}

		return tmp;
	}
}

int is_access(char *path)//해당 경로 존재 여부 확인
{
	if(access(path,R_OK)==0)
	{
		if(access(path,W_OK)==0)
		{
			return 1;
		}
	}

	//접근권한 없는 경우
	return 0;
}

void print_usage()//사용법 출력 함수
{
	printf("Usage : ssu_rsync [option] <src> <dst>\n");
	printf("Option : \n");
	printf(" -r			syncronized to subdir in <src>\n");
	printf(" -t			files syncronize at once\n");
	printf(" -m			delete files that are only in <dst>\n");
}

