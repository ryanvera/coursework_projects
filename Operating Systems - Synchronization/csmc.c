// 11/19/2023

/* Authorship
// RYAN VERA rdv170000
// created fundamental code thread structure
// including the fundamentals of the student, tutor, and coordinator threads and how they interact
// checked Justin's code

// JUSTIN AEGTEMA jtb200002
// worked on details including adding many of the semaphores which make the values reach the correct values
// created required print statements and several variables required for them
// added semaphores to improve flow
// checked Ryan's code

*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <assert.h>
// #define NDEBUG

//semaphores and locks
sem_t studentSema;
sem_t coordinatorWaiting;
sem_t tutorWaiting;
pthread_mutex_t chairLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t newArrivalQueueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t arrivalOrderLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pqLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t totalCoordRequestsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t studentIDLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t studentsDoneLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tutorIDLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t totalTutoringSessionsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t currentHelpLock = PTHREAD_MUTEX_INITIALIZER;

//user input
int studentNum = -1;
int tutorNum = -1;
int maxChair = -1;
int maxHelps = -1;

//other variables
int chairOpen = -1;
int studentID = 0;
int studentsDone = 0;
int allStudentsDone = 0;
int totalCoordRequests = 0;
int tutorID = 0;
int totalTutoringSessions = 0;
int currentlyReceivingHelp = 0;

//structures
struct student **pq = NULL;
struct student *queueHead;
struct student *queueTail;
int waitingQueueSize;     

struct tutor {
	int idNum;
	int sessionsTutored;
	sem_t tutorSema1;
};

struct student {
	int idNum;
	int helpReceived;
	sem_t studentSema;
	int myTutorID;
	struct student *next;
    struct student *waitingQueueNext;
	struct tutor *myTutorPtr;
};

// programming() simulates time the student spends programming as per project specs
void programming()
{
	// Create random number
	srand(time(NULL));
	int num = rand();
	num = num % 2001;
	assert(num < 2001);

	struct timespec programmingTime;
	programmingTime.tv_sec = 0; // 0 seconds
	programmingTime.tv_nsec = num; // 0-2000 nanoseconds, so up to 2 microseconds

	// printf("Doing programming...\n");
	nanosleep(&programmingTime,NULL);
}

// sleep200Nano() helps simulate tutoring as per project specs
void sleep200Nano()
{
	struct timespec tutorTime;
	tutorTime.tv_sec = 0;	//0 seconds
	tutorTime.tv_nsec = 200; //200 nanoseconds = 0.2 microseconds
	nanosleep(&tutorTime, NULL);
}


// NEW ARRIVAL QUEUE 
// NEW ARRIVAL QUEUE stores students who found a chair but are waiting for the coordinator to add them to the priority queue
// printNewArrivalQueue() prints the new arrivl queue
// JA: note that you should use pthread_mutex_lock(&newArrivalQueueLock) before calling printNewArrivalQueue()
// JA: unlike the other newArrivalQueue functions, this one does not and cannot call pthread_mutex_lock(&newArrivalQueueLock) within itself
void printNewArrivalQueue()
{
    //printf("NewArrivalQueue will be printed\n");

	struct student *temp = queueHead;

	for(int i = 0; i < waitingQueueSize; i++)
	{
		//printf("NewArrivalQueue: Index: %d | Student ID: %d\n", i, temp->idNum);
		temp = temp->waitingQueueNext;
	}
	//printf("Total students in NewArrivalQueue: %d\n", waitingQueueSize);
}

// add a new student to the NEW ARRIVAL QUEUE
int addNewArrivalQueue(struct student *s)
{
	pthread_mutex_lock(&newArrivalQueueLock);

	// Start of waiting queue
	if(waitingQueueSize == 0)
	{
		queueHead = s;
		queueTail = s;
		waitingQueueSize++;
	}
	// Waiting queue has items inside
	else
	{
		queueTail->waitingQueueNext = s;
		queueTail = s;
		s->waitingQueueNext = NULL;
		waitingQueueSize++;
	}

    //printNewArrivalQueue();
	pthread_mutex_unlock(&newArrivalQueueLock);
	return 1;
}

// get the next student from the NEW ARRIVAL QUEUE
struct student* getNewArrivalQueue()
{
	pthread_mutex_lock(&newArrivalQueueLock);
	struct student* temp = queueHead;
	queueHead = queueHead->waitingQueueNext;
	temp->waitingQueueNext = NULL;
	waitingQueueSize--;
	pthread_mutex_unlock(&newArrivalQueueLock);
	return temp;
}

// PRIORITY QUEUE
// PRIORITY QUEUE stores the students as sorted by the coordinator
// it first prioritizes students who have received less help, and then prioritizes them based on the earliest to arrive

// printPQ() prints the priority queue in its current form
// JA: Note that you should call pthread_mutex_lock(&pqLock) before calling printPQ()
// JA: unlike other PQ functions, this one does not and cannot call pthread_mutex_lock(&pqLock) within itself
void printPQ()
{
	struct student* temp = NULL;

	// For each level
	for(int i = 0; i < maxHelps; i++)
	{
		temp = pq[i];	// Start at the head

		// Traverse a level only printing valid students
		while(temp != NULL && temp->idNum != 0)
		{
			//printf("printPQ: Student ID %d | level %d of %d | helpReceived %d of %d\n", temp->idNum, i, maxHelps-1, temp->helpReceived, maxHelps);
			temp = temp->next;
		}
	}
}

// addPQ() adds a student to the PRIORITY QUEUE
void addPQ(struct student *s)
{
	pthread_mutex_lock(&pqLock);
	struct student *trav;	// Pointer used to traverse the PQ
	int h = s->helpReceived;
    //printf("addPQ: Student %d | helpsReceived %d of %d\n", s->idNum, s->helpReceived, maxHelps);
    //printf("addPQ: pq[%d] == %p\n", h, pq[h]);
    // printf("addPQ: pq[%d]->idNum == %d\n", h, pq[h]->idNum);

	// If level h of array is empty add to the front
	if(pq[h] == NULL || pq[h]->idNum == 0)
    {
		pq[h] = s;
        //printf("Student %d added to PQ head at level %d\n", s->idNum, h);
    }
	else
	{
		// Start on correct priority level
		trav = pq[h];

		// Go to the end of the level
		while(trav->next != NULL)
		{
			trav = trav->next;
		}
			
		// Add new student to end
		trav->next = s;
		s->next = NULL;
        //printf("Student %d added to PQ at level %d\n", s->idNum, h);
	}

    //printf("Printing PQ\n");
    //printPQ();
	pthread_mutex_unlock(&pqLock);
}

// getPQ() gets the next prioritized student from the PRIORITY QUEUE
struct student* getPQ()
{
	pthread_mutex_lock(&pqLock);
	struct student* ret = NULL;		// Used to hold the desired student
	struct student* temp = NULL;	// Used to traverse and hold on to the current pointer

	for(int i = 0; i < maxHelps; i++)
	{
        // assert(temp->idNum != 0);

		// Start at the front of the PQ
		temp = pq[i];

		// If level is empty, continue
		if (temp == NULL) continue;

		// printf("Test: Addy: %p | ID %d | helpRecevied: %d | next: %p\n", temp, temp->idNum, temp->helpReceived, temp->next);

		// If invalid student is present, continue
		if(temp->idNum == 0)
			continue;

		ret = temp;				// Set return value
		//printf("getPQ returning Student ID %d\n", ret->idNum);
		pq[i] = temp->next;		// Assign new head of level
		temp->next = NULL;		// Disconnect old pointer
		break;
	}

    //printPQ();
	pthread_mutex_unlock(&pqLock);
	return ret;
}

// student() function simulates a single student until they have received all of their tutoring
void* student()
{
	// Set student struct data
	struct student s;
	pthread_mutex_lock(&studentIDLock);
	studentID++;
	s.idNum = studentID;
    assert(s.idNum != 0); // To ensure that a student ID cannot be 0
	pthread_mutex_unlock(&studentIDLock);
    
	s.helpReceived = 0;
	s.next = NULL;
    s.waitingQueueNext = NULL;      // Ryan
	s.myTutorPtr = NULL;
	s.myTutorID = -1;
	sem_init(&(s.studentSema), 0, 0);

	//printf("Student %d created...\n", s.idNum);

	while(1)
	{
		// Do work
		programming();

		// Chech for open waiting room chairs, if none leave and come back
		pthread_mutex_lock(&arrivalOrderLock);
		pthread_mutex_lock(&chairLock);
		if(chairOpen == 0)
		{
			printf("S: Student %d found no empty chair. Will try again later.\n", s.idNum);
			pthread_mutex_unlock(&chairLock);
			pthread_mutex_unlock(&arrivalOrderLock);
            sleep(0.5);
			continue;
		}

		// student takes a chair
		chairOpen--;
		printf("S: Student %d takes a seat. Empty chairs = %d.\n", s.idNum, chairOpen);
        pthread_mutex_unlock(&chairLock);

		// Wait in the wait room
        //printf("Student %d is entering waiting room queue\n", s.idNum);
        addNewArrivalQueue(&s);
        // printf("Student %d is now in waiting room queue\n", s.idNum);
		pthread_mutex_unlock(&arrivalOrderLock);

		// Wake up coordinator
		//printf("Student %d waking coordinator\n", s.idNum);
		sem_post(&coordinatorWaiting);

		// Wait on tutor
		//printf("Student %d waiting tutor\n", s.idNum);
		sem_wait(&(s.studentSema));

		// Un-occupy waiting room chair
		pthread_mutex_lock(&chairLock);
		chairOpen++;
		//printf("Student %d leaving chair | Chairs remaining: %d of %d\n", s.idNum, chairOpen, maxChair);
		pthread_mutex_unlock(&chairLock);

		pthread_mutex_lock(&currentHelpLock);
		currentlyReceivingHelp++;
		pthread_mutex_unlock(&currentHelpLock);

		// now that student has left the chair and is included in "currently receiving help", allow the tutor to proceed in tutoring
		sem_post(&(s.myTutorPtr->tutorSema1));

		// tutoring occurs
        sleep200Nano();
		s.helpReceived++;

		pthread_mutex_lock(&currentHelpLock);
		currentlyReceivingHelp--;
		pthread_mutex_unlock(&currentHelpLock);
		
		// wait to print until after the tutor is also done tutoring and ready to print
		sem_wait(&(s.studentSema));
		printf("S: Student %d received help from Tutor %d.\n", s.idNum, s.myTutorID);
		//printf("S: Student %d received help from tutor %d. | Helps recevived: %d of %d\n", s.idNum, s.myTutorID, s.helpReceived, maxHelps);

		// get ready for a new tutor next time
		s.myTutorPtr = NULL;
		s.myTutorID = -1;

		// If all helps received, leave
		if (s.helpReceived == maxHelps)
		{
			pthread_mutex_lock(&studentsDoneLock);
			studentsDone++;
			pthread_mutex_unlock(&studentsDoneLock);
			//printf("Student %d is done\n", s.idNum);

			if(studentsDone == studentNum)
            {
                // printf("All students are done\n");
                allStudentsDone = 1;
            }
			break;
		}
	}

	// printf("Student %d ending\n", s.idNum);
    pthread_exit(NULL);
}


// coordinator() simulates the coordinator, managing students and tutors
void* coordinator()
{
	//printf("Coordinator created...\n");

	while(1)
	{
		if(allStudentsDone) break;

		// Wait for student
		//printf("Coordinator is waiting for students\n");
		sem_wait(&coordinatorWaiting);

        if(allStudentsDone) break;

		// Get student from waiting room
		//printf("Coordinator is getting student\n");
        struct student* s = getNewArrivalQueue();
        //printf("Coordinator got student %d from the new arrival queue\n", s->idNum);

        // Add student to PQ
        addPQ(s);

		// this totalCoordRequestsLock is technically unnecessary since it could effectively be applied with chairLock
		// however, it was kept in order to make the code robust
		pthread_mutex_lock(&totalCoordRequestsLock);
		totalCoordRequests++;
		pthread_mutex_lock(&chairLock);
		int waitingStudents = maxChair - chairOpen;
		printf("C: Student %d with priority %d added to the queue. Waiting students now = %d. Total requests = %d\n", s->idNum, s->helpReceived, waitingStudents, totalCoordRequests);
		pthread_mutex_unlock(&chairLock);
		pthread_mutex_unlock(&totalCoordRequestsLock);

		// Wake up tutor
		//printf("Coordinator is waking tutor\n");
		sem_post(&tutorWaiting);
	}

	// tell the tutors they can be done since no more students
	for (int i = 0; i < tutorNum; i++)
	{
		sem_post(&tutorWaiting);
	}

	//printf("Coordinator ending\n");
    pthread_exit(NULL);
}


// tutor() simulates a single tutor, helping a single student as prioritized by the coordinator() and the priority queue
void* tutor()
{
	struct tutor t;
	pthread_mutex_lock(&tutorIDLock);
	tutorID++;
	t.idNum = tutorID;
	pthread_mutex_unlock(&tutorIDLock);
	t.sessionsTutored = 0;
	sem_init(&(t.tutorSema1), 0, 0);

	//printf("Tutor %d created\n", t.idNum);

	while(1)
	{
		if(allStudentsDone) break;

		// Wait for student
		//printf("Tutor %d is waiting\n", t.idNum);
		sem_wait(&tutorWaiting);
        if(allStudentsDone) break;
		//printf("Tutor %d is awake\n", t.idNum);

		// Wake up student from priority queue
        //printf("Tutor %d is getting student\n", t.idNum);
		struct student *myStudent = getPQ();
		if(myStudent == NULL)
		{
			//printf("Tutor %d sees no students. Continue\n", t.idNum);
			continue;
		}

		myStudent->myTutorID = t.idNum; //tell student their tutor's ID
		myStudent->myTutorPtr = &t; // tell student who is tutoring them
		//printf("Tutor %d got student %d\n", t.idNum, myStudent->idNum);
		sem_post(&(myStudent->studentSema));  // let student get out of the chair and enter "currently tutoring" status

		sem_wait(&(t.tutorSema1)); // wait until student is out of chair and "currently tutoring"

		sleep200Nano(); // Tutor student
		t.sessionsTutored++;

		//this totalTutoringSessionsLock is technically unnecessary because totalTutoringSessions++ could simply be placed inside the currentHelpLock
		//however totalTutoringSessionsLock is kept here for thoroughness and to make the code robust
		pthread_mutex_lock(&totalTutoringSessionsLock);
		totalTutoringSessions++;
		pthread_mutex_lock(&currentHelpLock);
		sem_post(&(myStudent->studentSema)); // now that totalTutoringSessions is accurate and tutor is ready to print, let student print that they have finished tutoring
		printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d\n", myStudent->idNum, t.idNum, currentlyReceivingHelp, totalTutoringSessions);
		pthread_mutex_unlock(&currentHelpLock);
		pthread_mutex_unlock(&totalTutoringSessionsLock);


	}

	//printf("Tutor %d ending\n", t.idNum);
    pthread_exit(NULL);
}


int main(int argc, char *argv[])
{
	// Check for correct amount of arguments
	if (argc != 5)
	{
		//printf("Wrong number of arguments. 4 arguments are needed.\n");
		return 0;
	}

	// Get parameters
	studentNum = atoi(argv[1]);
	tutorNum = atoi(argv[2]);
	maxChair = atoi(argv[3]);
	maxHelps = atoi(argv[4]);

	chairOpen = maxChair;

    // Create priority queue - Ryan
	pq = (struct student**) malloc(maxHelps * sizeof(struct student));
	for(int i = 0; i < maxHelps; i++)
    {
		// pq[i] = (struct student*) malloc(sizeof(struct student));
        pq[i] = NULL;
    }

	// Initialize the waiting room queue - Ryan
	queueHead = NULL;
	queueTail = NULL;
	waitingQueueSize = 0;

//-------------------------------------------------------------------

	// Create waiting room semaphore
	int retCoor = sem_init(&coordinatorWaiting, 0, 0);
	assert(retCoor == 0);

	// Create semaphore for tutors
	// int retTut = sem_init(&tutorWaiting, 0, tutorNum);
	int retTut = sem_init(&tutorWaiting, 0, 0);
	assert(retTut == 0);

//------------------------------------------------------------------

	// Statically create single coordinator thread
	pthread_t coordinatorThread;
	int retVal = pthread_create(&coordinatorThread, NULL, coordinator, NULL);
	//pthread_create(&coordinatorThread, NULL, coordinator, NULL);
	assert(retVal == 0);

	// Dynamically create student threads
	pthread_t *studentThreads = (pthread_t*) malloc(studentNum * sizeof(pthread_t));
	for(int i = 0; i < studentNum; i++)
	{
		int ret = pthread_create(&studentThreads[i], NULL, student, NULL);
		//pthread_create(&studentThreads[i], NULL, student, NULL); //this line is for if you want to skip the below assert
		assert(ret == 0);
	}

	// Dynamically create tutor threads
	pthread_t *tutorThreads = (pthread_t*) malloc(tutorNum * sizeof(pthread_t));
	for(int i = 0; i < tutorNum; i++)
	{
		int ret = pthread_create(&tutorThreads[i], NULL, tutor, NULL);
		//pthread_create(&tutorThreads[i], NULL, tutor, NULL); // this line is for if you want to skip the below assert
		assert(ret == 0);
	}

	// Join student threads
	for(int i = 0; i < studentNum; i++)
	{
        void* ret;
		int retS = pthread_join(studentThreads[i], &ret);
		assert(retS == 0);
        free(ret);
	}
	// printf("Finished student thread joins.\n");

	sem_post(&coordinatorWaiting);

	// Join coordinator thread
    void* ret;
	int retC = pthread_join(coordinatorThread, &ret);
	assert(retC == 0);
    free(ret);
	// printf("Finished coordinator thread join.\n");

	// Join tutor threads
	for(int i = 0; i < tutorNum; i++)
	{
        void* ret;
		int retT = pthread_join(tutorThreads[i], &ret);
		assert(retT == 0);
        free(ret);
	}
	// printf("Finished tutor thread joins.\n");
	// printf("Program has finished.\n");
	return 0;
}