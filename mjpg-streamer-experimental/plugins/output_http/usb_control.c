/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2020 Linus K.                                             #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include "usb_control.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <wiringPi.h>


static pthread_t auto_disabler_thread;
static pthread_mutex_t mutex_global;
static unsigned long lastEnabled;
static int is_shutting_down;


static unsigned long getMillis() {
    struct timespec spec;
    //clock_gettime(CLOCK_REALTIME, &spec);
    clock_gettime(CLOCK_MONOTONIC, &spec); // Uptime
    time_t s;
    long ms;

    s = spec.tv_sec;
    ms = spec.tv_nsec / 1.0E6;
    if (ms > 999) { 
        s++;
        ms = 0;
    }

    unsigned long fullMs = s * 1000 + ms;
    return fullMs;
}

static void enableUsb() {
    pthread_mutex_lock(&mutex_global);
    digitalWrite(USB_SWITCH_PIN, 1);
    lastEnabled = getMillis();
    pthread_mutex_unlock(&mutex_global);
}

static void disableUsb() {
    pthread_mutex_lock(&mutex_global);
    digitalWrite(USB_SWITCH_PIN, 0);
    pthread_mutex_unlock(&mutex_global);
}

static int hasTimedOut() {
    unsigned long millis = getMillis();
    pthread_mutex_lock(&mutex_global);
    unsigned long sinceEnabled = millis - lastEnabled;
    pthread_mutex_unlock(&mutex_global);
    return sinceEnabled >= USB_SWITCH_TIMEOUT_MILLIS;
}

static int isShuttingDown() {
    pthread_mutex_lock(&mutex_global);
    int is_shutdown = is_shutting_down;
    pthread_mutex_unlock(&mutex_global);
    return is_shutdown;
}

static void shutdown() {
    pthread_mutex_lock(&mutex_global);
    is_shutting_down = 1;
    pthread_mutex_unlock(&mutex_global);
}

void* auto_disabler(void* _useless) {
    while(!isShuttingDown()) {
        usleep(1000*100);
        if(hasTimedOut())
            disableUsb();
    }
    return NULL;
}

void uc_init(void) {
    // WiringPi
    wiringPiSetup();
    pinMode(USB_SWITCH_PIN, OUTPUT);
    disableUsb();

    // Variables
    lastEnabled = 0;
    is_shutting_down = 0;

    // Mutex
    pthread_mutex_init(&mutex_global, NULL);


    // Auto-Disabler thread
    if(pthread_create(&auto_disabler_thread, NULL, auto_disabler, NULL) != 0) {
        fprintf(stderr, "FATAL: Failed to create auto_disabler_thread!!!\n");
        exit(1);
    }
    //pthread_detach(auto_disabler_thread);
}

void uc_stop(void) {
    //pthread_cancel(auto_disabler_thread);

    // Signal thread shutdown and wait for it
    shutdown();
    pthread_join(auto_disabler_thread, NULL);

    disableUsb();
}

void uc_webcam_used(void) {
    // Enable usb
    // (will get automatically disabled by
    // auto_disabler_thread after USB_SWITCH_TIMEOUT_MILLIS)
    enableUsb();
}