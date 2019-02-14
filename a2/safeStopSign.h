#pragma once
/**
* CSC369 Assignment 2
*
* This is the header file for your safe stop sign submission code.
*/
#include "car.h"
#include "stopSign.h"

/* This struct will be used to lock/unlock a quadrant. */
typedef struct _quadrant {
	// Set to 0 if the quadrant is not locked, 1 otherwise.
	int is_quad_locked;

	// Mutex lock for the quadrant.
	pthread_mutex_t quad_lock;

	// Condition variable for the quadrant.
	pthread_cond_t quad_condition;

} quadrant;

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

	// Condition variable for exiting the simulation.
	pthread_cond_t exit_condition;
} Lane;

/**
* @brief Structure that you can modify as part of your solution to implement
* proper synchronization for the stop sign intersection.
*
* This is basically a wrapper around StopSign, since you are not allowed to 
* modify or directly access members of StopSign.
*/
typedef struct _SafeStopSign {

	/**
	* @brief The underlying stop sign.
	*
	* You are not allowed to modify the underlying stop sign or directly
	* access its members. All interactions must be done through the functions
	* you have been provided.
	*/
	StopSign base;

	pthread_mutex_t intersection_lock;

	// TODO: Add any members you need for synchronization here.
	Lane lanes[DIRECTION_COUNT];

	// 4 quadrants
	quadrant quad[QUADRANT_COUNT];

} SafeStopSign;

/**
* @brief Initializes the safe stop sign.
*
* @param sign pointer to the instance of SafeStopSign to be initialized.
* @param carCount number of cars in the simulation.
*/
void initSafeStopSign(SafeStopSign* sign, int carCount);

/**
* @brief Destroys the safe stop sign.
*
* @param sign pointer to the instance of SafeStopSign to be freed
*/
void destroySafeStopSign(SafeStopSign* sign);

/**
* @brief Runs a car-thread in a stop-sign scenario.
*
* @param car pointer to the car.
* @param sign pointer to the stop sign intersection.
*/
void runStopSignCar(Car* car, SafeStopSign* sign);