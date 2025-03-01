#ifndef GPIO_CTRL_H
#define GPIO_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

void gpio_init(int pin);
void gpio_set_value(int pin, int value);
void gpio_cleanup(int pin);

#ifdef __cplusplus
}
#endif

#endif // GPIO_CTRL_H