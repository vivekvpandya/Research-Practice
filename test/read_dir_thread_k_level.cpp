/* Simple program to read content of diretory 
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <cstring>
#include <dirent.h>
#include <cerrno>
#include <cstdio>
#include <limits.h>
#include <cstdlib>
#include <pthread.h>
#include <openssl/md5.h>
#include <map>
#include <string>
#include <vector>
//#include "vt_user.h"

typedef struct argument {
	std::string dirPath;
	int argNum;
	std::string childDir;
} argS;  // struct to store directory path, initial thread number, child directory prefix

typedef struct generateMD5Arg {
	std::string filePath;
	unsigned char ** digest;
} genMD5arg; // struct to be used as inout parameter for thread generateMD5

std::map<int,std::string> dirRootMap; // a map from initial thread number to directory path 

typedef struct stat fStat; 

typedef std::map<int,std::string>::iterator dr_it; // dirRootMap iterator

typedef std::vector<int> tVector; // vector to hold for thread number 

std::map<std::string,tVector> fileHash; // global file name hash table

std::map<std::string,tVector> dirHash; // global directories name hashtable

typedef std::map<std::string,tVector>::iterator fh_it; // fileHash and dirHash iterator

typedef std::map<long, tVector> fileSizeMap; // a map from filesize to thread numbers (which have reported same size)

typedef std::map<long, tVector>::iterator fs_it; // fileSizeMap iterator

typedef std::map<std::string,int> md5Map; // a map from MD5 string to number of files having same MD5 hashtable

typedef std::map<std::string,int>::iterator md5Map_it; // iterator for md5Map

pthread_mutex_t lock; // lock variable 

int threshold = 0; // this will be used as k/2 

static const char hexchars[] = "0123456789abcdef";  // This array is used for MD5 to string conversion

bool identical = true; 
bool divergent = true;

/*
	desc: simple function that converts unsigned char * 
		  to string .
	input : unsigned char *
	output : string 
*/
inline std::string toString(unsigned char *md5) {
	std::string result;
	for (int i=0; i < MD5_DIGEST_LENGTH ; i++) {
		unsigned char b = md5[i];
		char hex[3];
		
		hex[0] = hexchars[b >> 4]; // divide by 16
		hex[1] = hexchars[b & 0xF]; // mod by 16
		hex[2] = 0; 
		result.append(hex);
	}
	return result;
}

/*
	desc: simple function that compares size of files.
	input: fileName and a tVector (pass by reference)
	output: modifies argVector such that threadIDs which do not have same size for given file 
			is deleted 
*/
void compareFileSize(std::string fileName, tVector &argVector) {	
	fileSizeMap *sizeHash = new fileSizeMap;
	fs_it fsIt;
	fStat *fileAttribute = new fStat;
	std::string fullPath;
	
	for (int i = 0; i < argVector.size() ; i++) {
		fullPath.clear();
		fullPath.append(dirRootMap[argVector[i]]);
		fullPath.append("/");
		fullPath.append(fileName);
		
		stat(fullPath.c_str(),fileAttribute);
		
		long size = fileAttribute->st_size;
		fsIt = sizeHash->find(size);
		if (fsIt != sizeHash->end()) {
			// File Size match found
			(*sizeHash)[size].push_back(argVector[i]);
		}
		else {
			(*sizeHash)[size] = tVector();
			(*sizeHash)[size].push_back(argVector[i]);
		}
	}
// Pick up first size for which at least threshold threads have in tVector
// argVector is pass by reference so it will edit original vector
	argVector.clear();
	
	for (fs_it it = sizeHash->begin(), end = sizeHash->end(); it != end ; it++) {
		tVector tvec = (tVector)it->second;
		if (tvec.size() >= threshold) {
			for( auto t : tvec) {
				int tID = (int)t;
			 	argVector.push_back(tID);
			}
			break; 
		}
	}

	delete fileAttribute;
	delete sizeHash;
}

/*
	desc: simple function that generates MD5 hash of file content.
	input: filePath
	output: allocates char * on heap and fills array for MD5 hash 
			memory will be released by calling thread
*/
void *generateMD5(void *arg) {
	genMD5arg * genMD5argT = (genMD5arg *)arg;
	char *filePath = (char *)genMD5argT->filePath.c_str();
	FILE *inFile = fopen(filePath,"rb");
	
	if(inFile == NULL) {
		perror(filePath);
		return (void *)-1;
	}
	
	MD5_CTX *mdContext = new MD5_CTX;
	int bytes;
	unsigned char data[1024];

	MD5_Init(mdContext);

	while((bytes = fread(data, 1,1024,inFile)) != 0)
		MD5_Update(mdContext,data,bytes);
		
	MD5_Final(*(genMD5argT->digest),mdContext);
	
	delete mdContext;
	fclose(inFile);
	pthread_exit((void *)0);
}

/*
	desc: simple function that compares MD5 hash of file content.
	input: fileName and tVector
	output: true if at least k/2 files have same content, false otherwise
*/
bool compareFileContent(std::string fileName, tVector &argVector) {	
	bool retVal = false;
	
	md5Map *md5MapT = new md5Map;
	md5Map_it md5it;
	
	int size = argVector.size();
	pthread_t *threadID = new pthread_t[size];
	void ** digests = new void * [size];
	
	
	genMD5arg ** genMD5argV = new genMD5arg * [size];
	unsigned char ** md5digests = new unsigned char * [size];
	
	for (int i = 0; i < size ; i++) {
		std::string fullPath;
		fullPath.append(dirRootMap[argVector[i]]);
		fullPath.append("/");
		fullPath.append(fileName);
		genMD5argV[i] = new genMD5arg;
		genMD5argV[i]->filePath = fullPath;
		md5digests[i] = new unsigned char [MD5_DIGEST_LENGTH];
		genMD5argV[i]->digest = &md5digests[i];
		pthread_create(&threadID[i],NULL,generateMD5,(void *)genMD5argV[i]); // at max argc - 1 threads can be spawned 
	}
	
	for (int i=0; i<size; i++) {
		pthread_join(threadID[i],NULL);
	}
		
		
	for (int i=0; i<size; i++) {
		std::string str = toString((unsigned char *)md5digests[i]);
		md5it = md5MapT->find(str);
		if (md5it != md5MapT->end()) {
			// content matches
			(*md5MapT)[str] = (*md5MapT)[str] + 1; // just increment count
		}
		else {
			(*md5MapT)[str] = 1; // initialize 
		}
	}
	delete [] digests;
	delete [] md5digests;
	delete [] genMD5argV;
	
	for (md5Map_it i = md5MapT->begin(), e = md5MapT->end() ; i != e ; i++) {
		if ( i->second >= threshold ) {
			retVal = true;
			break; 
		}
	}

	delete md5MapT;
	return retVal;
}

/* 
	simple function to read the directory content.
	This function will be executed by a thread for each directory.
	It updates the gloab hash table at the end of the traversal.
*/
void *readDir(void *arg)
{	DIR *dirP;
	struct dirent *dirp;
	argS *sarg = (argS *)arg;
	char *dirPath = (char *)sarg->dirPath.c_str();
	int argNum = sarg->argNum;
	std::vector<std::string> *files = new std::vector<std::string>;
	std::vector<std::string> *dirs = new std::vector<std::string>;
	
	if(sarg->childDir == "") {
	pthread_mutex_lock(&lock);
		dirRootMap[argNum] = sarg->dirPath;
	pthread_mutex_unlock(&lock);
	}
		
	if((dirP = opendir(dirPath)) == NULL) {	
		perror(dirPath);	
	}
	else {	
		while((dirp = readdir(dirP) )!= NULL) {
			if(strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name,"..") == 0 )
				continue;
			else {	
				if(dirp->d_type == DT_DIR) {
					if(sarg->childDir != "") {
						
						std::string dirName = std::string(dirp->d_name);
						std::string newChildDir = sarg->childDir + "/" + dirName;
						std::string newDirPath = sarg->dirPath+ "/" + dirName;
						dirs->push_back(newChildDir);
					}
					else {	
						std::string newChildDir = std::string(dirp->d_name);
						std::string newDirPath = sarg->dirPath + "/" + newChildDir;
						dirs->push_back(newChildDir);
					}

				}
				else {
					if(sarg->childDir != "") {
						std::string childDir = std::string(sarg->childDir);
						std::string fileName = std::string(dirp->d_name);
						std::string fullFileName = sarg->childDir + "/" + fileName;
						files->push_back(fullFileName);
					}
					else {
						files->push_back(std::string(dirp->d_name));
					}	
					
				}		
			}
		}
	}
	
	if(dirP != NULL)
	closedir(dirP);	
	
	/* Critical Section */
	pthread_mutex_lock(&lock);
	fh_it it;
	for (auto t : *files) {	
		it = fileHash.find(t);
		if(it != fileHash.end()) {
			fileHash[t].push_back(argNum);
		}
		else {
			fileHash[t] = tVector();
			fileHash[t].push_back(argNum);
		}
	}
	// Add found directories
	for (auto d : *dirs) {
		it = dirHash.find(d);
		if (it != dirHash.end()) {
			dirHash[d].push_back(argNum);
		}
		else {
			dirHash[d] = tVector();
			dirHash[d].push_back(argNum);
		}
	}
	pthread_mutex_unlock(&lock);
	/* Critical Section Ends */

	delete dirs;
	delete files;
	delete sarg;
	return ((void *)0);
}

/* 
	simple helper function to create threads for given directory.
*/

void spawnThreadForDir(std::string dirPath, tVector &tvec) {
	int size = tvec.size();
	pthread_t *threadID = new pthread_t[size];
	
	for (int i = 0 ; i < size ; i++) {
		argS *argD = new argS;
		argD->dirPath = dirRootMap[tvec[i]] + "/" + dirPath;
		argD->argNum = tvec[i];
		argD->childDir = dirPath;
		pthread_create(&threadID[i],NULL,readDir,(void *)argD);
	}
	
	for (int i = 0 ; i < size ; i++) {
		pthread_join((pthread_t)threadID[i],NULL);
	}
	delete [] threadID;
}

int main(int argc, char **argv) {
	if((argc == 2) && (strcmp("--help", argv[1]) == 0)) {
		printf("read_dir : simply reads the content of specified directories.\n");
		printf("syntax: read_dir dir_path1 dir_path2 ... \n");
	}
	if(argc < 3) {
		printf("please provide atleast 2 directory path. \nTry passing --help option for more details. \n");
	}
	else {		
		int err;
		threshold = argc / 2; 
		pthread_t *ntid = new pthread_t [argc];
		pthread_mutex_init(&lock,NULL);
		
		for(int i = 1; i < argc ; i++) {	
			argS *sarg = new argS;
			sarg->dirPath = std::string(argv[i]);
			sarg->argNum = i;
			sarg->childDir = std::string(""); // for initial argument childDir is set to NULL
			err = pthread_create(&ntid[i],NULL,readDir,(void*)sarg);
			
			if(err != 0) {
				printf("can not create threads\n");
				exit(-1);
			}
		}
				
		for(int i = 1 ; i < argc; i++) {
			pthread_join(ntid[i],NULL);
		}
		
		delete [] ntid;

		printf("Reading from hashtable: \n");
		int count;
		bool needToProcess = true;
		int level = 0;
		
		while (needToProcess) {
		// First process all majority copy files at given level
		printf("\v------------------------------------\n");
		printf(" Majority copy files at level %d    |\n",level);
		printf("------------------------------------\n\v");
		
		for(fh_it fhIt = fileHash.begin(); fhIt != fileHash.end(); fhIt++) {
			tVector tvec = fhIt->second;
			count = tvec.size();
			if (count >= threshold) {
				compareFileSize(fhIt->first,tvec);
				count = tvec.size();
				if (count >= threshold) {
					if (compareFileContent(fhIt->first,tvec)) {
						divergent = false; // atleast one file is K Major copy
						printf("%s\n",(fhIt->first).c_str());
					}
					else {
						// atleast one file differs
						identical = false;
					}
				}
			}
		}
		
		fileHash.clear(); // releasing memory allocated but it costs O(n) time 
		// Now spawn threads for each mejority copy directories found at current level
		std::map<std::string,tVector> dirHashCopy(dirHash); // locally operate on copy
		dirHash.clear(); // releasing memory allocated but it costs O(n) time
		needToProcess = false;
		level++;
		for (fh_it dirIt = dirHashCopy.begin(); dirIt != dirHashCopy.end(); dirIt++) {
			if (dirIt->second.size() >= threshold) {
			spawnThreadForDir(dirIt->first,dirIt->second);
			needToProcess = true;
			}
		}
	}
	if (divergent) {
		printf("\v Input file systems are completely divergent.\n");
	}
	else {
		if (identical) {
			printf("\v Input file systems are identical.\n");
		}
	}
	}
	return 0;
}

