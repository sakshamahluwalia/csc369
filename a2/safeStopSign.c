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
	initMutex(&sign->intersection_lock);

	for (int i = 0; i < DIRECTION_COUNT; ++i)
	{
		sign->quad[i].is_quad_locked = 0;
		sign->lanes[i].queue_index = 0;
		initMutex(&sign->lanes[i].lane_lock);
		initMutex(&sign->quad[i].quad_lock);
		initConditionVariable(&sign->quad[i].quad_condition);
		sign->lanes[i].car_queue = malloc(sizeof(Car *) * count);
	}
}

void destroySafeStopSign(SafeStopSign* sign) {
	destroyStopSign(&sign->base);

	destroyMutex(&sign->intersection_lock);

	// TODO: Add any logic you need to clean up data structures.
	for (int i = 0; i < DIRECTION_COUNT; ++i)
	{
		destroyMutex(&sign->lanes[i].lane_lock);
		destroyMutex(&sign->quad[i].quad_lock);
		destroyConditionVariable(&sign->quad[i].quad_condition);
		free(sign->lanes[i].car_queue);
	}

}

void runStopSignCar(Car* car, SafeStopSign* sign) {
	int dir = getLaneIndex(car);
	// Get the lane the car is trying to go through and enter
	EntryLane* lane = getLane(car, &sign->base);

	// Lock the lane
	lock(&sign->lanes[dir].lane_lock);

	// Car enters lane and is given token
	enterLane(car, lane);

	// Add Car to the queue
	sign->lanes[dir].car_queue[sign->lanes[dir].queue_index] = car;
	sign->lanes[dir].queue_index++;

	// Get the quadrants the car will go through
	int quad_count;
	quad_count = getStopSignRequiredQuadrants(car, sign->quad_indexes);

	// Check that quadrants are available. Block if not available.
	int index;
	while (1) {
		int i;
		for (i = 0; i < quad_count; i++){
			index = sign->quad_indexes[i];
			if (sign->quad[index].is_quad_locked == 1){
				wait(&sign->quad[index].quad_condition, &sign->quad[index].quad_lock);
				break;
			}
		}
		if (i == quad_count) {
			break;
		}
	}

	// Go through the stop sign
	goThroughStopSign(car, &sign->base);

	// Unlock the lane
	unlock(&sign->lanes[dir].lane_lock);

	// Wake up other threads waiting for the quadrants and unlock the quadrants
	for (int i = 0; i < quad_count; i++){
		index = sign->quad_indexes[i];
		sign->quad[index].is_quad_locked = 0;
		unlock(&sign->quad[index].quad_lock);
		pthread_cond_broadcast(&sign->quad[index].quad_condition);
	}

	// Exit simulation
	exitIntersection(car, lane);

	// /*** Add sync here for lane. ***/
	// TODO: Wait if the quadrants are not empty

	// Car enters lane and is given token
	// lock(&sign->intersection_lock);
	lock(&sign->lanes[dir].lane_lock);
	// unlock(&sign->intersection_lock);
	enterLane(car, lane);

	// lock(&sign->intersection_lock);
	for (int i = 0; i < quad_count; ++i)
	{	
		// printf("initiallock %d\n", i);
		index = sign->quad_indexes[i];
		sign->quad[index].is_quad_locked = 1;
		lock(&sign->quad[index].quad_lock);
		// printf("afterlock %d\n", i);
	}
	// unlock(&sign->intersection_lock);

	// Go through the stop sign
	lock(&sign->intersection_lock);
	goThroughStopSign(car, &sign->base);
	unlock(&sign->lanes[dir].lane_lock);
	unlock(&sign->intersection_lock);

	// logic to check if they exit in order.

	for (int i = 0; i < quad_count; ++i)
	{
		index = sign->quad_indexes[i];
		sign->quad[index].is_quad_locked = 0;
		unlock(&sign->quad[index].quad_lock);
		pthread_cond_broadcast(&sign->quad[index].quad_condition);
	}

	// Exit
	exitIntersection(car, lane);
}
