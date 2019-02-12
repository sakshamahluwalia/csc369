/**
* CSC369 Assignment 2
*
* This is the source/implementation file for your safe stop sign
* submission code.
*/
#include "safeStopSign.h"

void initSafeStopSign(SafeStopSign* sign, int count) {
	// Initialize SafeStopSign given number of cars in simulation (count)
	initStopSign(&sign->base, count);

	// TODO: Add any initialization logic you need.
	// **** Where to initialize and how many? *****
	// pthread_cond_t enterLaneEmpty = PTHREAD_COND_INITIALIZER;
	// pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
}

void destroySafeStopSign(SafeStopSign* sign) {
	destroyStopSign(&sign->base);

	// TODO: Add any logic you need to clean up data structures.
}

void runStopSignCar(Car* car, SafeStopSign* sign) {

	// TODO: Add your synchronization logic to this function.

	// Get the lock
// 	pthread_mutext_lock(&mutex);

// 	// Get the lane the car is trying to go through and enter
// 	EntryLane* lane = getLane(car, &sign->base);

// // Add sync here for lane.

// 	// Car enters lane and is given token
// 	enterLane(car, lane);

// 	// Block if previous car has not exited the entry lane
// 	while (lane.enterCounter != lane.exitCounter){
// 		pthread_cond_wait(&enterLaneEmpty, &mutex);
// 	}

// 	// **** Checking collision detection handled here or already handled? ****

// 	// Go through the stop sign
// 	goThroughStopSign(car, &sign->base);

// 	// Exit
// 	exitIntersection(car, lane);

// 	// Car exited so signal next car waiting on lane
// 	pthread_cond_signal(&laneEmpty);

// 	// Unlock
// 	pthread_mutex_unlock(&mutex);
}
