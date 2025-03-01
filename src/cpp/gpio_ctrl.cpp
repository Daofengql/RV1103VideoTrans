#include <stdio.h>
#include <string.h>
#include "gpio_ctrl.h"

void gpio_init(int pin) {
    FILE *export_file = fopen("/sys/class/gpio/export", "w");
    if (export_file) {
        fprintf(export_file, "%d", pin);
        fclose(export_file);
    }
    
    char direction_path[50];
    snprintf(direction_path, sizeof(direction_path), "/sys/class/gpio/gpio%d/direction", pin);
    FILE *direction_file = fopen(direction_path, "w");
    if (direction_file) {
        fprintf(direction_file, "out");
        fclose(direction_file);
    }
}

void gpio_set_value(int pin, int value) {
    char value_path[50];
    snprintf(value_path, sizeof(value_path), "/sys/class/gpio/gpio%d/value", pin);
    FILE *value_file = fopen(value_path, "w");
    if (value_file) {
        fprintf(value_file, "%d", value);
        fclose(value_file);
    }
}

void gpio_cleanup(int pin) {
    FILE *unexport_file = fopen("/sys/class/gpio/unexport", "w");
    if (unexport_file) {
        fprintf(unexport_file, "%d", pin);
        fclose(unexport_file);
    }
}