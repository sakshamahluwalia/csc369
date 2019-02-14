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
	if (car_index == 0){
		sign->lanes[dir].next_car_exit = car_index;
	}
	sign->lanes[dir].queue_index++;
	printf("Car %d entering lane %d\n", car_index, dir);

	// Get the quadrants the car will go through
	int quad_indexes[4];
	int quad_count = getStopSignRequiredQuadrants(car, quad_indexes);

	// Check that quadrants are available. Block if not available.
	while (1) {
		int i = 0;
		// printf("------------------------------\n");
		// printf("Current state of quadrants:\n");
		// printf("[1:%d] [0:%d]\n[2:%d] [3:%d]\n", sign->quad[1].is_quad_locked, sign->quad[0].is_quad_locked,
		// 	sign->quad[2].is_quad_locked, sign->quad[3].is_quad_locked);
		// printf("------------------------------\n");
		for (i = 0; i < quad_count; i++){
			lock(&sign->intersection_lock);
			if (sign->quad[quad_indexes[i]].is_quad_locked == 1){
				unlock(&sign->intersection_lock);
				printf("Car %d blocked at lane %d with action %d\n", car_index, dir, car->action);
				pthread_cond_wait(&(sign->quad[quad_indexes[i]].quad_condition), &(sign->quad[quad_indexes[i]].quad_lock));
				break;
			}
			unlock(&sign->intersection_lock);
		}
		if (i == quad_count){
			break;
		}
	}

	lock(&sign->intersection_lock);
	for (int i = 0; i < quad_count; i++){
		sign->quad[quad_indexes[i]].is_quad_locked = 1;
		lock(&sign->quad[quad_indexes[i]].quad_lock);
	}
	// unlock(&sign->intersection_lock);

	// printf("Car %d (lane %d) going through intersection with action %d\n", car_index, dir, car->action);
	// printf("------------------------------\n");
	// 	printf("Current state of quadrants:\n");
	// 	printf("[1:%d] [0:%d]\n[2:%d] [3:%d]\n", sign->quad[1].is_quad_locked, sign->quad[0].is_quad_locked,
	// 		sign->quad[2].is_quad_locked, sign->quad[3].is_quad_locked);
	// 	printf("------------------------------\n");

	// unlock(&sign->intersection_lock);

	// Go through the stop sign
	goThroughStopSign(car, &sign->base);

	// Unlock the lane so other cars can enter the lane
	unlock(&sign->lanes[dir].lane_lock);

	// lock(&sign->intersection_lock);
	// Wake up other threads waiting for the quadrants and unlock the quadrants
	for (int i = 0; i < quad_count; i++){
		unlock(&sign->quad[quad_indexes[i]].quad_lock);
		sign->quad[quad_indexes[i]].is_quad_locked = 0;
	}
	unlock(&sign->intersection_lock);

	for (int i = 0; i < quad_count; i++){
		pthread_cond_broadcast(&sign->quad[quad_indexes[i]].quad_condition);
	}

	// Exit simulation
	lock(&sign->lanes[dir].exit_lock);

	while (sign->lanes[dir].next_car_exit != car_index){
		pthread_cond_wait(&sign->lanes[dir].exit_condition, &sign->lanes[dir].exit_lock);
	}

	printf("Car %d exiting lane %d\n", car_index, dir);
	exitIntersection(car, lane);
	sign->lanes[dir].next_car_exit = car_index + 1;

	unlock(&sign->lanes[dir].exit_lock);
	pthread_cond_broadcast(&sign->lanes[dir].exit_condition);

	// /*** Add sync here for lane. ***/

 // 	// TODO: Wait if previous car has not exited the entry lane
 // 	while (sign->lane[dir].is_lane_locked == 1) {
 // 		wait(&sign->lane[dir].lane_condition, &sign->lane[dir].lane_lock);
 // 	}

	// TODO: Wait if the quadrants are not empty
	// int index;
	// for (int i = 0; i < quad_count; ++i)
	// {
	// 	index = sign->quad_indexes[i];
	//  	while (sign->quad[index].is_quad_locked == 1) {
	//  		wait(&sign->quad[index].quad_condition, &sign->quad[index].quad_lock);
	//  	}
	// }

	// get the lock
	// lock(&sign->intersection_lock);
	// lock(&sign->lane[dir].lane_lock);
	// sign->lane[dir].is_lane_locked = 1;
	// unlock(&sign->intersection_lock);


	// for (int i = 0; i < quad_count; ++i)
	// {
	// 	index = sign->quad_indexes[i];
	// 	lock(&sign->intersection_lock);
	// 	sign->quad[index].is_quad_locked = 1;
	// 	unlock(&sign->intersection_lock);
	// 	lock(&sign->quad[index].quad_lock);
	// }

	// Unlock
	// for (int i = 0; i < quad_count; ++i)
	// {
	// 	index = sign->quad_indexes[i];
	// 	lock(&sign->intersection_lock);
	// 	sign->quad[index].is_quad_locked = 0;
	// 	unlock(&sign->intersection_lock);
	// 	unlock(&sign->quad[index].quad_lock);
	// 	pthread_cond_broadcast(&sign->quad[index].quad_condition);
	// }

	// lock(&sign->intersection_lock);
	// sign->lane[dir].is_lane_locked = 0;
	// unlock(&sign->lane[dir].lane_lock);
	// unlock(&sign->intersection_lock);

	// Car exited so signal next car waiting on lane
	// pthread_cond_broadcast(&sign->lane[dir].lane_condition);
}
