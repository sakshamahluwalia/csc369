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

	for (int i = 0; i < DIRECTION_COUNT; ++i)
	{
		initMutex(&sign->lane[i]);
		initMutex(&sign->quad[i]);
	}
	// TODO: Initialize condition variables
}

void destroySafeStopSign(SafeStopSign* sign) {
	destroyStopSign(&sign->base);

	// TODO: Add any logic you need to clean up data structures.
	for (int i = 0; i < DIRECTION_COUNT; ++i)
	{
		destroyMutex(&sign->lane[i]);
		destroyMutex(&sign->quad[i]);
	}
	// TODO: Destroy condition variables
}

void runStopSignCar(Car* car, SafeStopSign* sign) {

	int dir = getLaneIndex(car);

	// Get the lane the car is trying to go through and enter
	EntryLane* lane = getLane(car, &sign->base);

	// Get the quadrants the car will go through
	int quad_count;
	quad_count = getStopSignRequiredQuadrants(car, sign->quad_indexes);

	/*** Add sync here for lane. ***/

 	// TODO: Wait if previous car has not exited the entry lane

	// TODO: Wait if the quadrants are not empty

	// get the lock
	lock(&sign->lane[dir]);

	int index;
	for (int i = 0; i < quad_count; ++i)
	{
		index = sign->quad_indexes[i];
		lock(&sign->quad[index]);
	}

	// Car enters lane and is given token
	enterLane(car, lane);

	// Go through the stop sign
	goThroughStopSign(car, &sign->base);

	// Exit
	exitIntersection(car, lane);

	// Unlock
	for (int i = 0; i < quad_count; ++i)
	{
		index = sign->quad_indexes[i];
		unlock(&sign->quad[index]);
	}

	unlock(&sign->lane[dir]);

	// Car exited so signal next car waiting on lane
	// pthread_cond_signal(&laneEmpty);
}
