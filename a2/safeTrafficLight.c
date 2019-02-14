/**
* CSC369 Assignment 2
*
* This is the source/implementation file for your safe traffic light 
* submission code.
*/
#include "safeTrafficLight.h"

void initSafeTrafficLight(SafeTrafficLight* light, int horizontal, int vertical) {
	initTrafficLight(&light->base, horizontal, vertical);

	// TODO: Add any initialization logic you need.
	for (int i = 0; i < TRAFFIC_LIGHT_LANE_COUNT; ++i)
	{
		light->lanes[i].queue_index = 0;
		light->num_cars_in = 0;
		initMutex(&light->lanes[i].lane_lock);
		light->lanes[i].car_queue = malloc(sizeof(Car *) * count);
		// initMutex(&light->lanes[i].exit_lock);
		initConditionVariable(&light->lanes[i].enter_condition);
	}

}

void destroySafeTrafficLight(SafeTrafficLight* light) {
	destroyTrafficLight(&light->base);

	// TODO: Add any logic you need to free data structures
	for (int i = 0; i < TRAFFIC_LIGHT_LANE_COUNT; ++i)
	{
		destroyMutex(&light->lanes[i].lane_lock);
		// destroyMutex(&light->lanes[i].exit_lock);
		destroyConditionVariable(&light->lanes[i].enter_condition);
		free(light->lanes[i].car_queue);
	}

}

void runTrafficLightCar(Car* car, SafeTrafficLight* light) {

	// Add the car to the proper lane queue.
	int lane_index = getLaneIndexLight(car);

	EntryLane* lane = getLaneLight(car, &light->base);

	lock(&light->lanes[lane_index].lane_lock);

	enterLane(car, lane);
	
	// keep track of number of cars in a queue.
	int car_index = light->lanes[lane_index].queue_index;
	light->lanes[lane_index].car_queue[car_index] = car;
	if (car_index == 0){
		light->lanes[lane_index].next_car_exit = car_index;
	}
	light->lanes[lane_index].queue_index++;

	// Unlock to add multiple cars to the queue.
	unlock(&light->lanes[lane_index].lane_lock);
	
	// check the signal.

	// lock the intersection to get the right light_mode.
	lock(&light->intersection_lock);

	LightState light_state = getLightState(light);

	// wait if the car is going in a different direction or if light == red.
	while(1) {
		if ((car->position == EAST || car->position == WEST) &&
					light_state != EAST_WEST)
		{	
			unlock(&light->intersection_lock);
			pthread_cond_wait(&(light->lanes[lane_index].enter_condition), &(light->lanes[lane_index].lane_lock));
		}
		else if ((car->position == NORTH || car->position == SOUTH) &&
					light_state != NORTH_SOUTH)
		{
			unlock(&light->intersection_lock);
			pthread_cond_wait(&(light->lanes[lane_index].enter_condition), &(light->lanes[lane_index].lane_lock));
		}
		else 
		{
			unlock(&light->intersection_lock);
			pthread_cond_wait(&(light->lanes[lane_index].enter_condition), &(light->lanes[lane_index].lane_lock));
		}

	}

	// If here then the car is in the lane and has the right direction.

	// TODO: keep track of the number of cars that will enter the intersection.
	enterTrafficLight(car, &light->base);

	// wait on car if its making a left.
	int oncoming = getStraightCount(light, car->position) + getStraightCount(light, getOppositePosition(car->position));

	while (1) {
		if (car->action == 2 && oncoming > 0)
		{
			unlock(&light->intersection_lock);

			// TODO: what should we wait on.
			pthread_cond_wait(&(light->lanes[lane_index].enter_condition), &(light->lanes[lane_index].lane_lock));
		}
	}

	// keep track of the number of cars leaving the intersection.
	actTrafficLight(car, &light->base, NULL, NULL, NULL);

	unlock(&light->intersection_lock);

	// Enter and act are separate calls because a car turning left can first
	// enter the intersection before it needs to check for oncoming traffic.

	lock(&light->lanes[lane_index].exit_lock);

	exitIntersection(car, lane);
	light->lanes[lane_index].next_car_exit = car_index + 1;

	unlock(&light->lanes[lane_index].exit_lock);

}
