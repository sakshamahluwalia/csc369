/**
* CSC369 Assignment 2
*
* This is the source/implementation file for your safe traffic light 
* submission code.
*/
#include "safeTrafficLight.h"

int safeToEnter(int car_position, LightState light_state){
	if ((car_position == EAST || car_position == WEST) && light_state == EAST_WEST){
		return 1;
	} else if ((car_position == NORTH || car_position == SOUTH) && light_state == NORTH_SOUTH){
		return 1;
	}
	return 0;
}

void initSafeTrafficLight(SafeTrafficLight* light, int horizontal, int vertical) {
	initTrafficLight(&light->base, horizontal, vertical);

	initMutex(&light->stoplight);

	// TODO: Add any initialization logic you need.
	for (int i = 0; i < TRAFFIC_LIGHT_LANE_COUNT; i++) {
		light->lanes[i].queue_index = 0;
		light->lanes[i].next_car_exit = 0;
		initMutex(&light->lanes[i].lane_lock);
		initMutex(&light->lanes[i].exit_lock);
		if ((0 <= i && i <= 2) || (6 <= i && i <= 8))	{
			light->lanes[i].car_queue = malloc(sizeof(Car *) * horizontal);
		} else {
			light->lanes[i].car_queue = malloc(sizeof(Car *) * vertical);
		}
		initConditionVariable(&light->lanes[i].left_condition);
		initConditionVariable(&light->lanes[i].go_condition);
		initConditionVariable(&light->lanes[i].exit_condition);
	}

	for (int i = 0; i < 2; ++i)
	{
		initConditionVariable(&light->go_condition[i]);
	}

}

void destroySafeTrafficLight(SafeTrafficLight* light) {
	destroyTrafficLight(&light->base);

	destroyMutex(&light->stoplight);

	// TODO: Add any logic you need to free data structures
	for (int i = 0; i < TRAFFIC_LIGHT_LANE_COUNT; i++)
	{
		destroyMutex(&light->lanes[i].lane_lock);
		destroyMutex(&light->lanes[i].exit_lock);
		destroyConditionVariable(&light->lanes[i].left_condition);
		destroyConditionVariable(&light->lanes[i].go_condition);
		destroyConditionVariable(&light->lanes[i].exit_condition);
		free(light->lanes[i].car_queue);
	}
	for (int i = 0; i < 2; ++i)
	{
		destroyConditionVariable(&light->go_condition[i]);
	}
}

void runTrafficLightCar(Car* car, SafeTrafficLight* light) {
	// Get the lane index in TrafficLight.
	int lane_index = getLaneIndexLight(car);

	// Get lane.
	EntryLane* lane = getLaneLight(car, &light->base);

	// Lock the lane and enter lane.
	lock(&light->lanes[lane_index].lane_lock);

	// Enter the lane
	enterLane(car, lane);

	// Add car to the Lane's queue.
	int car_index = light->lanes[lane_index].queue_index;
	light->lanes[lane_index].car_queue[car_index] = car;
	if (car_index == 0){
		lock(&light->lanes[lane_index].exit_lock);
		light->lanes[lane_index].next_car_exit = car_index;
		unlock(&light->lanes[lane_index].exit_lock);
	}
	light->lanes[lane_index].queue_index++;

	// Unlock to let other cars enter the lane.
	unlock(&light->lanes[lane_index].lane_lock);

	printf("Car %d[%d] entered lane\n", car_index, lane_index);

	// Check to see if it's safe to enter the intersection.

	lock(&light->stoplight);
	LightState light_state = getLightState(&light->base);
	printf("Light: %d\n", light_state);







	while (!safeToEnter(car->position, getLightState(&light->base))) {
		printf("Car %d[%d] blocked making action %d\n", car_index, lane_index, car->action);
		pthread_cond_wait(&light->go_condition[light_state], &light->stoplight);
		// Get the state of the light.
		// light_state = getLightState(&light->base);
		// go = safeToEnter(car->position, light_state);
	}
	// unlock(&light->lanes[lane_index].lane_lock);

	// Enter the traffic light.
	enterTrafficLight(car, &light->base);

	pthread_cond_broadcast(&light->go_condition[light_state]);

	printf("Car %d[%d] has entered the traffic\n", car_index, lane_index);

	lock(&light->lanes[lane_index].lane_lock);

	// Check to see if it's safe to act.
	if (car->action == 2){
		while (getStraightCount(&light->base, getOppositePosition(car->position)) > 0) {
			pthread_cond_wait(&light->lanes[lane_index].left_condition, &light->lanes[lane_index].lane_lock);
		}
	}

	// Unlock and wake up other threads to move through the traffic.

	// Move through the traffic light
	printf("Car %d[%d] moving making action %d\n", car_index, lane_index, car->action);
	actTrafficLight(car, &light->base, NULL, NULL, NULL);

	pthread_cond_broadcast(&light->lanes[lane_index].left_condition);
	unlock(&light->lanes[lane_index].lane_lock);

	// Signal the other cars to go.
	pthread_cond_broadcast(&light->go_condition[1-light_state]);
	unlock(&light->stoplight);







	lock(&light->lanes[lane_index].exit_lock);
	while (light->lanes[lane_index].next_car_exit != car_index) {
		pthread_cond_wait(&light->lanes[lane_index].exit_condition, &light->lanes[lane_index].exit_lock);
	}
	exitIntersection(car, lane);
	printf("Car %d exited lane %d\n", car_index, lane_index);
	light->lanes[lane_index].next_car_exit++;
	pthread_cond_broadcast(&light->lanes[lane_index].exit_condition);
	unlock(&light->lanes[lane_index].exit_lock);
}
