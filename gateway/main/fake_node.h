#ifndef __FAKE_NODE_H
#define __FAKE_NODE_H

#include "esp_system.h" // esp_random()
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"

typedef struct
{
    const char *dev_name;
    uint16_t device_id;
    double base_lat;
    double base_lon;
    uint8_t buzzer;
} fake_node_t;

static fake_node_t fake_nodes[] = {
    {"NODE_1", 0x1001, 10.762622, 106.660172, 0},
    {"NODE_2", 0x1002, 10.763000, 106.660500, 1},
    {"NODE_3", 0x1003, 10.761900, 106.659800, 0},
    {"NODE_4", 0x1004, 10.762200, 106.660900, 0},
    {"NODE_5", 0x1005, 10.762900, 106.661200, 1}};

static const size_t NUM_NODES = sizeof(fake_nodes) / sizeof(fake_nodes[0]);
void make_and_send_fake_for_node(const fake_node_t *n);
#endif