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
		initMutex(&sign->lanes[i].exit_lock);
		initConditionVariable(&sign->lanes[i].exit_condition);
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
		destroyMutex(&sign->lanes[i].exit_lock);
		destroyConditionVariable(&sign->quad[i].quad_condition);
		destroyConditionVariable(&sign->lanes[i].exit_condition);
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
	int car_index = sign->lanes[dir].queue_index;
	sign->lanes[dir].car_queue[car_index] = car;
	if (car_index == 0) {
		sign->lanes[dir].next_car_exit = car;
	}
	sign->lanes[dir].queue_index++;
	printf("Car %d entering lane %d\n", car_index, dir);

	// Get the quadrants the car will go through
	int quad_count = getStopSignRequiredQuadrants(car, sign->quad_indexes);

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
		// if (i == quad_count) {
		// 	printf("Car %d blocked at lane %d\n", car_index, dir);
		// 	pthread_cond_wait(&(sign->quad[index].quad_condition), &(sign->quad[index].quad_lock));
		// 	break;
		// }
		if (i == quad_count){
			printf("Car %d going through intersection %d\n", car_index, sign->quad_indexes[0]);
			break;
		}
	}

	for (int i = 0; i < quad_count; i++){
		lock(&sign->quad[sign->quad_indexes[i]].quad_lock);
		sign->quad[sign->quad_indexes[i]].is_quad_locked = 1;
	}

	lock(&sign->intersection_lock);
	// Go through the stop sign
	goThroughStopSign(car, &sign->base);

	unlock(&sign->intersection_lock);

	// Unlock the lane so other cars can enter the lane
	unlock(&sign->lanes[dir].lane_lock);

	// Wake up other threads waiting for the quadrants and unlock the quadrants
	for (int i = 0; i < quad_count; i++){
		index = sign->quad_indexes[i];
		sign->quad[index].is_quad_locked = 0;
		unlock(&sign->quad[index].quad_lock);
		pthread_cond_broadcast(&sign->quad[index].quad_condition);
	}

	// Exit simulation
	lock(&sign->lanes[dir].exit_lock);

	while (sign->lanes[dir].next_car_exit != sign->lanes[dir].car_queue[car_index]){
		pthread_cond_wait(&sign->lanes[dir].exit_condition, &sign->lanes[dir].exit_lock);
	}

	printf("Car %d exiting lane %d\n", car_index, dir);
	exitIntersection(car, lane);
	sign->lanes[dir].next_car_exit = sign->lanes[dir].car_queue[car_index++];

	unlock(&sign->lanes[dir].exit_lock);
	pthread_cond_broadcast(&sign->lanes[dir].exit_condition);
}
