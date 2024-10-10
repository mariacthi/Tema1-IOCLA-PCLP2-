#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "structs.h"

#define DIE(assertion, call_description)				\
	do {								\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(errno);				        \
		}							\
	} while (0)

#define MAX_STRING 100

void get_operations(void **operations);

void free_sensor(sensor **s, const int n)
{
	for (int i = 0; i < n; i++) {
		/* freeing each sensor's parts that were
		dynamically allocated */

		free((*s)[i].sensor_data);
		free((*s)[i].operations_idxs);
	}

	free(*s);
}

sensor create_pmu_sensor(sensor old)
{
	/* allocating memory for a new PMU sensor and copying
	the old sensor's data in the new one */

	sensor new;
	new.sensor_type = PMU;

	new.sensor_data = malloc(sizeof(power_management_unit));
	DIE(!new.sensor_data, "Malloc failed");

	memcpy(new.sensor_data, old.sensor_data, sizeof(power_management_unit));

	new.nr_operations = old.nr_operations;

	new.operations_idxs = malloc(old.nr_operations * sizeof(int));
	DIE(!new.operations_idxs, "Malloc failed");

	for(int j = 0; j < old.nr_operations; j++)
		new.operations_idxs[j] = old.operations_idxs[j];

	return new;
}

sensor create_tire_sensor(sensor old)
{
	/* allocating memory for a new tire sensor and copying
	the old sensor's data in the new one */

	sensor new;
	new.sensor_type = TIRE;

	new.sensor_data = malloc(sizeof(tire_sensor));
	DIE(!new.sensor_data, "Malloc failed");

	memcpy(new.sensor_data, old.sensor_data, sizeof(tire_sensor));

	new.nr_operations = old.nr_operations;

	new.operations_idxs = malloc(old.nr_operations * sizeof(int));
	DIE(!new.operations_idxs, "Malloc failed");

	for(int j = 0; j < old.nr_operations; j++)
		new.operations_idxs[j] = old.operations_idxs[j];

	return new;
}

sensor *sort_sensor(sensor *s, const int n)
{
	/* sorting by priority the array of sensors
	and replacing it with a new one, after freeing
	the memory allocated */
	sensor *new_s = malloc(n * sizeof(*new_s));
	DIE(!new_s, "Malloc failed");

	int new_n = 0;

	for (int i = 0; i < n; i++) {
		/* in the newly sorted array of sensors, the PMU
		sensors have priority, so I go first through those
		and create them in the new one */
		if (s[i].sensor_type == PMU) {
			new_s[new_n] = create_pmu_sensor(s[i]);
			new_n++;
		}
	}

	for (int i = 0; i < n; i++) {
		/* after the PMU sensorrs have been placed in the
		new array, I go through the tire sensors and create
		them */
		if (s[i].sensor_type == TIRE) {
			new_s[new_n] = create_tire_sensor(s[i]);
			new_n++;
		}
	}

	free_sensor(&s, n);
	return new_s;
}

sensor *read_sensors_from_file(FILE *fin, const int n)
{
	/* reading each sensor from the binary file and
	dynamically allocating memory where it is needed */

	sensor *sensor = malloc(n * sizeof(*sensor));
	DIE(!sensor, "Malloc failed");

	for (int i = 0; i < n; i++) {
		fread(&sensor[i].sensor_type, sizeof(enum sensor_type), 1, fin);

		/* based on the type of sensor, the sensor_data will
		have two types that should be allocated separately */
		if(sensor[i].sensor_type == TIRE) {
			tire_sensor *tire_data = malloc(sizeof(*tire_data));
			DIE(!tire_data, "Malloc failed");

			fread(tire_data, sizeof(*tire_data), 1, fin);
			sensor[i].sensor_data = tire_data;
		} else {
			power_management_unit *pmu_data = malloc(sizeof(*pmu_data));
			DIE(!pmu_data, "Malloc failed");

			fread(pmu_data, sizeof(*pmu_data), 1, fin);
			sensor[i].sensor_data = pmu_data;
		}

		fread(&sensor[i].nr_operations, sizeof(int), 1, fin);

		sensor[i].operations_idxs = malloc(sensor[i].nr_operations * sizeof(int));
		DIE(!sensor[i].operations_idxs, "Malloc failed");

		for (int j = 0; j < sensor[i].nr_operations; j++)
			fread(&sensor[i].operations_idxs[j], sizeof(int), 1, fin);
	}

	/* after reading all of the sensors, they are
	sorted by priority */
	sensor = sort_sensor(sensor, n);

	return sensor;
}

void print_sensor(sensor s, int analysed) {
	/* analysed = 1 only if the sensor is a tire
	sensor and through the operations that were made
	on it was the one that calculated the performace
	score as well (operations[3]) */
	if (s.sensor_type == TIRE) {
		tire_sensor *tire = (tire_sensor *)s.sensor_data;
		printf("Tire Sensor\n");

		printf("Pressure: %.2f\n", tire->pressure);

		printf("Temperature: %.2f\n", tire->temperature);

		printf("Wear Level: %d%%\n", tire->wear_level);

		if (analysed)
			printf("Performance Score: %d\n", tire->performace_score);
		else
			printf("Performance Score: Not Calculated\n");
	} else {
		power_management_unit *pmu = (power_management_unit *)s.sensor_data;
		printf("Power Management Unit\n");

		printf("Voltage: %.2f\n", pmu->voltage);

		printf("Current: %.2f\n", pmu->current);

		printf("Power Consumption: %.2f\n", pmu->power_consumption);

		printf("Energy Regen: %d%%\n", pmu->energy_regen);

		printf("Energy Storage: %d%%\n", pmu->energy_storage);
	}
}

void analyze_sensor(sensor s, int *v_analysed)
{
	/* the analyse function calls the get_operations
	function to create an array of the 8 functions
	that are needed */
	void (*operations[8])(void *);
	get_operations((void **)operations);

	for (int i = 0; i < s.nr_operations; i++) {
		operations[s.operations_idxs[i]](s.sensor_data);
		if (s.operations_idxs[i] == 3) {
			/* if the operation is for a tire sensor
			that calculates the performance score */
			*v_analysed = 1;
		}
	}

}

int verify_tire(tire_sensor *tire)
{
	/* verifies if the tire sensor values are correct */

	if (tire->pressure < 19 || tire->pressure > 28)
		return 0;

	if (tire->temperature < 0 || tire->temperature > 120)
		return 0;

	if(tire->wear_level < 0 || tire->wear_level > 100)
		return 0;

	return 1;
}

int verify_pmu(power_management_unit *pmu)
{
	/* verifies if the PMU sensor values are correct */

	if (pmu->voltage < 10 || pmu->voltage > 20)
		return 0;

	if (pmu->current < -100 || pmu->current > 100)
		return 0;

	if (pmu->power_consumption < 0 || pmu->power_consumption > 1000)
		return 0;

	if (pmu->energy_regen < 0 || pmu->energy_regen > 100)
		return 0;

	if (pmu->energy_storage < 0 || pmu->energy_storage > 100)
		return 0;

	return 1;
}

sensor *clear_sensor(sensor *s, int *n, int **v_analysed)
{
	/* the clear function creates a new array of sensors that
	will be reallocked every time a new sensor is valid
	and also creates a new array for the analysed tire sensors
	(copies the values from the old one when a sensor is valid)*/

	sensor *new_s = malloc(sizeof(sensor));
	DIE(!new_s, "Malloc failed");

	int *new_analysed = malloc(sizeof(int));
	DIE(!new_analysed, "Malloc failed");

	int new_n = 0;

	for (int i = 0; i < *n; i++) {
		if (s[i].sensor_type == TIRE) {
			tire_sensor *tire = (tire_sensor *)s[i].sensor_data;

			if (verify_tire(tire) == 1) {
				memcpy(&new_s[new_n], &s[i], sizeof(s[i]));

				new_analysed[new_n] = (*v_analysed)[i];

				new_n++;

				new_s = realloc(new_s, (new_n + 1) * sizeof (sensor));
				DIE(!new_s, "Realloc failed");

				new_analysed = realloc(new_analysed, (new_n + 1) * sizeof(int));
				DIE(!new_analysed, "Realloc failed");
			}
		} else {
			power_management_unit *pmu = (power_management_unit *)s[i].sensor_data;

			if(verify_pmu(pmu) == 1) {
				memcpy(&new_s[new_n], &s[i], sizeof(s[i]));

				new_analysed[new_n] = (*v_analysed)[i];

				new_n++;

				new_s = realloc(new_s, (new_n + 1) * sizeof (sensor));
				DIE(!new_s, "Realloc failed");

				new_analysed = realloc(new_analysed, (new_n + 1) * sizeof(int));
				DIE(!new_analysed, "Realloc failed");
			}
		}
	}

	free_sensor(&s, *n);

	*n = new_n;

	free(*v_analysed);
	*v_analysed = new_analysed;

	if (new_n != 0)
		return new_s;

	return NULL;
}

int main(int argc, char const *argv[])
{
	FILE *fin = fopen(argv[1], "rb");
	DIE(!fin, "Can't open file");

	/* first reading from the opened file the number of sensors */
	int number_of_sensors;
	fread(&number_of_sensors, sizeof(int), 1, fin);

	sensor *sensor = read_sensors_from_file(fin, number_of_sensors);

	char *command = malloc(MAX_STRING * sizeof(*command));
	DIE(!command, "Malloc failed");

	int index;
	fclose(fin);

	/* this array has a role when printing a sensor because
	it will only change its value from 0 to 1 for those
	tire sensors for which the performance score has been
	calculated (through analyse) */
	int *v_analysed = calloc(number_of_sensors, sizeof(int));
	DIE(!v_analysed, "Malloc failed");

	while (1) {
		fgets(command, MAX_STRING, stdin);
		if (command[strlen(command) - 1] == '\n')
			command[strlen(command) - 1] = '\0';

		if (strncmp(command, "print", 5) == 0) {
			/* "print" + one space = 6 characters that I ignore
			to get to the index */
			index = atoi(command + 6);

			if (index < 0 || index >= number_of_sensors) {
				printf("Index not in range!\n");
				continue;
			}

			print_sensor(sensor[index], v_analysed[index]);
		} else if (strncmp(command, "analyze", 7) == 0) {
			/* "analyyze" + one space = 8 characters that I ignore
			to get to the index */
			index = atoi(command + 8);

			if (index < 0 || index >= number_of_sensors) {
				printf("Index not in range!\n");
				continue;
			}

			analyze_sensor(sensor[index], &v_analysed[index]);

		} else if(strcmp(command, "clear") == 0) {
			sensor = clear_sensor(sensor, &number_of_sensors, &v_analysed);
		} else if (strcmp(command, "exit") == 0) {
			free(v_analysed);
			free_sensor(&sensor, number_of_sensors);
			free(command);
			break;
		}
	}
	return 0;
}
