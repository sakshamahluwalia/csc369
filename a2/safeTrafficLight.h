#pragma once
/**
* CSC369 Assignment 2
*
* This is the header file for your safe traffic light submission code.
*/
#include "car.h"
#include "trafficLight.h"

/* Struct containing locks for the lane and the queue of cars that have entered. */
typedef struct _lane{
	// Queue of Cars who have entered the lane.
	Car **car_queue;

	// Index of the Car whose turn it is to exit.
	int next_car_exit;

	// First index of queue which is empty.
	int queue_index;

	// Mutex lock for the lane.
	pthread_mutex_t lane_lock;

	// Mutex lock for exiting the simulation.
	pthread_mutex_t exit_lock;

	// Condition variable for entering the intersection.
	pthread_cond_t enter_condition;

	// Condition variable for exiting the simulation.
	pthread_cond_t exit_condition;
} Lane;

/**
* @brief Structure that you can modify as part of your solution to implement
* proper synchronization for the traffic light intersection.
*
* This is basically a wrapper around TrafficLight, since you are not allowed to 
* modify or directly access members of TrafficLight.
*/
typedef struct _SafeTrafficLight {

	/**
	* @brief The underlying light.
	*
	* You are not allowed to modify the underlying traffic light or directly
	* access its members. All interactions must be done through the functions
	* you have been provided.
	*/
	TrafficLight base;

	pthread_mutex_t intersection_lock;

	int num_cars_in;

	// TODO: Add any members you need for synchronization here.
	// TODO: Add any members you need for synchronization here.
	Lane lanes[TRAFFIC_LIGHT_LANE_COUNT];

} SafeTrafficLight;

/**
* @brief Initializes the safe traffic light.
*
* @param light pointer to the instance of SafeTrafficLight to be initialized.
* @param eastWest total number of cars moving east-west.
* @param northSouth total number of cars moving north-south.
*/
void initSafeTrafficLight(SafeTrafficLight* light, int eastWest, int northSouth);

/**
* @brief Destroys the safe traffic light.
*
* @param light pointer to the instance of SafeTrafficLight to be destroyed.
*/
void destroySafeTrafficLight(SafeTrafficLight* light);

/**
* @brief Runs a car-thread in a traffic light scenario.
*
* @param car pointer to the car.
* @param light pointer to the traffic light intersection.
*/
void runTrafficLightCar(Car* car, SafeTrafficLight* light);
