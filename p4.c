// ******************************************************************************************************************
//  Multi-threading program with shared memory (mutex).  Implemented for Linux.
//  Copyright(C) 2018  James LoForti
//  Contact Info: jamesloforti@gmail.com
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.If not, see<https://www.gnu.org/licenses/>.
//									     ____.           .____             _____  _______   
//									    |    |           |    |    ____   /  |  | \   _  \  
//									    |    |   ______  |    |   /  _ \ /   |  |_/  /_\  \ 
//									/\__|    |  /_____/  |    |__(  <_> )    ^   /\  \_/   \
//									\________|           |_______ \____/\____   |  \_____  /
//									                             \/          |__|        \/ 
//
// ******************************************************************************************************************
//

//Project Purpose: 
//To implement 3 threads (main, producer, consumer) that share 2 different blocks of memory.

#include <stdlib.h> // needed for exit()
#include <stdio.h> // needed for printf()
#include <string.h> // needed for memset()
#include <pthread.h> // needed for pthread_t
#include <math.h> // needed for floor() & sqrt()

#define BUFFER_SIZE 10
#define MAX_FACTORS 10
#define SENTINEL (-1)

//Struct to hold a number & its prime factors
typedef struct data
{
	int num;
	int primes[MAX_FACTORS];
}Data;

//Struct to hold buffer & associated data
typedef struct buffer
{
	struct data buff[BUFFER_SIZE];
	pthread_mutex_t mutex;
	pthread_cond_t waitingRoom1;
	pthread_cond_t waitingRoom2;
	int in;
	int out;
}Buffer;

//Purpose: To find the prime factors of every given number
//Numbers are removed from mp_buffer and added to the cp_buffer
void* producer(void* empty);

//Purpose: To remove the data structs from cp_buffer and print nums & their primes to console
void* consumer(void* empty);

//Purpose: To safely add values to the given buffer by locking critical segments
void buffer_add(Buffer* buffer, Data* data);

//Purpose: To safely remove values from the given buffer by locking critical segments
Data buffer_remove(Buffer* buffer);

//Purpose: Helper function for prime factoring
void* factor(void* container);

//Purpose: To test for primality
int prime_tester(int num);

//Purpose: To flag all non-prime numbers with a 1
void prime_builder(char* flags, int primeThreshold);

//Purpose: To find the first prime factor, then the following component factor
//If that component is prime, finish. If that component is not prime, recursion
void trial_divider(int num, char* flags, int primes[], int count);

//Declare globals
Buffer mp_buffer;
Buffer cp_buffer;

int main(int argc, char* argv[])
{
	//Initialize each buffer's mutex
	pthread_mutex_init(&mp_buffer.mutex, NULL);
	pthread_mutex_init(&cp_buffer.mutex, NULL);

	//Declare producer thread and consumer thread
	pthread_t prodThread;
	pthread_t consThread;
	
	//Create the producer thread and consumer thread
	pthread_create(&prodThread, NULL, producer, NULL);
	pthread_create(&consThread, NULL, consumer, NULL);

	//For every given number
	int i;
	for (i = 0; i < argc - 1; i++)
	{
		//Create struct to hold given number
		Data temp;
		memset(temp.primes, 0, sizeof(temp.primes)); // zero out primes array
		temp.num = atoi(argv[i + 1]); // save given number as num

    	//Add data to mp_buffer
    	buffer_add(&mp_buffer, &temp);
	} // end for

	//Create SENTINEL to terminate loops and add it to mp_buffer
	Data sentinel;
	sentinel.num = SENTINEL;
	memset(sentinel.primes, 0, sizeof(sentinel.primes));
	buffer_add(&mp_buffer, &sentinel);

	//Have parent wait for producer thread to finish
  	if (pthread_join(prodThread, NULL) != 0) 
  	{
  		//And if it failed...
    	perror("An error occurred during producer thread joining... ");
    	exit(1);
  	}

	//Have parent wait for consumer thread to finish
  	if (pthread_join(consThread, NULL) != 0) 
  	{
  		//And if it failed...
    	perror("An error occurred during consumer thread joining... ");
    	exit(1);
  	}

	return 0;
} // end main()

void* producer(void* empty) 
{
	//Infinite loop
	while (1) 
	{
		//Remove value from mp_buffer
		Data temp = buffer_remove(&mp_buffer);

		//If no more numbers exist
		if (temp.num == SENTINEL)
		{
			//Add SENTINEL to cp_buffer
			buffer_add(&cp_buffer, &temp);
			return NULL; // exit loop
		} // end if
		else // numbers remain to be factored
		{
			//Find factors
			temp = *((Data*)factor(&temp));
			
			//Add data to cp_buffer
			buffer_add(&cp_buffer, &temp);
		} // end else
	} // end while
	
	return NULL;
} // end function producer()

void* consumer(void* empty) 
{
    //Print opening seperator, name, and project
    printf("\n*********************************************** ");
    printf("\nName: James LoForti \nProject: Project 4\n\n");

    //Print header
    printf("Prime Factors:\n");

    //Infinite loop
	while (1) 
	{
		//Remove data from cp_buffer
		Data temp = buffer_remove(&cp_buffer);

		//If no more numbers exist
		if (temp.num == SENTINEL)
		{
			//Exit loop
			return NULL;
		}
		else // numbers remain to be printed
		{
	    	//Print original number
	    	printf("\t%d: ", temp.num);

	    	//For this given number
	    	int j;
	    	for (j = 0; j < MAX_FACTORS; j++)
	    	{
	    		//If prime value is NOT null
	    		if (temp.primes[j] > 0)
	    		{
		    		//Print prime factor
	    			printf("%d ", (temp.primes[j]));
	    		}
	    	} // end for

	    	printf("\n");
		} // end else
	} // end while

    //Print closing seperator
    printf("\n*********************************************** \n");

    return NULL;
} // end function consumer()

void buffer_add(Buffer* buffer, Data* data)
{
	//Lock resources
	pthread_mutex_lock(&buffer->mutex);

	//While the buffer is full, wait
	while ((buffer->in + 1) % BUFFER_SIZE == buffer->out)
	{
		pthread_cond_wait(&buffer->waitingRoom1, &buffer->mutex);
	} 
		
	//Find its prime factors and save them in the consumer-producer buffer
	buffer->buff[buffer->in] = *data;

	//Move in to next position in circular buffer
	buffer->in = (buffer->in + 1) % BUFFER_SIZE;

	//Signal the consumer and unlock resources
	pthread_cond_signal(&buffer->waitingRoom2);
	pthread_mutex_unlock(&buffer->mutex);
} // end function buffer_add()

Data buffer_remove(Buffer* buffer)
{
	//Lock resources
	pthread_mutex_lock(&buffer->mutex);

	//While nothing is in the buffer, wait
	while (buffer->in == buffer->out) 
	{
		pthread_cond_wait(&buffer->waitingRoom2, &buffer->mutex);
	}

	//Read next number and its factors from buffer and save
	Data temp = buffer->buff[buffer->out];

	//Move out to next position in circular buffer
	buffer->out = (buffer->out + 1) % BUFFER_SIZE;

	//Unlock resources and signal producer
	pthread_cond_signal(&buffer->waitingRoom1);
	pthread_mutex_unlock(&buffer->mutex);

	return temp;
} // end function buffer_remove()

void* factor(void* container)
{
	//Cast the given struct from void* to Data*
	Data* data = (Data*)container;
	
	//Save num and init count to track next element in prime array
	int num = data->num;
	int count = 0;

	//If num is prime (1)
	if (prime_tester(num))
	{
		//Add itself to the array
		data->primes[count] = num;
		return data;
	}
	else // num is NOT prime
	{
		//Use the squareroot as the highest possible prime factor 
		//Any values between the sqrt(n)-num will already have been tested
		int primeThreshold = floor(sqrt(num));

		//Allocate memory to track prime and non-prime numbers via flags
		char* flags = calloc(primeThreshold, 1);

		//Flag all non-prime numbers
		prime_builder(flags, primeThreshold);

		//Find prime factors via trial division
		trial_divider(num, flags, data->primes, count);

		//Deallocate 
		free(flags);
	} // end else

	return data;
} // end function factor()

int prime_tester(int num)
{
	//If num is evenly divisibe by 2 (NOT prime)
	if (num % 2 == 0)
	{
		//Needed to recognize 2 as prime
		if (num == 2)
			return 1;
		else
			return 0;
	} // end if

	//At this point we know that num is not divisible by 2
	//Therefore, it is not divisible by any multiple of 2

	//Use the squareroot as the highest possible prime factor 
	int primeThreshold = floor(sqrt(num));
	
	//For every number between 1 and the sqrt of num
	int i;
	for (i = 1; i < primeThreshold; i++)
	{
		//Save the next odd value
		int temp = (2 * i) + 1;

		//If the current odd number is a factor of num
		if (num % temp == 0)
		{
			//Num is NOT prime
			return 0;
		}
	} // end for

	//Num is prime, return 1
	return 1;
} // end function prime_tester()

void prime_builder(char* flags, int primeThreshold)
{
	//For all possible primes
	int i;
	for (i = 2; i <= primeThreshold; i++)
	{
		//If this values multiples have not been marked as non-prime yet (0)
		if (!flags[i])
		{
			//Iterate through all multiples of 2-sqrt(num)
			int j;
			for (j = 2 * i; j < primeThreshold; j += i)
			{
				//Flag as NOT prime (1)
				flags[j] = 1;
			} // end for
		} // end if
	} // end for
} // end function prime_builder()

void trial_divider(int num, char* flags, int primes[], int count)
{
	//Use the squareroot as the highest possible prime factor 
	int primeThreshold = floor(sqrt(num));

	//For all numbers from 2-sqrt(num)
	int i;
	for (i = 2; i <= primeThreshold; i++)
	{
		//If the current value is not prime (1)
		if (flags[i])
		{
			//Skip it
			continue;
		}
		else // current value is prime
		{
			//Current value is prime, so save it
			int prime = i;

			//If the last prime has been found (prime^2)
			if ((pow(prime, 2)) == num)
			{
				//Add it to the list
				primes[count++] = prime;
				primes[count++] = prime;
				break;
			} // end if

			//If the current prime produces a component prime
			if (num % prime == 0)
			{
				//Add it to the list
				primes[count++] = prime;

				//Find the component factor
				int component = num / prime;

				//If the component is prime
				if (prime_tester(component))
				{
					//Add it to the list
					primes[count++] = component;
				}
				else // component is not prime
				{
					//Factor the component
					trial_divider(component, flags, primes, count);
				}

				break;
			} // end if
		} // end else
	} // end for
} // end function trial_divider()